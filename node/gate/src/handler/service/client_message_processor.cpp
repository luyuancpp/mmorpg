#include "client_message_processor.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <optional>

#include "gate_node.h"
#include "grpc/generator/proto/login/login_service_grpc.h"
#include "pbc/common_error_tip.pb.h"
#include "service_info/service_info.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/login_service_service_info.h"
#include "service_info/game_client_player_service_info.h"
#include "thread_local/storage_gate.h"
#include "util/random.h"
#include "proto/common/node.pb.h"
#include "node/system/node_system.h"
#include "google/protobuf/descriptor.h"

constexpr char kSessionBinMetaKey[] = "x-session-detail-bin";

std::vector<std::string> SerializeSessionDetails(const SessionDetails& sessionDetails) {
	std::vector<std::string> result;
	std::string serialized;
	if (sessionDetails.SerializeToString(&serialized)) {
		result.push_back(std::move(serialized));
	}
	return result;
}

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
std::optional<entt::entity> FindNodeForSession(uint64_t sessionId, uint32_t nodeType)
{
    const auto sessionIt = tls_gate.sessions().find(sessionId);
    if (sessionIt == tls_gate.sessions().end()) {
        LOG_ERROR << "Session not found for session id: " << sessionId;
        return std::nullopt;
    }

    auto& session = sessionIt->second;
    if (!session.HasNodeId(nodeType)) {
        const auto loginNodeIt = tls.GetConsistentNode(nodeType).GetByHash(sessionId);
        if (tls.GetConsistentNode(nodeType).end() == loginNodeIt) {
            LOG_ERROR << "Login server not found for session id: " << sessionId;
            return std::nullopt;
        }
        session.SetNodeId(nodeType, entt::to_integral(loginNodeIt->second));
    }

    const auto loginNodeIt = tls.GetConsistentNode(nodeType).GetNodeValue(session.GetNodeId(nodeType));
    if (tls.GetConsistentNode(nodeType).end() == loginNodeIt) {
        LOG_ERROR << "Login server crashed for session id: " << sessionId;
        session.SetNodeId(nodeType, kInvalidNodeId);
        return std::nullopt;
    }
    return loginNodeIt->second;
}

void RpcClientSessionHandler::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    // todo如果我没登录就发送其他协议到controller game server 怎么办
    if (conn->connected()){
        HandleConnectionEstablished(conn);
    }
    else{
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
    message.set_message_id(SceneClientPlayerCommonSendTipToClientMessageId);
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

    // 检查请求体是否有效
    const std::string& requestBody = request->body();
    if (requestBody.empty()) {
        return;
    }

    if (!message.ParseFromArray(requestBody.data(), requestBody.size())) {
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
    const auto& loginNode = FindNodeForSession(sessionId, eNodeType::LoginNodeService);
    if (loginNode)
    {
        loginpb::LoginNodeDisconnectRequest request;
        request.set_session_id(sessionId);
        SessionDetails sessionDetails;
        sessionDetails.set_session_id(sessionId);
        loginpb::SendClientPlayerLoginDisconnect(tls.GetNodeRegistry(eNodeType::LoginNodeService), *loginNode, request, { kSessionBinMetaKey }, SerializeSessionDetails(sessionDetails));
    }

    // 通知中心服务器
    GateSessionDisconnectRequest request;
    request.mutable_session_info()->set_session_id(sessionId);
	gGateNode->CallRemoteMethodZoneCenter(CentreGateSessionDisconnectMessageId, request);

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
void HandleTcpNodeMessage(const Session& session, const RpcClientMessagePtr& request, Guid sessionId, const muduo::net::TcpConnectionPtr& conn)
{
    auto& messageInfo = gRpcServiceRegistry[request->message_id()];

	// 玩家没登录直接发其他消息，乱发消息
    entt::entity tcpNodeId = entt::entity{ session.GetNodeId(messageInfo.targetNodeType) };
	auto& registry = tls.GetNodeRegistry(messageInfo.targetNodeType);
    if (!registry.valid(tcpNodeId))
    {
        LOG_ERROR << "Invalid tcp node id " << session.GetNodeId(messageInfo.targetNodeType) << " for session id : " << sessionId << " registy " << NodeSystem::GetRegistryName(registry);
        RpcClientSessionHandler::SendTipToClient(conn, kServerCrashed);
        return;
    }

    auto& sceneNode = registry.get<RpcClientPtr>(tcpNodeId);
    ClientSendMessageToPlayerRequest message;
    message.mutable_message_content()->set_serialized_message(request->body());
    message.set_session_id(sessionId);
    message.mutable_message_content()->set_id(request->id());
    message.mutable_message_content()->set_message_id(request->message_id());
    sceneNode->CallRemoteMethod(SceneClientSendMessageToPlayerMessageId, message);

    LOG_TRACE << "Sent message to game node, session id: " << sessionId << ", message id: " << request->message_id();
}

void HandleLoginNodeMessage(Guid sessionId, const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn){
    auto loginNode = FindNodeForSession(sessionId, eNodeType::LoginNodeService);
    if (!loginNode)
    {
        LOG_ERROR << "Login node not found for session id: " << sessionId << ", message id: " << request->message_id();
        // TODO: Handle connection closure logic here.
        return;
    }

	auto& messageInfo = gRpcServiceRegistry[request->message_id()];

	SetSessionAndParseBody(*messageInfo.requestPrototype, request, sessionId);
	SessionDetails sessionDetails;
	sessionDetails.set_session_id(sessionId);

	if (messageInfo.messageSender)
	{
		messageInfo.messageSender(tls.GetNodeRegistry(messageInfo.targetNodeType), 
            *loginNode, 
            *messageInfo.requestPrototype, 
            { kSessionBinMetaKey }, 
            SerializeSessionDetails(sessionDetails));
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

	if (request->message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "Invalid message ID: " << request->message_id();
		return;
	}

    if (!CheckMessageSize(request, conn)) return;

	auto& session = sessionIt->second;
    if (!CheckMessageLimit(session, request, conn)) return;

	if (!gClientMessageIdWhitelist.contains(request->message_id())) {
		LOG_ERROR << "Client sent an invalid message: message ID not allowed - " << request->message_id();
		return;
	}

	auto& messageInfo = gRpcServiceRegistry[request->message_id()];
    if (messageInfo.protocolType == PROTOCOL_TCP){
		HandleTcpNodeMessage(session, request, sessionId, conn);
    }else if (messageInfo.protocolType == PROTOCOL_GRPC){
        if (messageInfo.targetNodeType == LoginNodeService)
        {
			HandleLoginNodeMessage(sessionId, request, conn);
        }
    }
}
