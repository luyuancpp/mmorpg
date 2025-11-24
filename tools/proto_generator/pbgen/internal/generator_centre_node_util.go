package internal

import (
	messageoption "github.com/luyuancpp/protooption"
)

func isCentreMethodHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	if isPlayerService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	return true
}

func isCentrePlayerHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	return isPlayerService(firstMethodInfo.ServiceDescriptorProto)
}

func isCentreMethodRepliedHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if isPlayerService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	if isClientProtocolService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	return true
}

func isCentrePlayerRepliedHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !firstMethodInfo.CcGenericServices() {
		return false
	}

	if IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	if !isPlayerService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	if isClientProtocolService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	return true
}
