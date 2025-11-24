package internal

import (
	messageoption "github.com/luyuancpp/protooption"
)

// IsCentreHostedServiceHandler 判断是否是Centre节点对外提供的普通服务处理器
// （Centre作为服务端，处理外部调用）
func IsCentreHostedServiceHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	if isPlayerService(firstMethodInfo.ServiceDescriptorProto) {
		return false
	}

	return true
}

// IsCentreHostedPlayerServiceHandler 判断是否是Centre节点对外提供的玩家服务处理器
// （Centre作为服务端，处理外部调用的玩家相关接口）
func IsCentreHostedPlayerServiceHandler(methodList *RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]

	if !IsFileBelongToNode(firstMethodInfo.Fd, messageoption.NodeType_NODE_CENTRE) {
		return false
	}

	return isPlayerService(firstMethodInfo.ServiceDescriptorProto)
}

// IsCentreReceivedServiceResponseHandler 判断是否是Centre节点接收的普通服务响应处理器
// （Centre作为客户端，处理外部服务返回的响应）
func IsCentreReceivedServiceResponseHandler(methodList *RPCMethods) bool {
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

// IsCentreReceivedPlayerServiceResponseHandler 判断是否是Centre节点接收的玩家服务响应处理器
// （Centre作为客户端，处理外部服务返回的玩家相关响应）
func IsCentreReceivedPlayerServiceResponseHandler(methodList *RPCMethods) bool {
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
