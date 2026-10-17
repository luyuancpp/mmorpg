package cpp

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"text/template"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
)

func installGateRouterDescriptors(t *testing.T, events ...*descriptorpb.DescriptorProto) {
	t.Helper()
	previous := internal.FdSet
	t.Cleanup(func() { internal.FdSet = previous })
	internal.FdSet = &descriptorpb.FileDescriptorSet{File: []*descriptorpb.FileDescriptorProto{
		{Name: proto.String("proto/contracts/kafka/gate_command.proto"), Package: proto.String("contracts.kafka"),
			MessageType: []*descriptorpb.DescriptorProto{{Name: proto.String("GateCommand")}}},
		{Name: proto.String("proto/contracts/kafka/gate_event.proto"), Package: proto.String("contracts.kafka"), MessageType: events},
	}}
}

func TestKafkaEventHandlersOnlyEmitIngressMessages(t *testing.T) {
	previousConfig, previousLogger, previousEvents := _config.Global, logger.Global, globalProtoEventList
	t.Cleanup(func() {
		_config.Global, logger.Global, globalProtoEventList = previousConfig, previousLogger, previousEvents
	})
	_config.Global = _config.Config{}
	logger.Global = zap.NewNop()
	root := t.TempDir()
	_config.Global.Paths.ProtoDir = filepath.Join(root, "proto")
	_config.Global.DirectoryNames.ProtoDirName = "proto"
	_config.Global.FileExtensions.Proto = ".proto"
	_config.Global.FileExtensions.PbH = ".pb.h"
	_config.Global.FileExtensions.HandlerH = "_handler.h"
	_config.Global.FileExtensions.HandlerCpp = "_handler.cpp"
	_config.Global.Naming.HandlerFile = "Handler"
	_config.Global.Naming.YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
	_config.Global.Naming.YourCodeEnd = "///<<< END WRITING YOUR CODE"
	_config.Global.Naming.YourCodePair = _config.Global.Naming.YourCodeBegin + "\n" + _config.Global.Naming.YourCodeEnd
	protoDir := filepath.Join(_config.Global.Paths.ProtoDir, "common", "event")
	if err := os.MkdirAll(protoDir, 0755); err != nil {
		t.Fatal(err)
	}
	for name, source := range map[string]string{
		"sample_event.proto":   "message UnmarkedEvent {}\nmessage AllowedEvent {}\nmessage SecondEvent {}\n",
		"internal_event.proto": "message InternalEvent {}\n",
	} {
		if err := os.WriteFile(filepath.Join(protoDir, name), []byte(source), 0644); err != nil {
			t.Fatal(err)
		}
	}
	globalProtoEventList = []*ProtoEventInfo{
		{MessageName: "SecondEvent", ProtoInclude: "proto/common/event/sample_event.pb.h", Ingress: eventIngressScene},
		{MessageName: "AllowedEvent", ProtoInclude: "proto/common/event/other_event.pb.h", Ingress: eventIngressScene},
		{MessageName: "UnmarkedEvent", ProtoInclude: "proto/common/event/sample_event.pb.h", Ingress: eventIngressNone},
		{MessageName: "AllowedEvent", ProtoInclude: "proto/common/event/sample_event.pb.h", Ingress: eventIngressScene},
	}
	files, err := os.ReadDir(protoDir)
	if err != nil {
		t.Fatal(err)
	}
	t.Chdir(filepath.Join("..", "..", ".."))
	outputDir := filepath.Join(root, "handlers")
	var wg sync.WaitGroup
	for _, file := range files {
		wg.Add(1)
		generateEventHandlerFiles(&wg, file, "common/event", outputDir)
	}
	wg.Wait()
	header, err := os.ReadFile(filepath.Join(outputDir, "sample_event_handler.h"))
	if err != nil {
		t.Fatal(err)
	}
	text := string(header)
	if strings.Contains(text, "UnmarkedEventHandler") || !strings.Contains(text, "AllowedEventHandler") ||
		!strings.Contains(text, "SecondEventHandler") || strings.Index(text, "AllowedEventHandler") > strings.Index(text, "SecondEventHandler") {
		t.Fatalf("只生成入口事件,并保持 proto 顺序:\n%s", text)
	}
	for _, ext := range []string{".h", ".cpp"} {
		if _, err := os.Stat(filepath.Join(outputDir, "internal_event_handler"+ext)); !os.IsNotExist(err) {
			t.Fatalf("无入口的文件不得生成 handler: ext=%s err=%v", ext, err)
		}
	}
}

