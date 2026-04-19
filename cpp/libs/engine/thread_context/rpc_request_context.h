#pragma once

#include <memory>
#include <string>
#include "proto/common/base/message.pb.h"
#include "core/type_define/type_define.h"
#include <muduo/net/TcpConnection.h>

// Per-RPC-call transient state.  Reset at each RPC dispatch entry point.
//
// Merges the old MessageContext + RpcThreadContext::tls_current_conn.
//
// Lifetime: request-level (valid only during a single RPC handler invocation).
struct RpcRequestContext
{
    // --- Routing ---
    RoutingNodeInfo& GetRoutingNodeInfo() { return routeData_; }
    std::string& GetRouteMsgBody() { return routeMsgBody_; }

    void SetNextRouteNodeType(uint32_t nodeType) { nextRouteNodeType_ = nodeType; }
    uint32_t GetNextRouteNodeType() const { return nextRouteNodeType_; }

    void SetNextRouteNodeId(uint32_t nodeId) { nextRouteNodeId_ = nodeId; }
    uint32_t GetNextRouteNodeId() const { return nextRouteNodeId_; }

    void SetCurrentSessionId(SessionId sessionId) { currentSessionId_ = sessionId; }
    SessionId GetSessionId() const { return currentSessionId_; }

    // --- Current TCP connection (set by RPC dispatch layer) ---
    std::shared_ptr<muduo::net::TcpConnection> conn;

private:
    RoutingNodeInfo routeData_;
    std::string routeMsgBody_;
    uint32_t nextRouteNodeType_{ UINT32_MAX };
    uint32_t nextRouteNodeId_{ UINT32_MAX };
    SessionId currentSessionId_{ kInvalidSessionId };
};

extern thread_local RpcRequestContext tlsRpc;
