package internal

import (
	messageoption "github.com/luyuancpp/protooption"
	"pbgen/internal/utils"
	"strings"
)

// IsRoomNodeHostedProtocolHandler 判断是否是RoomNode节点对外提供的协议服务处理器
// （RoomNode作为服务端，处理外部调用的客户端协议接口）
func IsRoomNodeHostedProtocolHandler(methods *RPCMethods) bool {
	return checkFirstMethod(methods,
		func(m *MethodInfo) bool {
			return IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_ROOM)
		},
		func(m *MethodInfo) bool {
			return !isPlayerService(m.ServiceDescriptorProto)
		},
		func(m *MethodInfo) bool {
			return !IsClientProtocolService(m.ServiceDescriptorProto)
		},
	)
}

// IsRoomNodeHostedPlayerProtocolHandler 判断是否是RoomNode节点对外提供的玩家协议服务处理器
// （RoomNode作为服务端，处理外部调用的玩家相关客户端协议接口）
func IsRoomNodeHostedPlayerProtocolHandler(methods *RPCMethods) bool {
	return checkFirstMethod(methods,
		func(m *MethodInfo) bool {
			return IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_ROOM)
		},
		func(m *MethodInfo) bool {
			return !utils.HasGrpcService(m.Path())
		},
		func(m *MethodInfo) bool {
			return isPlayerService(m.ServiceDescriptorProto)
		},
	)
}

// IsNoOpHandler 空处理器判断（始终返回false）
func IsNoOpHandler(methods *RPCMethods) bool {
	return false
}

// IsRoomNodeReceivedPlayerResponseHandler 判断是否是RoomNode节点接收的玩家服务响应处理器
// （RoomNode作为客户端，处理外部服务返回的玩家相关响应）
func IsRoomNodeReceivedPlayerResponseHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(m *MethodInfo) bool {
			return !IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_ROOM)
		},
		func(m *MethodInfo) bool {
			return !IsClientProtocolService(m.ServiceDescriptorProto)
		},
		func(m *MethodInfo) bool {
			return !utils.HasGrpcService(strings.ToLower(m.Path()))
		},
		func(m *MethodInfo) bool {
			return isPlayerService(m.ServiceDescriptorProto)
		},
	)
}

// IsRoomNodeReceivedProtocolResponseHandler 判断是否是RoomNode节点接收的普通协议响应处理器
// （RoomNode作为客户端，处理外部服务返回的协议相关响应）
func IsRoomNodeReceivedProtocolResponseHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(m *MethodInfo) bool {
			return !IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_ROOM)
		},
		func(m *MethodInfo) bool {
			return !IsClientProtocolService(m.ServiceDescriptorProto)
		},
		func(m *MethodInfo) bool {
			return !isPlayerService(m.ServiceDescriptorProto)
		},
	)
}
