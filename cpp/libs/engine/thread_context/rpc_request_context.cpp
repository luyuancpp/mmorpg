#include "rpc_request_context.h"

thread_local RpcRequestContext tlsRpc;

void RpcRequestContext::Reset()
{
    routeData_.Clear();
    routeMsgBody_.clear();
    nextRouteNodeType_ = UINT32_MAX;
    nextRouteNodeId_ = UINT32_MAX;
    currentSessionId_ = kInvalidSessionId;
    conn.reset();
}
