package cpp

import (
	"path/filepath"
	"strings"
	"sync"

	"google.golang.org/protobuf/types/descriptorpb"

	"pbgen/internal"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger"

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
		logger.Global.Warn("未找到gate_command.proto，跳过Gate Kafka路由文件生成")
		return nil
	}

	gateEventFd := findProtoFileBySuffix("contracts.kafka", "gate_event.proto")
	if gateEventFd == nil {
		logger.Global.Warn("未找到gate_event.proto，跳过Gate Kafka路由文件生成")
		return nil
	}

	gateCommandMsg := findMessageByName(gateCommandFd, "GateCommand")
	if gateCommandMsg == nil {
		logger.Global.Warn("未找到GateCommand消息定义，跳过Gate Kafka路由文件生成")
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
		logger.Global.Warn("未找到GateCommand.CommandType枚举，跳过Gate Kafka路由文件生成")
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
				logger.Global.Warn("GateCommand与Event字段类型不一致，跳过自动赋值",
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
			logger.Global.Warn("Gate Kafka路由文件无可生成case，跳过输出")
			return
		}

		outputPath := filepath.Join(_config.Global.Paths.GateNodeEventHandlerDirectory, "gate_kafka_command_router.cpp")
		data := gateKafkaRouterTemplateData{Cases: cases}

		if err := utils2.RenderTemplateToFile("internal/template/gate_kafka_command_router.cpp.tmpl", outputPath, data); err != nil {
			logger.Global.Error("生成Gate Kafka路由文件失败",
				zap.String("output_file", outputPath),
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("生成Gate Kafka路由文件成功",
			zap.String("output_file", outputPath),
			zap.Int("case_count", len(cases)),
		)
	}()
}
