package internal

import (
	messageoption "github.com/luyuancpp/protooption"
)

// 判断 methodList 第一个 method 是否满足给定条件函数
func checkFirstMethod(methodList *RPCMethods, conditions ...func(*MethodInfo) bool) bool {
	first := (*methodList)[0]
	for _, cond := range conditions {
		if !cond(first) {
			return cond(first)
		}
	}
	return true
}

// IsGateNodeHostedServiceHandler 判断是否是Gate节点对外提供的服务处理器
// （Gate作为服务端，处理外部调用）
func IsGateNodeHostedServiceHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList, func(m *MethodInfo) bool {
		return IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_GATE)
	})
}

// IsGateNodeReceivedResponseHandler 判断是否是Gate节点接收的服务响应处理器
// （Gate作为客户端，处理外部服务返回的响应）
func IsGateNodeReceivedResponseHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(info *MethodInfo) bool {
			return !(isClientProtocolService(info.ServiceDescriptorProto))
		},
		func(m *MethodInfo) bool {
			return !isPlayerService(m.ServiceDescriptorProto)
		},
		func(m *MethodInfo) bool {
			return !IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_GATE)
		},
	)
}
