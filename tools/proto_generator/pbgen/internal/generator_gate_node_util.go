package internal

import (
	messageoption "github.com/luyuancpp/protooption"
)

// 判断 methodList 是否为空
func isEmpty(methodList *RPCMethods) bool {
	return len(*methodList) == 0
}

// 判断 methodList 第一个 method 是否满足给定条件函数
func checkFirstMethod(methodList *RPCMethods, conditions ...func(*MethodInfo) bool) bool {
	if isEmpty(methodList) {
		return false
	}
	first := (*methodList)[0]
	for _, cond := range conditions {
		if !cond(first) {
			return false
		}
	}
	return true
}

// 特定条件判断
func isGateServiceHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList, func(m *MethodInfo) bool {
		return IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_GATE)
	})
}

func isGateMethodRepliedHandler(methodList *RPCMethods) bool {
	return checkFirstMethod(methodList,
		func(m *MethodInfo) bool {
			return m.CcGenericServices()
		},
		func(info *MethodInfo) bool {
			return !(isClientProtocolService(info.ServiceDescriptorProto))
		},
		func(m *MethodInfo) bool {
			return !IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_GATE)
		},
	)
}
