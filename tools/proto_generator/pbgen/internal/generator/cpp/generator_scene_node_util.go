package cpp

import (
	"pbgen/internal"
	"pbgen/internal/utils"
	"strings"

	messageoption "github.com/luyuancpp/protooption"
)

func isSceneOwnedMethod(m *internal.MethodInfo) bool {
	return internal.IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_SCENE) || strings.Contains(strings.ToLower(m.Path()), "/scene/")
}

// IsSceneNodeHostedProtocolHandler 判断是否是SceneNode节点对外提供的协议服务处理器
// （SceneNode作为服务端，处理外部调用的客户端协议接口）
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

// IsSceneNodeHostedPlayerProtocolHandler 判断是否是SceneNode节点对外提供的玩家协议服务处理器
// （SceneNode作为服务端，处理外部调用的玩家相关客户端协议接口）
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

// IsNoOpHandler 空处理器判断（始终返回false）
func IsNoOpHandler(methods *internal.RPCMethods) bool {
	return false
}

// IsSceneNodeReceivedPlayerResponseHandler 判断是否是SceneNode节点接收的玩家服务响应处理器
// （SceneNode作为客户端，处理外部服务返回的玩家相关响应）
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

// IsSceneNodeReceivedProtocolResponseHandler 判断是否是SceneNode节点接收的普通协议响应处理器
// （SceneNode作为客户端，处理外部服务返回的协议相关响应）
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
