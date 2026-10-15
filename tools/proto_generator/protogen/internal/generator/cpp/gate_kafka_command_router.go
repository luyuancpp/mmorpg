package cpp

import (
	"fmt"
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
	EventIdConstant  string
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

func buildGateKafkaRouterCases() ([]gateKafkaRouterCase, error) {
	gateCommandFd := findProtoFileBySuffix("contracts.kafka", "gate_command.proto")
	if gateCommandFd == nil {
		return nil, fmt.Errorf("缺少 contracts.kafka gate_command.proto 描述符")
	}

	gateEventFd := findProtoFileBySuffix("contracts.kafka", "gate_event.proto")
	if gateEventFd == nil {
		return nil, fmt.Errorf("缺少 contracts.kafka gate_event.proto 描述符")
	}

	gateCommandMsg := findMessageByName(gateCommandFd, "GateCommand")
	if gateCommandMsg == nil {
		return nil, fmt.Errorf("缺少 GateCommand 消息描述符")
	}

	commandFieldMap := collectFieldsByName(gateCommandMsg)

	cases := make([]gateKafkaRouterCase, 0)
	for _, eventMsg := range gateEventFd.GetMessageType() {
		enabled, err := kafkaIngressEnabled(eventMsg)
		if err != nil {
			return nil, fmt.Errorf("网关事件 %s 入口标注损坏: %w", eventMsg.GetName(), err)
		}
		if !enabled {
			continue
		}
		eventMessageName := eventMsg.GetName()

		// Compute the C++ event_id constant name: e.g. "ContractsKafkaKickPlayerEventEventId"
		eventIdConstant := buildEventIdName("contracts.kafka", eventMessageName) + "EventId"

		caseData := gateKafkaRouterCase{
			EventIdConstant:  eventIdConstant,
			EventMessageName: eventMessageName,
			Assignments:      make([]string, 0),
		}

		// Auto-map fields: for each event field, if GateCommand has a field with
		// the same name and compatible type, generate an assignment.
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
					zap.String("event", eventMessageName),
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

	return cases, nil
}

func GenerateGateKafkaCommandRouter(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		cases, err := buildGateKafkaRouterCases()
		if err != nil {
			logger.Global.Fatal("生成网关 Kafka 路由失败", zap.Error(err))
		}

		// 撤销全部标注也必须覆写旧输出,使空白名单确实拒绝所有事件。
		outputPath := filepath.Join(_config.Global.Paths.GateNodeEventHandlerDirectory, "gate_kafka_command_router.cpp")
		data := gateKafkaRouterTemplateData{Cases: cases}

		if err := utils2.RenderTemplateToFile("internal/template/gate_kafka_command_router.cpp.tmpl", outputPath, data); err != nil {
			logger.Global.Fatal("写入网关 Kafka 路由失败",
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
