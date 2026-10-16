package internal

import (
	"strings"
	"testing"

	_config "protogen/internal/config"

	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

// 方法 tag(docs/design/muduo-rpc-channel.md §10)只给节点间 RPC0 服务(cc_generic、非玩家、非客户端协议)生成;
// 其余文件的 <file>_service_metadata.h 必须与旧版逐字节相同。

func setServiceMetadataTestConfig(t *testing.T) {
	t.Helper()
	original := _config.Global
	t.Cleanup(func() {
		_config.Global = original
	})
	_config.Global = _config.Config{}
	_config.Global.FileExtensions.Proto = ".proto"
	_config.Global.FileExtensions.PbH = ".pb.h"
	_config.Global.Naming.MessageId = "MessageId"
}

type metadataTestMethod struct {
	name   string
	input  string
	output string
	id     uint64
	index  uint64
}

func newMetadataTestFile(name, packageName string, ccGeneric bool, messages ...*descriptorpb.DescriptorProto) *descriptorpb.FileDescriptorProto {
	file := &descriptorpb.FileDescriptorProto{Name: proto.String(name), MessageType: messages}
	if packageName != "" {
		file.Package = proto.String(packageName)
	}
	if ccGeneric {
		file.Options = &descriptorpb.FileOptions{CcGenericServices: proto.Bool(true)}
	}
	return file
}

func newMetadataTestMessage(name string, nested ...*descriptorpb.DescriptorProto) *descriptorpb.DescriptorProto {
	return &descriptorpb.DescriptorProto{Name: proto.String(name), NestedType: nested}
}

func newMetadataTestService(file *descriptorpb.FileDescriptorProto, serviceName string, options *descriptorpb.ServiceOptions, methods ...metadataTestMethod) *RPCServiceInfo {
	service := &descriptorpb.ServiceDescriptorProto{Name: proto.String(serviceName), Options: options}
	info := &RPCServiceInfo{ProtoFileInfo: ProtoFileInfo{Fd: file, ServiceDescriptorProto: service}}
	for _, method := range methods {
		descriptor := &descriptorpb.MethodDescriptorProto{
			Name:       proto.String(method.name),
			InputType:  proto.String(method.input),
			OutputType: proto.String(method.output),
		}
		info.Methods = append(info.Methods, &MethodInfo{
			ProtoFileInfo:         info.ProtoFileInfo,
			Id:                    method.id,
			Index:                 method.index,
			MethodDescriptorProto: descriptor,
		})
	}
	return info
}

func playerServiceOptions(clientProtocol bool) *descriptorpb.ServiceOptions {
	options := &descriptorpb.ServiceOptions{}
	proto.SetExtension(options, messageoption.E_OptionIsPlayerService, true)
	if clientProtocol {
		proto.SetExtension(options, messageoption.E_OptionIsClientProtocolService, true)
	}
	return options
}

// 公共消息所在的无 package 文件(empty.proto / message.proto 的子集)。
func commonMetadataTestFiles() []*descriptorpb.FileDescriptorProto {
	return []*descriptorpb.FileDescriptorProto{
		newMetadataTestFile("proto/common/base/empty.proto", "", false, newMetadataTestMessage("Empty")),
		newMetadataTestFile("proto/common/base/message.proto", "", false,
			newMetadataTestMessage("NodeHandshakeRequest"), newMetadataTestMessage("NodeHandshakeResponse")),
	}
}

func TestServiceMetadataHeader_SceneProtoEmitsMethodTags(t *testing.T) {
	setServiceMetadataTestConfig(t)

	sceneFile := newMetadataTestFile("proto/scene/scene.proto", "", true,
		newMetadataTestMessage("PlayerEnterGameNodeRequest"),
		newMetadataTestMessage("ProcessClientPlayerMessageRequest"),
		newMetadataTestMessage("ProcessClientPlayerMessageResponse"))
	files := append(commonMetadataTestFiles(), sceneFile)
	// scene.proto 里 Scene 服务的方法子集,消息号与下标取自现行生成物。
	scene := newMetadataTestService(sceneFile, "Scene", nil,
		metadataTestMethod{name: "PlayerEnterGameNode", input: ".PlayerEnterGameNodeRequest", output: ".Empty", id: 83, index: 0},
		metadataTestMethod{name: "ProcessClientPlayerMessage", input: ".ProcessClientPlayerMessageRequest", output: ".ProcessClientPlayerMessageResponse", id: 10, index: 2},
		metadataTestMethod{name: "NodeHandshake", input: ".NodeHandshakeRequest", output: ".NodeHandshakeResponse", id: 62, index: 11},
	)

	got, err := renderServiceMetadataHeader([]*RPCServiceInfo{scene}, files)
	if err != nil {
		t.Fatalf("render failed: %v", err)
	}

	const want = `#pragma once
#include <cstdint>

#include "proto/scene/scene.pb.h"

constexpr uint32_t ScenePlayerEnterGameNodeMessageId = 83;
constexpr uint32_t ScenePlayerEnterGameNodeIndex = 0;
#define ScenePlayerEnterGameNodeMethod  ::Scene_Stub::descriptor()->method(0)

constexpr uint32_t SceneProcessClientPlayerMessageMessageId = 10;
constexpr uint32_t SceneProcessClientPlayerMessageIndex = 2;
#define SceneProcessClientPlayerMessageMethod  ::Scene_Stub::descriptor()->method(2)

constexpr uint32_t SceneNodeHandshakeMessageId = 62;
constexpr uint32_t SceneNodeHandshakeIndex = 11;
#define SceneNodeHandshakeMethod  ::Scene_Stub::descriptor()->method(11)

// 节点间 RPC0 的方法 tag(docs/design/muduo-rpc-channel.md §10):RpcChannel / RpcClient / RpcSession 的
// CallMethod<Tag> / SendRequest<Tag> 按它取消息号与请求 / 响应类型。只含类型信息、不依赖 core;
// 请求 / 响应类型在此前向声明,tag 本身不要求完整类型。
class Empty;
class NodeHandshakeRequest;
class NodeHandshakeResponse;
class PlayerEnterGameNodeRequest;
class ProcessClientPlayerMessageRequest;
class ProcessClientPlayerMessageResponse;

struct ScenePlayerEnterGameNodeRpc
{
    static constexpr uint32_t kMessageId = ScenePlayerEnterGameNodeMessageId;
    using Request = ::PlayerEnterGameNodeRequest;
    using Response = ::Empty;
};

struct SceneProcessClientPlayerMessageRpc
{
    static constexpr uint32_t kMessageId = SceneProcessClientPlayerMessageMessageId;
    using Request = ::ProcessClientPlayerMessageRequest;
    using Response = ::ProcessClientPlayerMessageResponse;
};

struct SceneNodeHandshakeRpc
{
    static constexpr uint32_t kMessageId = SceneNodeHandshakeMessageId;
    using Request = ::NodeHandshakeRequest;
    using Response = ::NodeHandshakeResponse;
};
`
	if got != want {
		t.Fatalf("scene metadata header mismatch\n--- got ---\n%s\n--- want ---\n%s", got, want)
	}

	// 生成头保持轻量:不新增 include(唯一的 include 是原有的服务 .pb.h)。
	if count := strings.Count(got, "#include"); count != 2 {
		t.Fatalf("metadata header must keep only <cstdint> and the service .pb.h include, got %d includes", count)
	}
}

// gRPC 文件(非 cc_generic_services)不产出 tag,输出与旧版逐字节相同。
// 描述符集合传 nil:若生成器试图解析类型就会报错,从而证明 gRPC 文件根本不走 tag 路径。
func TestServiceMetadataHeader_GrpcFileEmitsNoMethodTags(t *testing.T) {
	setServiceMetadataTestConfig(t)

	grpcFile := newMetadataTestFile("proto/scene_manager/scene_node_service.proto", "scene_node", false,
		newMetadataTestMessage("ReleasePlayerRequest"))
	service := newMetadataTestService(grpcFile, "SceneNodeGrpc", nil,
		metadataTestMethod{name: "CreateScene", input: ".CreateSceneRequest", output: ".CreateSceneResponse", id: 122, index: 0},
		metadataTestMethod{name: "ReleasePlayer", input: ".scene_node.ReleasePlayerRequest", output: ".Empty", id: 128, index: 2},
	)

	got, err := renderServiceMetadataHeader([]*RPCServiceInfo{service}, nil)
	if err != nil {
		t.Fatalf("render failed: %v", err)
	}

	const want = `#pragma once
#include <cstdint>

#include "proto/scene_manager/scene_node_service.pb.h"

constexpr uint32_t SceneNodeGrpcCreateSceneMessageId = 122;
constexpr uint32_t SceneNodeGrpcCreateSceneIndex = 0;
#define SceneNodeGrpcCreateSceneMethod  ::SceneNodeGrpc_Stub::descriptor()->method(0)

constexpr uint32_t SceneNodeGrpcReleasePlayerMessageId = 128;
constexpr uint32_t SceneNodeGrpcReleasePlayerIndex = 2;
#define SceneNodeGrpcReleasePlayerMethod  ::SceneNodeGrpc_Stub::descriptor()->method(2)
`
	if got != want {
		t.Fatalf("gRPC metadata header changed\n--- got ---\n%s\n--- want ---\n%s", got, want)
	}
}

// 玩家服务(含客户端协议玩家服务)不产出 tag:它们只作为内层载荷派发,不由 RpcChannel 直接调用。
func TestServiceMetadataHeader_PlayerServiceFilesEmitNoMethodTags(t *testing.T) {
	setServiceMetadataTestConfig(t)

	cases := []struct {
		name    string
		file    *descriptorpb.FileDescriptorProto
		service string
		options *descriptorpb.ServiceOptions
	}{
		{"player service", newMetadataTestFile("proto/scene/player_lifecycle.proto", "", true), "ScenePlayer", playerServiceOptions(false)},
		{"client protocol player service", newMetadataTestFile("proto/scene/player_bag.proto", "", true), "SceneBagClientPlayer", playerServiceOptions(true)},
	}
	for _, tc := range cases {
		service := newMetadataTestService(tc.file, tc.service, tc.options,
			metadataTestMethod{name: "Foo", input: ".FooRequest", output: ".Empty", id: 7, index: 0})

		got, err := renderServiceMetadataHeader([]*RPCServiceInfo{service}, nil)
		if err != nil {
			t.Fatalf("%s: render failed: %v", tc.name, err)
		}
		if strings.Contains(got, "kMessageId") || strings.Contains(got, "struct ") || strings.Contains(got, "class ") {
			t.Fatalf("%s: must not emit method tags or forward declarations:\n%s", tc.name, got)
		}
		if !strings.Contains(got, "constexpr uint32_t "+tc.service+"FooMessageId = 7;") {
			t.Fatalf("%s: message id constant missing:\n%s", tc.name, got)
		}
	}
}

// 带 package 的类型在对应命名空间里前向声明;嵌套消息按 protoc 规则用 Outer_Inner(只有它能在命名空间作用域前向声明)。
func TestServiceMetadataHeader_PackagedAndNestedTypes(t *testing.T) {
	setServiceMetadataTestConfig(t)

	demoFile := newMetadataTestFile("proto/demo/demo_node.proto", "demo.rpc", true,
		newMetadataTestMessage("PingRequest"),
		newMetadataTestMessage("Outer", newMetadataTestMessage("Reply")))
	files := append(commonMetadataTestFiles(), demoFile)
	service := newMetadataTestService(demoFile, "DemoNode", nil,
		metadataTestMethod{name: "Ping", input: ".demo.rpc.PingRequest", output: ".demo.rpc.Outer.Reply", id: 1, index: 0},
		metadataTestMethod{name: "Notify", input: ".demo.rpc.PingRequest", output: ".Empty", id: 2, index: 1},
	)

	got, err := renderServiceMetadataHeader([]*RPCServiceInfo{service}, files)
	if err != nil {
		t.Fatalf("render failed: %v", err)
	}

	for _, fragment := range []string{
		"class Empty;\nnamespace demo::rpc\n{\nclass Outer_Reply;\nclass PingRequest;\n}\n\nstruct DemoNodePingRpc\n",
		"    static constexpr uint32_t kMessageId = DemoNodePingMessageId;\n    using Request = ::demo::rpc::PingRequest;\n    using Response = ::demo::rpc::Outer_Reply;\n",
		"struct DemoNodeNotifyRpc\n{\n    static constexpr uint32_t kMessageId = DemoNodeNotifyMessageId;\n    using Request = ::demo::rpc::PingRequest;\n    using Response = ::Empty;\n};\n",
	} {
		if !strings.Contains(got, fragment) {
			t.Fatalf("missing fragment %q in:\n%s", fragment, got)
		}
	}
	if strings.Count(got, "class PingRequest;") != 1 {
		t.Fatalf("forward declarations must be deduplicated:\n%s", got)
	}
}

// 描述符集合里找不到类型时报错,不猜一个可能编不过的名字。
func TestServiceMetadataHeader_UnknownTypeFails(t *testing.T) {
	setServiceMetadataTestConfig(t)

	sceneFile := newMetadataTestFile("proto/scene/scene.proto", "", true)
	service := newMetadataTestService(sceneFile, "Scene", nil,
		metadataTestMethod{name: "NodeHandshake", input: ".NodeHandshakeRequest", output: ".MissingResponse", id: 62, index: 11})

	_, err := renderServiceMetadataHeader([]*RPCServiceInfo{service}, commonMetadataTestFiles())
	if err == nil {
		t.Fatal("expected an error for a response type missing from the descriptor set")
	}
	if !strings.Contains(err.Error(), "SceneNodeHandshake") || !strings.Contains(err.Error(), ".MissingResponse") {
		t.Fatalf("error should name the method and the missing type: %v", err)
	}
}
