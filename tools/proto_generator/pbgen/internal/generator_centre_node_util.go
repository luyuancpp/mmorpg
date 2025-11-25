package internal

import (
	messageoption "github.com/luyuancpp/protooption"
)

// IsCentreHostedServiceHandler 判断是否是Centre节点对外提供的普通服务处理器
// （Centre作为服务端，处理外部调用）
func IsCentreHostedServiceHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_CENTRE)
		},
		func(m *MethodInfo) bool {
			return !isPlayerService(m.ServiceDescriptorProto)
		},
	)
}

// IsCentreHostedPlayerServiceHandler 判断是否是Centre节点对外提供的玩家服务处理器
// （Centre作为服务端，处理外部调用的玩家相关接口）
func IsCentreHostedPlayerServiceHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_CENTRE)
		},
		func(m *MethodInfo) bool {
			return isPlayerService(m.ServiceDescriptorProto)
		},
	)
}

// IsCentreReceivedServiceResponseHandler 判断是否是Centre节点接收的普通服务响应处理器
// （Centre作为客户端，处理外部服务返回的响应）
func IsCentreReceivedServiceResponseHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return !IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_CENTRE)
		},
		func(m *MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(m *MethodInfo) bool {
			return !isPlayerService(m.ServiceDescriptorProto)
		},
		func(m *MethodInfo) bool {
			return !IsClientProtocolService(m.ServiceDescriptorProto)
		},
	)
}

// IsCentreReceivedPlayerServiceResponseHandler 判断是否是Centre节点接收的玩家服务响应处理器
// （Centre作为客户端，处理外部服务返回的玩家相关响应）
func IsCentreReceivedPlayerServiceResponseHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(m *MethodInfo) bool {
			return !IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_CENTRE)
		},
		func(m *MethodInfo) bool {
			return isPlayerService(m.ServiceDescriptorProto)
		},
		func(m *MethodInfo) bool {
			return !IsClientProtocolService(m.ServiceDescriptorProto)
		},
	)
}
