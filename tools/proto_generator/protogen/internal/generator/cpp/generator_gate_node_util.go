package cpp

import (
	"protogen/internal"

	messageoption "github.com/luyuancpp/protooption"
)
func checkFirstMethod(methodList *internal.RPCMethods, conditions ...func(*internal.MethodInfo) bool) bool {
	first := (*methodList)[0]
	for _, cond := range conditions {
		if !cond(first) {
			return cond(first)
		}
	}
	return true
}

// IsGateNodeHostedServiceHandler checks if the handler serves external requests on the Gate node.
// (Gate acts as server, handling incoming calls.)
func IsGateNodeHostedServiceHandler(methodList *internal.RPCMethods) bool {
	return checkFirstMethod(methodList, func(m *internal.MethodInfo) bool {
		return internal.IsFileBelongToNode(m.Fd, messageoption.NodeType_NODE_GATE)
	})
}

// IsGateNodeReceivedResponseHandler checks if the handler processes service responses received by the Gate node.
// (Gate acts as client, handling responses from external services.)
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
