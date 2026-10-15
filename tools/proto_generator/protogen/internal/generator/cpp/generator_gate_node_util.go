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
