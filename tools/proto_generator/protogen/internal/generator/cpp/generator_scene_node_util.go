package cpp

import (
	"protogen/internal"
	"protogen/internal/utils"
	"strings"

	messageoption "github.com/luyuancpp/protooption"
)

func isSceneOwnedMethod(m *internal.MethodInfo) bool {
	return internal.IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_SCENE) || strings.Contains(strings.ToLower(m.Path()), "/scene/")
}

// IsSceneNodeHostedProtocolHandler checks if the handler serves external protocol requests on the SceneNode.
// (SceneNode acts as server, handling incoming client protocol calls.)
func IsSceneNodeHostedProtocolHandler(methods *internal.RPCMethods) bool {
	return checkFirstMethod(methods,
		func(m *internal.MethodInfo) bool {
			return isSceneOwnedMethod(m)
		},
		func(m *internal.MethodInfo) bool {
			return !internal.IsPlayerService(m.ServiceDescriptorProto)
		},
		func(m *internal.MethodInfo) bool {
			return !internal.IsClientProtocolService(m.ServiceDescriptorProto)
		},
	)
}

// IsSceneNodeHostedPlayerProtocolHandler checks if the handler serves player protocol requests on the SceneNode.
// (SceneNode acts as server, handling incoming player-related client protocol calls.)
func IsSceneNodeHostedPlayerProtocolHandler(methods *internal.RPCMethods) bool {
	return checkFirstMethod(methods,
		func(m *internal.MethodInfo) bool {
			return isSceneOwnedMethod(m)
		},
		func(m *internal.MethodInfo) bool {
			return !utils.HasGrpcService(m.Path())
		},
		func(m *internal.MethodInfo) bool {
			return internal.IsPlayerService(m.ServiceDescriptorProto)
		},
	)
}

// IsNoOpHandler is a no-op handler check (always returns false).
func IsNoOpHandler(methods *internal.RPCMethods) bool {
	return false
}

// IsSceneNodeReceivedPlayerResponseHandler checks if the handler processes player service responses received by the SceneNode.
// (SceneNode acts as client, handling player-related responses from external services.)
func IsSceneNodeReceivedPlayerResponseHandler(methodList *internal.RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *internal.MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(m *internal.MethodInfo) bool {
			return !internal.IsClientProtocolService(m.ServiceDescriptorProto)
		},
		func(m *internal.MethodInfo) bool {
			return !utils.HasGrpcService(strings.ToLower(m.Path()))
		},
		func(m *internal.MethodInfo) bool {
			return internal.IsPlayerService(m.ServiceDescriptorProto)
		},
	)
}

// IsSceneNodeReceivedProtocolResponseHandler checks if the handler processes protocol responses received by the SceneNode.
// (SceneNode acts as client, handling protocol-related responses from external services.)
func IsSceneNodeReceivedProtocolResponseHandler(methodList *internal.RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *internal.MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(m *internal.MethodInfo) bool {
			return !isSceneOwnedMethod(m)
		},
		func(m *internal.MethodInfo) bool {
			return !internal.IsClientProtocolService(m.ServiceDescriptorProto)
		},
		func(m *internal.MethodInfo) bool {
			return !internal.IsPlayerService(m.ServiceDescriptorProto)
		},
	)
}

// IsSceneNodeGrpcHandler checks if the handler is a gRPC service targeting the Scene node.
// Matches proto files that:
//   - use gRPC (not cc_generic_services)
//   - belong to scene_manager domain (which has rpc.type: grpc)
//   - contain "scene_node" in the proto file name (convention for scene-targeted gRPC services)
func IsSceneNodeGrpcHandler(methods *internal.RPCMethods) bool {
	return checkFirstMethod(methods,
		func(m *internal.MethodInfo) bool {
			return !m.CcGenericServices()
		},
		func(m *internal.MethodInfo) bool {
			return utils.HasGrpcService(m.Path())
		},
		func(m *internal.MethodInfo) bool {
			return strings.Contains(strings.ToLower(m.FileBaseNameNoEx()), "scene_node")
		},
	)
}
