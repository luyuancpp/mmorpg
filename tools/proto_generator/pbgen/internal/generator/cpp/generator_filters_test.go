package cpp

import (
	"testing"

	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal"
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

	scenePlayer := asMethods(buildMethod("proto/scene/game_player.proto", messageoption.NodeType_NODE_SCENE, true, true, false))
	if IsSceneNodeHostedProtocolHandler(&scenePlayer) {
		t.Fatalf("did not expect scene player service in non-player hosted filter")
	}

	centreNormal := asMethods(buildMethod("proto/centre/centre_service.proto", messageoption.NodeType_NODE_CENTRE, true, false, false))
	if IsSceneNodeHostedProtocolHandler(&centreNormal) {
		t.Fatalf("did not expect centre service hosted by scene node")
	}
}

func TestSceneNodeReceivedFilters(t *testing.T) {
	centrePlayer := asMethods(buildMethod("proto/centre/centre_player.proto", messageoption.NodeType_NODE_CENTRE, true, true, false))
	if !IsSceneNodeReceivedPlayerResponseHandler(&centrePlayer) {
		t.Fatalf("expected scene node to receive centre player responses")
	}

	centreNormal := asMethods(buildMethod("proto/centre/centre_service.proto", messageoption.NodeType_NODE_CENTRE, true, false, false))
	if !IsSceneNodeReceivedProtocolResponseHandler(&centreNormal) {
		t.Fatalf("expected scene node to receive centre normal responses")
	}

	sceneNormal := asMethods(buildMethod("proto/scene/scene.proto", messageoption.NodeType_NODE_SCENE, true, false, false))
	if IsSceneNodeReceivedProtocolResponseHandler(&sceneNormal) {
		t.Fatalf("did not expect scene node to treat own scene service as external response")
	}
}

func TestCentreAndGateFilters(t *testing.T) {
	centreNormal := asMethods(buildMethod("proto/centre/centre_service.proto", messageoption.NodeType_NODE_CENTRE, true, false, false))
	if !IsCentreHostedServiceHandler(&centreNormal) {
		t.Fatalf("expected centre normal service to be hosted by centre")
	}

	centrePlayer := asMethods(buildMethod("proto/centre/centre_player.proto", messageoption.NodeType_NODE_CENTRE, true, true, false))
	if !IsCentreHostedPlayerServiceHandler(&centrePlayer) {
		t.Fatalf("expected centre player service to be hosted by centre player filter")
	}

	sceneNormal := asMethods(buildMethod("proto/scene/scene.proto", messageoption.NodeType_NODE_SCENE, true, false, false))
	if !IsCentreReceivedServiceResponseHandler(&sceneNormal) {
		t.Fatalf("expected centre to receive scene normal responses")
	}

	if !IsGateNodeReceivedResponseHandler(&sceneNormal) {
		t.Fatalf("expected gate to receive scene/centre normal responses")
	}

	gateNormal := asMethods(buildMethod("proto/gate/gate_service.proto", messageoption.NodeType_NODE_GATE, true, false, false))
	if IsGateNodeReceivedResponseHandler(&gateNormal) {
		t.Fatalf("did not expect gate to receive its own gate hosted service as external response")
	}
}
