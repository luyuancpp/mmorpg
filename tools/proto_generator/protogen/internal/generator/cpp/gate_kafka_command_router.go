package cpp

import (
	"path/filepath"
	"strings"
	"sync"

	"google.golang.org/protobuf/types/descriptorpb"

	"protogen/internal"
	_config "protogen/internal/config"
	utils2 "protogen/internal/utils"
	"protogen/logger"

	"go.uber.org/zap"
)

type gateKafkaRouterCase struct {
	EnumName         string
	EventMessageName string
	Assignments      []string
}

type gateKafkaRouterTemplateData struct {
	Cases []gateKafkaRouterCase
}

func findProtoFileBySuffix(pkgName string, nameSuffix string) *descriptorpb.FileDescriptorProto {
	for _, fd := range internal.FdSet.File {
		if fd.GetPackage() != pkgName {
			continue
		}
		if strings.HasSuffix(fd.GetName(), nameSuffix) {
			return fd
		}
	}
	return nil
}

func findMessageByName(fd *descriptorpb.FileDescriptorProto, messageName string) *descriptorpb.DescriptorProto {
	if fd == nil {
		return nil
	}
	for _, message := range fd.GetMessageType() {
		if message.GetName() == messageName {
			return message
		}
	}
	return nil
}

func collectFieldsByName(message *descriptorpb.DescriptorProto) map[string]*descriptorpb.FieldDescriptorProto {
	fields := make(map[string]*descriptorpb.FieldDescriptorProto)
	if message == nil {
		return fields
	}
	for _, field := range message.GetField() {
		fields[field.GetName()] = field
	}
	return fields
}

func resolveGateEventMessage(commandName string, eventMessageMap map[string]*descriptorpb.DescriptorProto) (string, *descriptorpb.DescriptorProto) {
	candidates := []string{
		commandName + "Event",
		"Player" + commandName + "Event",
	}

	for _, candidate := range candidates {
		if msg, ok := eventMessageMap[candidate]; ok {
			return candidate, msg
		}
	}

	return "", nil
}

func buildGateKafkaRouterCases() []gateKafkaRouterCase {
	gateCommandFd := findProtoFileBySuffix("contracts.kafka", "gate_command.proto")
	if gateCommandFd == nil {
		logger.Global.Warn("gate_command.proto not found, skipping Gate Kafka router file generation")
		return nil
	}

	gateEventFd := findProtoFileBySuffix("contracts.kafka", "gate_event.proto")
	if gateEventFd == nil {
		logger.Global.Warn("gate_event.proto not found, skipping Gate Kafka router file generation")
		return nil
	}

	gateCommandMsg := findMessageByName(gateCommandFd, "GateCommand")
	if gateCommandMsg == nil {
		logger.Global.Warn("GateCommand message definition not found, skipping Gate Kafka router file generation")
		return nil
	}

	var commandTypeEnum *descriptorpb.EnumDescriptorProto
	for _, enum := range gateCommandMsg.GetEnumType() {
		if enum.GetName() == "CommandType" {
			commandTypeEnum = enum
			break
		}
	}
	if commandTypeEnum == nil {
		logger.Global.Warn("GateCommand.CommandType enum not found, skipping Gate Kafka router file generation")
		return nil
	}

	commandFieldMap := collectFieldsByName(gateCommandMsg)

	eventMessageMap := make(map[string]*descriptorpb.DescriptorProto)
	for _, message := range gateEventFd.GetMessageType() {
		eventMessageMap[message.GetName()] = message
	}

	cases := make([]gateKafkaRouterCase, 0)
	for _, enumValue := range commandTypeEnum.GetValue() {
		commandName := enumValue.GetName()
		eventMessageName, eventMsg := resolveGateEventMessage(commandName, eventMessageMap)
		if eventMsg == nil {
			continue
		}

		caseData := gateKafkaRouterCase{
			EnumName:         commandName,
			EventMessageName: eventMessageName,
			Assignments:      make([]string, 0),
		}

		for _, eventField := range eventMsg.GetField() {
			commandField, exists := commandFieldMap[eventField.GetName()]
			if !exists {
				continue
			}
			if commandField.GetLabel() == descriptorpb.FieldDescriptorProto_LABEL_REPEATED ||
				eventField.GetLabel() == descriptorpb.FieldDescriptorProto_LABEL_REPEATED {
				continue
			}
			if commandField.GetType() != eventField.GetType() {
				logger.Global.Warn("GateCommand and Event field type mismatch, skipping auto-assignment",
					zap.String("command", commandName),
					zap.String("field", eventField.GetName()),
				)
				continue
			}

			fieldName := eventField.GetName()
			caseData.Assignments = append(caseData.Assignments,
				"event.set_"+fieldName+"(command."+fieldName+"());")
		}

		cases = append(cases, caseData)
	}

	return cases
}

func GenerateGateKafkaCommandRouter(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		cases := buildGateKafkaRouterCases()
		if len(cases) == 0 {
			logger.Global.Warn("No cases to generate for Gate Kafka router file, skipping output")
			return
		}

		outputPath := filepath.Join(_config.Global.Paths.GateNodeEventHandlerDirectory, "gate_kafka_command_router.cpp")
		data := gateKafkaRouterTemplateData{Cases: cases}

		if err := utils2.RenderTemplateToFile("internal/template/gate_kafka_command_router.cpp.tmpl", outputPath, data); err != nil {
			logger.Global.Error("Failed to generate Gate Kafka router file",
				zap.String("output_file", outputPath),
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("Gate Kafka router file generated successfully",
			zap.String("output_file", outputPath),
			zap.Int("case_count", len(cases)),
		)
	}()
}
