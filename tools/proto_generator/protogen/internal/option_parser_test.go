package internal

import (
	"testing"

	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

func TestIsClientProtocolService(t *testing.T) {
	svc := &descriptorpb.ServiceDescriptorProto{}
	if IsClientProtocolService(svc) {
		t.Fatalf("expected false when options are nil")
	}

	opts := &descriptorpb.ServiceOptions{}
	proto.SetExtension(opts, messageoption.E_OptionIsClientProtocolService, true)
	svc.Options = opts

	if !IsClientProtocolService(svc) {
		t.Fatalf("expected true when extension is set")
	}
}

func TestIsPlayerService(t *testing.T) {
	svc := &descriptorpb.ServiceDescriptorProto{Options: &descriptorpb.ServiceOptions{}}
	proto.SetExtension(svc.Options, messageoption.E_OptionIsPlayerService, true)

	if !IsPlayerService(svc) {
		t.Fatalf("expected true when player service option is set")
	}
}

func TestGetFileDefaultNodeAndBelongCheck(t *testing.T) {
	file := &descriptorpb.FileDescriptorProto{}
	if got := GetFileDefaultNode(file); got != messageoption.NodeType_NODE_UNSPECIFIED {
		t.Fatalf("expected unspecified for nil options, got %v", got)
	}

	node := messageoption.NodeType(1)
	file.Options = &descriptorpb.FileOptions{}
	proto.SetExtension(file.Options, messageoption.E_OptionFileDefaultNode, node)

	if got := GetFileDefaultNode(file); got != node {
		t.Fatalf("GetFileDefaultNode mismatch: got=%v want=%v", got, node)
	}
	if !IsFileBelongToNode(file, node) {
		t.Fatalf("expected file belong to target node")
	}
	if IsFileBelongToNode(file, node+1) {
		t.Fatalf("did not expect file belong to another node")
	}
}

func TestOptionTypeCompatibility(t *testing.T) {
	svc1 := &descriptorpb.ServiceDescriptorProto{Options: &descriptorpb.ServiceOptions{}}
	proto.SetExtension(svc1.Options, messageoption.E_OptionIsClientProtocolService, true)
	if !IsClientProtocolService(svc1) {
		t.Fatalf("expected true when client protocol option is bool")
	}

	svc2 := &descriptorpb.ServiceDescriptorProto{Options: &descriptorpb.ServiceOptions{}}
	proto.SetExtension(svc2.Options, messageoption.E_OptionIsPlayerService, true)
	if !IsPlayerService(svc2) {
		t.Fatalf("expected true when player service option is bool")
	}

	file := &descriptorpb.FileDescriptorProto{Options: &descriptorpb.FileOptions{}}
	proto.SetExtension(file.Options, messageoption.E_OptionFileDefaultNode, messageoption.NodeType_NODE_SCENE)
	if got := GetFileDefaultNode(file); got != messageoption.NodeType_NODE_SCENE {
		t.Fatalf("expected NODE_SCENE for enum extension, got %v", got)
	}
}
