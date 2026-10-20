package internal

import (
	"math"
	"strings"
	"testing"

	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

func clientRpcRegressionService() (*RPCServiceInfo, *MethodInfo) {
	service := &descriptorpb.ServiceDescriptorProto{
		Name: proto.String("GuildService"), Options: &descriptorpb.ServiceOptions{},
	}
	proto.SetExtension(service.Options, messageoption.E_OptionIsClientProtocolService, true)
	method := &descriptorpb.MethodDescriptorProto{
		Name: proto.String("Call"), InputType: proto.String(".fixture.Request"), OutputType: proto.String(".fixture.Response"),
		Options: &descriptorpb.MethodOptions{},
	}
	service.Method = []*descriptorpb.MethodDescriptorProto{method}
	file := &descriptorpb.FileDescriptorProto{Name: proto.String("proto/test/client.proto"), Service: []*descriptorpb.ServiceDescriptorProto{service}}
	info := ProtoFileInfo{Fd: file, ServiceDescriptorProto: service}
	mi := &MethodInfo{ProtoFileInfo: info, MethodDescriptorProto: method, Id: 20}
	return &RPCServiceInfo{ProtoFileInfo: info, Methods: RPCMethods{mi}}, mi
}

func clientRpcRegressionSetKind(method *MethodInfo, value uint64) {
	method.MethodDescriptorProto.Options.ProtoReflect().SetUnknown(protowire.AppendVarint(
		protowire.AppendTag(nil, OptionClientRpcKindNumber, protowire.VarintType), value))
}

func TestClientRpcKindsRejectInvalidContractsWithMethodIdentity(t *testing.T) {
	for _, test := range []struct {
		name   string
		scene  bool
		mutate func(*RPCServiceInfo, *MethodInfo)
	}{
		{"Notify必须标推送", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			method.MethodDescriptorProto.Name = proto.String("NotifyChanged")
		}},
		{"推送必须有Notify前缀", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			clientRpcRegressionSetKind(method, uint64(ClientRpcPush))
			method.MethodDescriptorProto.OutputType = proto.String(".Empty")
		}},
		{"单向响应必须精确Empty", true, func(_ *RPCServiceInfo, method *MethodInfo) {
			clientRpcRegressionSetKind(method, uint64(ClientRpcOneWay))
			method.MethodDescriptorProto.OutputType = proto.String(".fixture.LoginEmptyResponse")
		}},
		{"grpc不得单向", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			clientRpcRegressionSetKind(method, uint64(ClientRpcOneWay))
			method.MethodDescriptorProto.OutputType = proto.String(".Empty")
		}},
		{"scene的Empty不能假装请求应答", true, func(_ *RPCServiceInfo, method *MethodInfo) {
			method.MethodDescriptorProto.OutputType = proto.String(".Empty")
		}},
		{"推送响应形态错误", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			clientRpcRegressionSetKind(method, uint64(ClientRpcPush))
			method.MethodDescriptorProto.Name = proto.String("NotifyChanged")
		}},
		{"非客户端服务不得标客户端形态", false, func(service *RPCServiceInfo, method *MethodInfo) {
			service.ServiceDescriptorProto.Options = nil
			clientRpcRegressionSetKind(method, uint64(ClientRpcRequest))
		}},
		{"枚举值越界", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			clientRpcRegressionSetKind(method, 4)
		}},
		{"选项wire类型错误", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			method.MethodDescriptorProto.Options.ProtoReflect().SetUnknown(protowire.AppendBytes(
				protowire.AppendTag(nil, OptionClientRpcKindNumber, protowire.BytesType), []byte("0")))
		}},
		{"选项字节损坏", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			method.MethodDescriptorProto.Options.ProtoReflect().SetUnknown(
				protowire.AppendTag(nil, OptionClientRpcKindNumber, protowire.VarintType))
		}},
		{"消息号尚未分配", false, func(_ *RPCServiceInfo, method *MethodInfo) {
			method.Id = math.MaxUint64
		}},
	} {
		t.Run(test.name, func(t *testing.T) {
			service, method := clientRpcRegressionService()
			test.mutate(service, method)
			classified, err := ClassifyClientRpcServices(RPCServiceInfoList{service}, func(*MethodInfo) bool { return test.scene })
			if err == nil || classified != nil {
				t.Fatalf("非法契约必须拒绝且不返回部分结果: classified=%v err=%v", classified, err)
			}
			identity := service.Service() + "." + method.Method()
			if !strings.Contains(err.Error(), identity) {
				t.Errorf("错误必须包含服务.方法 %s: %v", identity, err)
			}
		})
	}
}

func TestClientRpcKindsAcceptExactEmptyGrpcReplyAndZeroMessageID(t *testing.T) {
	service, method := clientRpcRegressionService()
	method.Id = 0
	method.MethodDescriptorProto.OutputType = proto.String(".Empty")
	classified, err := ClassifyClientRpcServices(RPCServiceInfoList{service}, func(*MethodInfo) bool { return false })
	if err != nil || len(classified) != 1 || len(classified[0].Methods) != 1 || classified[0].Methods[0].Kind != ClientRpcRequest {
		t.Fatalf("gRPC Empty 应答仍是请求应答，消息号0合法: classified=%v err=%v", classified, err)
	}
}

func TestClientRpcKindsDefaultAndRepeatedUnknownOption(t *testing.T) {
	method := &descriptorpb.MethodDescriptorProto{}
	actual, explicit, err := ReadClientRpcKind(method)
	if err != nil || actual != ClientRpcRequest || explicit {
		t.Fatalf("没有option应默认REQUEST: actual=%v explicit=%v err=%v", actual, explicit, err)
	}
	method.Options = &descriptorpb.MethodOptions{}
	wire := protowire.AppendVarint(protowire.AppendTag(nil, OptionClientRpcKindNumber, protowire.VarintType), uint64(ClientRpcPush))
	wire = protowire.AppendVarint(protowire.AppendTag(wire, OptionClientRpcKindNumber, protowire.VarintType), uint64(ClientRpcOneWay))
	method.Options.ProtoReflect().SetUnknown(wire)
	actual, explicit, err = ReadClientRpcKind(method)
	if err != nil || actual != ClientRpcOneWay || !explicit {
		t.Fatalf("重复单值option按protobuf最后值语义读取: actual=%v explicit=%v err=%v", actual, explicit, err)
	}
}
