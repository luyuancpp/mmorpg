#include "rpc_reply_bridge.h"

#include <cstdint>
#include <string>
#include <unordered_map>

#include "muduo/base/Logging.h"

#include "client_session_handler.h"
#include "client_request_lane.h"
#include "gate_codec.h"
#include "login_admission.h"
#include "proto/common/base/message.pb.h"
#include "proto/common/base/session.pb.h"
#include "rpc/service_metadata/rpc_event_registry.h"
#include "session/manager/session_manager.h"
#include "table/proto/tip/common_error_tip.pb.h"

namespace gate_rpc_reply_bridge
{

namespace
{

// 响应 proto 全名 → message_id,用来把非 MessageContent 的 gRPC 回包包进 MessageContent 下发。
//
// 路由服 ClientRpcRouter.Forward 的响应类型就是 MessageContent(client-rpc-router.md D30),走原样下发分支,
// 不查这张表。表里因此会多一条 "MessageContent -> ClientRpcRouterForwardMessageId(176)",但这张表只在 reply
// **不是** MessageContent 时才查,这一条永远不会被命中;全表也只有 Forward 一条以 MessageContent 应答,
// 不会挤掉别的回包类型的映射。首次使用时从生成的注册表构建一次。
const std::unordered_map<std::string, uint32_t> &ResponseTypeToMessageId()
{
    static const std::unordered_map<std::string, uint32_t> table = []
    {
        std::unordered_map<std::string, uint32_t> built;
        for (uint32_t i = 0; i < kMaxRpcMethodCount; ++i)
        {
            const auto &meta = gRpcMethodRegistry[i];
            if (meta.responseProto)
            {
                built[std::string(meta.responseProto->GetDescriptor()->full_name())] = i;
            }
        }
        return built;
    }();
    return table;
}

} // namespace

void DeliverReply(const SessionDetails &sessionDetails, const ::google::protobuf::Message &reply)
{
    // 路由 lane 的调用(gate_call_seq != 0)由 lane 认领:核对会话代次与队头、恰好一次下发,
    // 再发同一会话的下一条(docs/design/client-request-ordering.md §4.5)。
    if (gate_lane::OnRouterReply(sessionDetails, reply))
    {
        return;
    }
    const auto it = tlsSessionManager.sessions().find(sessionDetails.session_id());
    if (it == tlsSessionManager.sessions().end())
    {
        return;
    }
    // 直连模式的 Login 回包:记下本连接已成功 Login(login_admission.h;路由模式在 lane 认领处记)。
    // 按回显的会话代次核对:session_id 被新连接复用之后,旧连接迟到的成功回包不得给新连接放行。
    if (reply.GetDescriptor() == loginpb::LoginResponse::descriptor() &&
        sessionDetails.session_incarnation() == it->second.incarnation &&
        gate_login_admission::IsLoginReplySuccess(reply))
    {
        it->second.loginSucceeded = true;
    }
    const auto clientConn = it->second.conn.lock();
    if (!clientConn)
    {
        return; // 会话记录还在但连接已释放(仅关机窗口可能):当断开处理
    }

    if (reply.GetDescriptor() == MessageContent::descriptor())
    {
        GetGateCodec().send(clientConn, reply);
        return;
    }

    const std::string typeName(reply.GetDescriptor()->full_name());
    const auto &table = ResponseTypeToMessageId();
    const auto typeIt = table.find(typeName);
    if (typeIt == table.end())
    {
        LOG_ERROR << "No message_id mapping for gRPC response type: " << typeName.c_str();
        return;
    }
    MessageContent wrapped;
    wrapped.set_serialized_message(reply.SerializeAsString());
    wrapped.set_message_id(typeIt->second);
    GetGateCodec().send(clientConn, wrapped);
}

void NotifyCallFailure(const char *method, int statusCode, const std::string &statusMessage,
                       const std::string *rawSessionDetails)
{
    if (rawSessionDetails == nullptr)
    {
        LOG_ERROR << "gRPC " << method << " failed: code=" << statusCode << " msg=" << statusMessage;
        return;
    }
    SessionDetails sessionDetails;
    if (!sessionDetails.ParseFromString(*rawSessionDetails))
    {
        LOG_ERROR << "gRPC " << method << " failed and its session metadata does not parse; client not notified";
        return;
    }
    static uint64_t failedCount = 0;
    if ((failedCount++ & 0x3FF) == 0)
    {
        LOG_WARN << "gRPC client call failed (sampled): method=" << method << " code=" << statusCode
                 << " msg=" << statusMessage << " latest_session_id=" << sessionDetails.session_id()
                 << " failed_total=" << failedCount;
    }
    // 路由 lane 的调用由 lane 认领:回带请求 id 的信封 1003(结果未知),再放行同一会话的下一条。
    if (gate_lane::OnRouterCallFailed(sessionDetails))
    {
        return;
    }
    const auto it = tlsSessionManager.sessions().find(sessionDetails.session_id());
    if (it == tlsSessionManager.sessions().end())
    {
        return;
    }
    const auto clientConn = it->second.conn.lock();
    if (!clientConn)
    {
        return;
    }
    GateClientSessionHandler::SendTipToClient(clientConn, kServiceUnavailable);
}

} // namespace gate_rpc_reply_bridge