func TestGateKafkaRouterEmptyWhitelistStillRendersRejectingOutput(t *testing.T) {
	installGateRouterDescriptors(t, ingressTestMessage(false))
	cases, err := buildGateKafkaRouterCases()
	if err != nil || len(cases) != 0 {
		t.Fatalf("空白名单应为合法输入: cases=%v err=%v", cases, err)
	}
	tmpl, err := template.ParseFiles(filepath.Join("..", "..", "template", "gate_kafka_command_router.cpp.tmpl"))
	if err != nil {
		t.Fatal(err)
	}
	var out bytes.Buffer
	if err := tmpl.Execute(&out, gateKafkaRouterTemplateData{Cases: cases}); err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out.String(), "if (!IsGateCommandEvent(command.event_id()))") ||
		strings.Contains(out.String(), "Handler(event)") {
		t.Fatal("空白名单输出必须保留入口拒绝,且不再含事件触发分支")
	}
}

func TestKafkaIngressDispatchCallsHandlerDirectly(t *testing.T) {
	allowed := ingressTestMessage(true)
	allowed.Name = proto.String("AllowedEvent")
	installGateRouterDescriptors(t, allowed)
	cases, err := buildGateKafkaRouterCases()
	if err != nil {
		t.Fatal(err)
	}
	render := func(name string, data any) string {
		t.Helper()
		tmpl, err := template.ParseFiles(filepath.Join("..", "..", "template", name))
		if err != nil {
			t.Fatal(err)
		}
		var out bytes.Buffer
		if err := tmpl.Execute(&out, data); err != nil {
			t.Fatal(err)
		}
		return out.String()
	}
	gate := render("gate_kafka_command_router.cpp.tmpl", gateKafkaRouterTemplateData{Cases: cases})
	if !strings.Contains(gate, "GateEventHandler::AllowedEventHandler(event)") || strings.Contains(gate, "dispatcher.trigger") {
		t.Fatalf("Kafka 必需命令须直接调用 handler:\n%s", gate)
	}
	scene := render("scene_kafka_command_router.cpp.tmpl", map[string]any{
		"Includes": []string{"allowed_event_handler.h"},
		"Cases": []map[string]string{{"EventIdConstant": "AllowedEventEventId", "QualifiedName": "AllowedEvent",
			"ClassName": "GateEventHandler", "HandlerName": "AllowedEventHandler"}},
	})
	if !strings.Contains(scene, "GateEventHandler::AllowedEventHandler(event)") || strings.Contains(scene, "dispatcher.trigger") {
		t.Fatalf("scene Kafka 必需命令须直接调用 handler:\n%s", scene)
	}
	handler := render("event_handler.cpp.tmpl", EventTemplateData{ClassName: "GateEventHandler",
		EventMessages: []EventHandlerMessage{{Name: "AllowedEvent", HandlerName: "AllowedEventHandler", QualifiedName: "AllowedEvent"}}})
	if strings.Contains(handler, "sink<") || strings.Contains(handler, "::Register") || strings.Contains(handler, "::UnRegister") {
		t.Fatalf("入口 handler 不再订阅 dispatcher:\n%s", handler)
	}
}

func TestGateKafkaRouterMissingContractFailsInsteadOfKeepingStaleOutput(t *testing.T) {
	installGateRouterDescriptors(t)
	internal.FdSet.File = internal.FdSet.File[:1]
	if _, err := buildGateKafkaRouterCases(); err == nil {
		t.Fatal("缺少网关事件契约必须拒绝生成")
	}
}

func TestGateKafkaRouterOnlyEmitsMarkedEvents(t *testing.T) {
	allowed := ingressTestMessage(true)
	allowed.Name = proto.String("AllowedEvent")
	unmarked := ingressTestMessage(false)
	unmarked.Name = proto.String("UnmarkedEvent")
	installGateRouterDescriptors(t, allowed, unmarked)
	cases, err := buildGateKafkaRouterCases()
	if err != nil || len(cases) != 1 || cases[0].EventMessageName != "AllowedEvent" {
		t.Fatalf("仅标注事件可生成路由: cases=%v err=%v", cases, err)
	}
}
