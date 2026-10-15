package cpp

import (
	"testing"

	"protogen/internal"

	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
)

func boolPtr(v bool) *bool {
	return &v
}

func strPtr(v string) *string {
	return &v
}

func buildMethod(path string, node messageoption.NodeType, ccGeneric bool, isPlayer bool, isClient bool) *internal.MethodInfo {
	fileOpts := &descriptorpb.FileOptions{CcGenericServices: boolPtr(ccGeneric)}
	proto.SetExtension(fileOpts, messageoption.E_OptionFileDefaultNode, node)

	svcOpts := &descriptorpb.ServiceOptions{}
	if isPlayer {
		proto.SetExtension(svcOpts, messageoption.E_OptionIsPlayerService, true)
	}
	if isClient {
		proto.SetExtension(svcOpts, messageoption.E_OptionIsClientProtocolService, true)
	}

	fd := &descriptorpb.FileDescriptorProto{
		Name:    strPtr(path),
		Options: fileOpts,
	}

	svc := &descriptorpb.ServiceDescriptorProto{
		Name:    strPtr("TestService"),
		Options: svcOpts,
	}

	return &internal.MethodInfo{
		ProtoFileInfo: internal.ProtoFileInfo{
			Fd:                     fd,
			ServiceDescriptorProto: svc,
		},
	}
}

func asMethods(m *internal.MethodInfo) internal.RPCMethods {
	return internal.RPCMethods{m}
}

func TestSceneNodeHostedProtocolHandler(t *testing.T) {
	sceneNormal := asMethods(buildMethod("proto/scene/scene.proto", messageoption.NodeType_NODE_SCENE, true, false, false))
	if !IsSceneNodeHostedProtocolHandler(&sceneNormal) {
		t.Fatalf("expected scene normal service to be hosted by scene node")
	}

	scenePlayer := asMethods(buildMethod("proto/scene/player_lifecycle.proto", messageoption.NodeType_NODE_SCENE, true, true, false))
	if IsSceneNodeHostedProtocolHandler(&scenePlayer) {
		t.Fatalf("did not expect scene player service in non-player hosted filter")
	}
}

// 节点间 RPC0 的回包已改为每次调用的 lambda(docs/design/muduo-rpc-channel.md §9.2),rpc_replies 只剩玩家回包链:
// 它只认非客户端协议的玩家服务,不认 Scene / Gate / SceneScene 这类节点级服务。
func TestSceneNodeReceivedPlayerResponseFilter(t *testing.T) {
	scenePlayer := asMethods(buildMethod("proto/scene/player_lifecycle.proto", messageoption.NodeType_NODE_SCENE, true, true, false))
	if !IsSceneNodeReceivedPlayerResponseHandler(&scenePlayer) {
		t.Fatalf("expected player service replies to stay in the player reply chain")
	}

	for _, methods := range []internal.RPCMethods{
		asMethods(buildMethod("proto/scene/scene.proto", messageoption.NodeType_NODE_SCENE, true, false, false)),
		asMethods(buildMethod("proto/scene/scene_admin.proto", messageoption.NodeType_NODE_SCENE, true, false, false)),
		asMethods(buildMethod("proto/gate/gate_service.proto", messageoption.NodeType_NODE_GATE, true, false, false)),
	} {
		if IsSceneNodeReceivedPlayerResponseHandler(&methods) {
			t.Fatalf("node-level service %s must not get a muduo reply handler", methods[0].FileName())
		}
	}

	clientPlayer := asMethods(buildMethod("proto/scene/player_bag.proto", messageoption.NodeType_NODE_SCENE, true, true, true))
	if IsSceneNodeReceivedPlayerResponseHandler(&clientPlayer) {
		t.Fatalf("client protocol player service must not get a reply handler")
	}
}
