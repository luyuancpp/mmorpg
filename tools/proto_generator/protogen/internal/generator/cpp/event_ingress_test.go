package cpp

import (
	"testing"

	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

func ingressTestMessage(allowed bool) *descriptorpb.DescriptorProto {
	opts := &descriptorpb.MessageOptions{}
	value := uint64(0)
	if allowed {
		value = 1
	}
	opts.ProtoReflect().SetUnknown(protowire.AppendVarint(protowire.AppendTag(nil, 700001, protowire.VarintType), value))
	return &descriptorpb.DescriptorProto{Name: proto.String("AllowedEvent"), Options: opts}
}

func TestClassifyEventIngress(t *testing.T) {
	for _, tc := range []struct {
		name, path string
		marked     bool
		want       eventIngress
		bad        bool
	}{
		{"默认拒绝", "common/event/player_event.proto", false, eventIngressNone, false},
		{"场景入口", "proto/common/event/battle_event.proto", true, eventIngressScene, false},
		{"网关入口", "contracts/kafka/gate_event.proto", true, eventIngressGate, false},
		{"命令文件不能作为事件", "contracts/kafka/gate_command.proto", true, eventIngressNone, true},
		{"其他目录拒绝", "scene/player.proto", true, eventIngressNone, true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			got, err := classifyEventIngress(tc.path, ingressTestMessage(tc.marked), "common/event/", "contracts/kafka/")
			if (err != nil) != tc.bad || got != tc.want {
				t.Fatalf("got=%v err=%v want=%v bad=%v", got, err, tc.want, tc.bad)
			}
		})
	}
}

func TestKafkaIngressDefaultsToClosedWithoutOption(t *testing.T) {
	message := &descriptorpb.DescriptorProto{Name: proto.String("InternalEvent")}
	got, err := classifyEventIngress("common/event/internal_event.proto", message, "common/event/", "contracts/kafka/")
	if err != nil || got != eventIngressNone {
		t.Fatalf("未标注消息必须默认拒绝: got=%v err=%v", got, err)
	}
}

func TestKafkaIngressRejectsEntityHandleFields(t *testing.T) {
	for _, field := range []string{"entity", "actor_entity", "scene_entity"} {
		message := ingressTestMessage(true)
		message.Field = []*descriptorpb.FieldDescriptorProto{{Name: proto.String(field)}}
		if _, err := classifyEventIngress("common/event/battle_event.proto", message, "common/event/", "contracts/kafka/"); err == nil {
			t.Fatalf("字段 %s 被错误放行", field)
		}
	}
}

func TestKafkaIngressRejectsMalformedOption(t *testing.T) {
	message := ingressTestMessage(true)
	message.Options.ProtoReflect().SetUnknown(protowire.AppendTag(nil, 700001, protowire.BytesType))
	if _, err := kafkaIngressEnabled(message); err == nil {
		t.Fatal("损坏的标注被静默放行")
	}
}
