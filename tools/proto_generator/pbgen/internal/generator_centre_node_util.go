package internal

import (
	messageoption "github.com/luyuancpp/protooption"
)

// IsCentreServiceMethodHandler 判断是否是Centre服务的普通方法处理器
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

// IsCentrePlayerServiceMethodHandler 判断是否是Centre的Player服务方法处理器
func IsCentrePlayerServiceMethodHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	return isPlayerService(firstMethodInfo.ServiceDescriptorProto)
}

// IsCentreServiceResponseHandler 判断是否是外部返回给Centre服务的响应处理器
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

// IsCentrePlayerServiceResponseHandler 判断是否是外部返回给Centre Player服务的响应处理器
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
