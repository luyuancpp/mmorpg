package internal

import (
	messageoption "github.com/luyuancpp/protooption"
	"pbgen/utils"
	"strings"
)

// / game server
func IsRoomNodeHostedProtocolHandler(methods *RPCMethods) bool {
	firstMethodInfo := (*methods)[0]
	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_ROOM) {
		return false
	}
	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

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

func IsNoOpHandler(methods *RPCMethods) bool {
	return false
}

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
