package internal

import (
	messageoption "github.com/luyuancpp/protooption"
)

func IsCentreServiceMethodHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	if isPlayerService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	return true
}

func IsCentrePlayerServiceMethodHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	return isPlayerService(firstMethodInfo.ServiceDescriptorProto)
}

func IsCentreServiceResponseHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
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

func IsCentrePlayerServiceResponseHandler(methodList *RPCMethods) bool {
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
