package internal

import (
	messageoption "github.com/luyuancpp/protooption"
	"pbgen/utils"
	"strings"
)

// / game server
func IsRoomNodeMethodHandler(methods *RPCMethods) bool {
	firstMethodInfo := (*methods)[0]
	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_ROOM) {
		return false
	}
	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

func IsRoomNodePlayerHandler(methods *RPCMethods) bool {
	firstMethodInfo := (*methods)[0]
	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_ROOM) {
		return false
	}

	if utils.HasGrpcService(firstMethodInfo.Path()) {
		return false
	}
	return isClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

func ReturnNoHandler(methods *RPCMethods) bool {
	return false
}

func isRoomNodePlayerRepliedHandler(methodList *RPCMethods) bool {
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

func isRoomNodeMethodRepliedHandler(methodList *RPCMethods) bool {
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
