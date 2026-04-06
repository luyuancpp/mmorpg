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

func TestSceneNodeReceivedFilters(t *testing.T) {
	sceneNormal := asMethods(buildMethod("proto/scene/scene.proto", messageoption.NodeType_NODE_SCENE, true, false, false))
	if IsSceneNodeReceivedProtocolResponseHandler(&sceneNormal) {
		t.Fatalf("did not expect scene node to treat own scene service as external response")
	}
}
