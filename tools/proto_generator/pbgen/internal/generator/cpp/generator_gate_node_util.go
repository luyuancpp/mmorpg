package cpp

import (
	messageoption "github.com/luyuancpp/protooption"
	"pbgen/internal"
)

// 判断 methodList 第一个 method 是否满足给定条件函数
func checkFirstMethod(methodList *internal.RPCMethods, conditions ...func(*internal.MethodInfo) bool) bool {
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
func IsGateNodeHostedServiceHandler(methodList *internal.RPCMethods) bool {
	return checkFirstMethod(methodList, func(m *internal.MethodInfo) bool {
		return internal.IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_GATE)
	})
}

// IsGateNodeReceivedResponseHandler 判断是否是Gate节点接收的服务响应处理器
// （Gate作为客户端，处理外部服务返回的响应）
func IsGateNodeReceivedResponseHandler(methodList *internal.RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *internal.MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(info *internal.MethodInfo) bool {
			return !(internal.IsClientProtocolService(info.ServiceDescriptorProto))
		},
		func(m *internal.MethodInfo) bool {
			return !internal.IsPlayerService(m.ServiceDescriptorProto)
		},
		func(m *internal.MethodInfo) bool {
			return !internal.IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_GATE)
		},
	)
}
