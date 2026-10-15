package cpp

import (
	"bytes"
	"path/filepath"
	"strings"
	"testing"
	"text/template"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"

	"protogen/internal"
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
		strings.Contains(out.String(), "tlsEcs.dispatcher.trigger(event)") {
		t.Fatal("空白名单输出必须保留入口拒绝,且不再含事件触发分支")
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
