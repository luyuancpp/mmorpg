#include "client_message_processor.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <optional>

#include "gate_node.h"
#include "grpc/generator/proto/login/login_service_grpc.h"
#include "pbc/common_error_tip.pb.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/login_service_service_info.h"
#include "service_info/player_common_service_info.h"
#include "thread_local/storage_gate.h"
#include "util/random.h"
#include "proto/common/node.pb.h"

extern std::unordered_set<uint32_t> gClientToServerMessageId;

RpcClientSessionHandler::RpcClientSessionHandler(ProtobufCodec& codec,
    ProtobufDispatcher& dispatcher)
    : protobufCodec(codec),
    messageDispatcher(dispatcher)
{
    // 注册客户端请求消息回调
    messageDispatcher.registerMessageCallback<ClientRequest>(
        std::bind(&RpcClientSessionHandler::HandleRpcRequest, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

//todo 考虑中间一个login服务关了，原来的login服务器处理到一半，新的login处理不了
std::optional<entt::entity> FindLoginNodeForSession(uint64_t sessionId)
{
    const auto sessionIt = tls_gate.sessions().find(sessionId);
    if (sessionIt == tls_gate.sessions().end()) {
        LOG_ERROR << "Session not found for session id: " << sessionId;
        return std::nullopt;
    }

    auto& session = sessionIt->second;
    if (!session.HasLoginNodeId()) {
        const auto loginNodeIt = tls_gate.login_consistent_node().GetByHash(sessionId);
        if (tls_gate.login_consistent_node().end() == loginNodeIt) {
            LOG_ERROR << "Login server not found for session id: " << sessionId;
            return std::nullopt;
        }
        session.loginNodeId = entt::to_integral(loginNodeIt->second);
    }

    const auto loginNodeIt = tls_gate.login_consistent_node().GetNodeValue(session.loginNodeId);
    if (tls_gate.login_consistent_node().end() == loginNodeIt) {
        LOG_ERROR << "Login server crashed for session id: " << sessionId;
        session.loginNodeId = kInvalidNodeId;
        return std::nullopt;
    }
    return loginNodeIt->second;
}

void RpcClientSessionHandler::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    // todo如果我没登录就发送其他协议到controller game server 怎么办
    if (conn->connected())
    {
        HandleConnectionEstablished(conn);
    }
    else
    {
        HandleConnectionDisconnection(conn);
    }
}

void RpcClientSessionHandler::SendMessageToClient(const muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& message) const
{
    protobufCodec.send(conn, message);
}

Guid RpcClientSessionHandler::GetSessionId(const muduo::net::TcpConnectionPtr& conn)
{
    try {
        return boost::any_cast<Guid>(conn->getContext());
    }
    catch (const boost::bad_any_cast& e) {
        LOG_ERROR << "Failed to cast session ID from connection context: " << e.what();
        return kInvalidGuid;
    }
}

void RpcClientSessionHandler::SendTipToClient(const muduo::net::TcpConnectionPtr& conn, uint32_t tipId)
{
    TipInfoMessage tipMessage;
    tipMessage.set_id(tipId);
    MessageContent message;
    message.set_serialized_message(tipMessage.SerializeAsString());
    message.set_message_id(PlayerClientCommonServiceSendTipToClientMessageId);
    gGateNode->Codec().send(conn, message);

    LOG_ERROR << "Sent tip message to session id: " << GetSessionId(conn) << ", tip id: " << tipId;
}

bool RpcClientSessionHandler::CheckMessageSize(const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn) const {
    constexpr size_t maxByteSize = 1024;
    if (request->ByteSizeLong() > maxByteSize) {
        LOG_ERROR << "Message size exceeds 1KB. Message ID: " << request->message_id();
        MessageContent errResponse;
        errResponse.set_id(request->id());
        errResponse.set_message_id(request->message_id());
        errResponse.mutable_error_message()->set_id(kMessageSizeExceeded);
        conn->send(errResponse.SerializeAsString());
        return false;
    }
    return true;
}

bool RpcClientSessionHandler::CheckMessageLimit(Session& session, const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn) const {
    if (const auto err = session.messageLimiter.CanSend(request->message_id()); kSuccess != err) {
        LOG_ERROR << "Failed to send message. Message ID: " << request->message_id() << ", Error: " << err;
        MessageContent errResponse;
        errResponse.set_id(request->id());
        errResponse.set_message_id(request->message_id());
        errResponse.mutable_error_message()->set_id(err);
        conn->send(errResponse.SerializeAsString());
        return false;
    }
    return true;
}

template <typename Message, typename Request>
void SetSessionAndParseBody(Message& message, const Request& request, const uint64_t sessionId) {
    // 设置会话ID
    message.mutable_session_info()->set_session_id(sessionId);

    // 检查请求体是否有效
    const std::string& requestBody = request->body();
    if (requestBody.empty()) {
        return;
    }

    if (!message.mutable_client_msg_body()->ParseFromArray(requestBody.data(), requestBody.size())) {
        LOG_ERROR << "Failed to parse client message body for session id: " << sessionId;
        return; // 解析失败时，避免发送无效消息
    }
}

void RpcClientSessionHandler::HandleConnectionDisconnection(const muduo::net::TcpConnectionPtr& conn)
{
    // 重要: 此消息一定要发，不能只通过centre 的gate disconnect去发
    // 重要: 比如:登录还没到centre, gate的disconnect 先到centre，登录的消息后到centre,创建session，那么centre server 永远删除不了这个sessionid了
    // 重要 登录时候断开应该登录服务器也告诉centre

    const auto sessionId = entt::to_integral(GetSessionId(conn));

    // 处理登录服务器和中心服务器的断开通知
    const auto& loginNode = FindLoginNodeForSession(sessionId);
    if (loginNode)
    {
        loginpb::LoginNodeDisconnectRequest request;
        request.set_session_id(sessionId);
        SendLoginServiceDisconnect(tls.GetNodeRegistry(eNodeType::LoginNodeService), *loginNode , request);
    }

    // 通知中心服务器
    GateSessionDisconnectRequest request;
    request.mutable_session_info()->set_session_id(sessionId);
    gGateNode->GetZoneCentreNode().CallRemoteMethod(CentreServiceGateSessionDisconnectMessageId, request);

    // 删除会话
    tls_gate.sessions().erase(sessionId);

    LOG_TRACE << "Disconnected session id: " << sessionId;
}

void RpcClientSessionHandler::HandleConnectionEstablished(const muduo::net::TcpConnectionPtr& conn)
{
	auto sessionId = tls_gate.session_id_gen().Generate();
	while (tls_gate.sessions().contains(sessionId))
	{
		sessionId = tls_gate.session_id_gen().Generate();
	}

	// 用session id 防止改包把消息发给其他玩家

	conn->setContext(sessionId);
	Session session;
	session.conn = conn;
	tls_gate.sessions().emplace(sessionId, std::move(session));

	LOG_TRACE << "New connection, assigned session id: " << sessionId;
}

// Handle messages related to the game node
void HandleGameNodeMessage(const Session& session, const RpcClientMessagePtr& request, Guid sessionId, const muduo::net::TcpConnectionPtr& conn)
{
	// 玩家没登录直接发其他消息，乱发消息
    const entt::entity gameNodeId{ session.gameNodeId };
    if (!tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(gameNodeId))
    {
        LOG_ERROR << "Invalid game node id " << session.gameNodeId << " for session id: " << sessionId;
        RpcClientSessionHandler::SendTipToClient(conn, kServerCrashed);
        return;
    }

    auto& sceneNode = tls.GetNodeRegistry(eNodeType::SceneNodeService).get<RpcClient>(gameNodeId);
    ClientSendMessageToPlayerRequest message;
    message.mutable_message_content()->set_serialized_message(request->body());
    message.set_session_id(sessionId);
    message.mutable_message_content()->set_id(request->id());
    message.mutable_message_content()->set_message_id(request->message_id());
    sceneNode.CallRemoteMethod(GameServiceClientSendMessageToPlayerMessageId, message);

    LOG_TRACE << "Sent message to game node, session id: " << sessionId << ", message id: " << request->message_id();
}


void SendLoginRequestToLoginNode(entt::entity loginNode, Guid sessionId, const RpcClientMessagePtr& request)
{
	loginpb::LoginC2LRequest message;
	SetSessionAndParseBody(message, request, sessionId);
	SessionDetails sessionDetils;
	sessionDetils.set_session_id(sessionId);

    SendLoginServiceLogin(tls.GetNodeRegistry(eNodeType::LoginNodeService), loginNode, message, { "x-session-detail-bin" }, { sessionDetils.SerializeAsString() });


    LOG_TRACE << "Sent LoginC2LRequest, session id: " << sessionId;
}

void SendCreatePlayerRequestToLoginNode(entt::entity loginNode, Guid sessionId, const RpcClientMessagePtr& request)
{
    loginpb::CreatePlayerC2LRequest message;
    SetSessionAndParseBody(message, request, sessionId);
    SendLoginServiceCreatePlayer(tls.GetNodeRegistry(eNodeType::LoginNodeService), loginNode, message);

    LOG_TRACE << "Sent CreatePlayerC2LRequest, session id: " << sessionId;
}

void SendEnterGameRequestToLoginNode(entt::entity loginNode, Guid sessionId, const RpcClientMessagePtr& request)
{
    loginpb::EnterGameC2LRequest message;
    SetSessionAndParseBody(message, request, sessionId);
    SendLoginServiceEnterGame(tls.GetNodeRegistry(eNodeType::LoginNodeService), loginNode, message);

    LOG_TRACE << "Sent EnterGameC2LRequest, session id: " << sessionId;
}

void HandleLoginNodeMessage(Guid sessionId, const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn)
{
    auto loginNode = FindLoginNodeForSession(sessionId);
    if (!loginNode)
    {
        LOG_ERROR << "Login node not found for session id: " << sessionId << ", message id: " << request->message_id();
        // TODO: Handle connection closure logic here.
        return;
    }

    switch (request->message_id())
    {
    case LoginServiceLoginMessageId:
        SendLoginRequestToLoginNode(*loginNode, sessionId, request);
        break;
    case LoginServiceCreatePlayerMessageId:
        SendCreatePlayerRequestToLoginNode(*loginNode, sessionId, request);
        break;
    case LoginServiceEnterGameMessageId:
        SendEnterGameRequestToLoginNode(*loginNode, sessionId, request);
        break;
    default:
        LOG_ERROR << "Unhandled message id: " << request->message_id() << " for session id: " << sessionId;
        break;
    }
}

//// Main request handler, forwards the request to the appropriate service
void RpcClientSessionHandler::HandleRpcRequest(const muduo::net::TcpConnectionPtr& conn,
	const RpcClientMessagePtr& request,
	muduo::Timestamp)
{
	auto sessionId = GetSessionId(conn);
	const auto sessionIt = tls_gate.sessions().find(sessionId);
	if (sessionIt == tls_gate.sessions().end()) {
		LOG_ERROR << "Session not found for session id: " << sessionId << " in RPC request.";
		return;
	}
	auto& session = sessionIt->second;
	if (!CheckMessageSize(request, conn)) return;
	if (gClientToServerMessageId.contains(request->message_id())) {
		if (!CheckMessageLimit(session, request, conn)) return;
		HandleGameNodeMessage(session, request, sessionId, conn);
	}
	else {
		LOG_TRACE << "Session ID: " << sessionId << " - Handling login node message. Message ID: " << request->message_id();
		HandleLoginNodeMessage(sessionId, request, conn);
	}
}
