package internal

import (
	messageoption "github.com/luyuancpp/protooption"
	"pbgen/utils"
	"strings"
)

// / game server

// IsRoomNodeHostedProtocolHandler 判断是否是RoomNode节点对外提供的协议服务处理器
// （RoomNode作为服务端，处理外部调用的客户端协议接口）
func IsRoomNodeHostedProtocolHandler(methods *RPCMethods) bool {
	firstMethodInfo := (*methods)[0]
	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_ROOM) {
		return false
	}
	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

// IsRoomNodeHostedPlayerProtocolHandler 判断是否是RoomNode节点对外提供的玩家协议服务处理器
// （RoomNode作为服务端，处理外部调用的玩家相关客户端协议接口）
func IsRoomNodeHostedPlayerProtocolHandler(methods *RPCMethods) bool {
	firstMethodInfo := (*methods)[0]
	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_ROOM) {
		return false
	}

	if utils.HasGrpcService(firstMethodInfo.Path()) {
		return false
	}
	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

// IsNoOpHandler 空处理器判断（始终返回false）
func IsNoOpHandler(methods *RPCMethods) bool {
	return false
}

// IsRoomNodeReceivedPlayerResponseHandler 判断是否是RoomNode节点接收的玩家服务响应处理器
// （RoomNode作为客户端，处理外部服务返回的玩家相关响应）
func IsRoomNodeReceivedPlayerResponseHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_ROOM) {
		return false
	}

	if isClientProtocolService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	if utils.HasGrpcService(strings.ToLower(firstMethodInfo.Path())) {
		return false
	}

	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

// IsRoomNodeReceivedProtocolResponseHandler 判断是否是RoomNode节点接收的普通协议响应处理器
// （RoomNode作为客户端，处理外部服务返回的协议相关响应）
func IsRoomNodeReceivedProtocolResponseHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_ROOM) {
		return false
	}

	if isClientProtocolService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	if isClientProtocolService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	return true
}
