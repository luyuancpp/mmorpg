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
#include "proto/common/node.pb.h"
#include "node/system/node_util.h"
#include "google/protobuf/descriptor.h"
#include "network/network_utils.h"
#include "util/node_utils.h"
#include "proto/logic/event/node_event.pb.h"
#include "thread_local/node_context_manager.h"
#include <session/manager/session_manager.h>
#include "thread_local/dispatcher_manager.h"

static std::optional<entt::entity> PickRandomNode(uint32_t nodeType, uint32_t targetNodeType) {
	std::vector<entt::entity> candidates;
	auto& registry = NodeContextManager::Instance().GetRegistry(nodeType);
	auto view = registry.view<NodeInfo>();
	for (auto entity : view) {
		const auto& node = view.get<NodeInfo>(entity);
		if (node.zone_id() == GetNodeInfo().zone_id()) {
			candidates.push_back(entity);
		}
	}

	if (candidates.empty()) {
		return std::nullopt;
	}

	// 随机选一个
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, candidates.size() - 1);
	return candidates[dis(gen)];
}


static inline NodeId GetEffectiveNodeId(
	const Session& session,
	uint32_t nodeType)
{
	if (IsZoneSingletonNodeType(nodeType)) {
        auto node = FindZoneUniqueNodeInfo(gGateNode->GetNodeInfo().zone_id(), nodeType);
        if (node == nullptr) {
            LOG_ERROR << "Node not found for type: " << nodeType;
            return kInvalidNodeId;
        }

		return node->node_id();
	}
	else {
		return session.GetNodeId(nodeType);
	}
}

RpcClientSessionHandler::RpcClientSessionHandler(ProtobufCodec& codec,
    ProtobufDispatcher& dispatcherParam)
    : protobufCodec(codec),
    messageDispatcher(dispatcherParam)
{
    messageDispatcher.registerMessageCallback<ClientRequest>(
        std::bind(&RpcClientSessionHandler::DispatchClientRpcMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	dispatcher.sink<OnNodeRemovePbEvent>().connect<&RpcClientSessionHandler::OnNodeRemovePbEventHandler>(*this);

}

//todo 考虑中间一个login服务关了，原来的login服务器处理到一半，新的login处理不了
std::optional<entt::entity> ResolveSessionTargetNode(uint64_t sessionId, uint32_t nodeType)
{
	// 先判断是否是 zone 单例类型（全局唯一逻辑）
	if (IsZoneSingletonNodeType(nodeType)) {
		auto nodeInfo = FindZoneUniqueNodeInfo(gGateNode->GetNodeInfo().zone_id(), nodeType);
		if (!nodeInfo) {
			LOG_ERROR << "Singleton node not found for nodeType: " << nodeType;
			return std::nullopt;
		}

		const auto& registry = NodeContextManager::Instance().GetRegistry(nodeType);
		auto entity = entt::entity{ nodeInfo->node_id() };

		if (!registry.valid(entity)) {
			LOG_ERROR << "[SingletonNode] Entity invalid. nodeType: " << nodeType;
			return std::nullopt;
		}

		return entity;
	}

	// 普通节点：需要 session 绑定
	const auto sessionIt = SessionManager::Instance().sessions().find(sessionId);
	if (sessionIt == SessionManager::Instance().sessions().end()) {
		LOG_ERROR << "Session not found for session id: " << sessionId;
		return std::nullopt;
	}

	auto& session = sessionIt->second;

	if (!session.HasNodeId(nodeType)) {
		auto randomNode = PickRandomNode(nodeType, nodeType);
		if (!randomNode) {
			LOG_ERROR << "[LoginNode] No available login node for session id: " << sessionId;
			return std::nullopt;
		}
		session.SetNodeId(nodeType, entt::to_integral(*randomNode));
	}

	const auto& registry = NodeContextManager::Instance().GetRegistry(nodeType);
	entt::entity nodeEntity = entt::entity{ GetEffectiveNodeId(session, nodeType) };
	if (!registry.valid(nodeEntity)) {
		LOG_ERROR << "[LoginNode] Bound login node is invalid. session id: " << sessionId;
		session.SetNodeId(nodeType, kInvalidNodeId);
		return std::nullopt;
	}

	return nodeEntity;
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
    constexpr size_t kMaxClientMessageSize = 1024;
    if (request->ByteSizeLong() > kMaxClientMessageSize) {
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
void ParseMessageFromRequestBody(Message& message, const Request& request, const uint64_t sessionId) {
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
    const auto& loginNode = ResolveSessionTargetNode(sessionId, eNodeType::LoginNodeService);
    if (loginNode)
    {
        loginpb::LoginNodeDisconnectRequest request;
        request.set_session_id(sessionId);
        SessionDetails sessionDetails;
        sessionDetails.set_session_id(sessionId);
        loginpb::SendClientPlayerLoginDisconnect(NodeContextManager::Instance().GetRegistry(eNodeType::LoginNodeService), *loginNode, request, { kSessionBinMetaKey }, SerializeSessionDetails(sessionDetails));
    }

    GateSessionDisconnectRequest request;
    request.mutable_session_info()->set_session_id(sessionId);
	gGateNode->CallRemoteMethodZoneCenter(CentreGateSessionDisconnectMessageId, request);

    // 删除会话
    SessionManager::Instance().sessions().erase(sessionId);

    LOG_TRACE << "Disconnected session id: " << sessionId;
}

void RpcClientSessionHandler::HandleConnectionEstablished(const muduo::net::TcpConnectionPtr& conn)
{
	auto sessionId = SessionManager::Instance().session_id_gen().Generate();
	while (SessionManager::Instance().sessions().contains(sessionId))
	{
		sessionId = SessionManager::Instance().session_id_gen().Generate();
	}

	// 用session id 防止改包把消息发给其他玩家

	conn->setContext(sessionId);
	Session session;
	session.conn = conn;
	SessionManager::Instance().sessions().emplace(sessionId, std::move(session));

	LOG_TRACE << "New connection, assigned session id: " << sessionId;
}

// Handle messages related to the game node
void HandleTcpNodeMessage(const Session& session, const RpcClientMessagePtr& request, Guid sessionId, const muduo::net::TcpConnectionPtr& conn)
{
    auto& handlerMeta = gRpcServiceRegistry[request->message_id()];

	// 玩家没登录直接发其他消息，乱发消息
    entt::entity tcpNodeId = entt::entity{ GetEffectiveNodeId(session, handlerMeta.targetNodeType) };
	auto& registry = NodeContextManager::Instance().GetRegistry(handlerMeta.targetNodeType);
	if (!registry.valid(tcpNodeId))
	{
		LOG_ERROR << "[TCP Node Error] Invalid tcp node id: " << static_cast<uint32_t>(tcpNodeId)
			<< ", message_id: " << request->message_id()
			<< ", session_id: " << sessionId
			<< ", node_type: " << handlerMeta.targetNodeType
			<< ", registry: " << NodeUtils::GetRegistryName(registry);

		RpcClientSessionHandler::SendTipToClient(conn, kServerCrashed);
		return;
	}

    auto& tcpNode = registry.get<RpcClientPtr>(tcpNodeId);
    ClientSendMessageToPlayerRequest message;
    message.mutable_message_content()->set_serialized_message(request->body());
    message.set_session_id(sessionId);
    message.mutable_message_content()->set_id(request->id());
    message.mutable_message_content()->set_message_id(request->message_id());
    tcpNode->CallRemoteMethod(SceneClientSendMessageToPlayerMessageId, message);

    LOG_TRACE << "Sent message to game node, session id: " << sessionId << ", message id: " << request->message_id();
}

void HandleGrpcNodeMessage(Guid sessionId, const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn){
	auto& rpcHandlerMeta  = gRpcServiceRegistry[request->message_id()];
	ParseMessageFromRequestBody(*rpcHandlerMeta .requestPrototype, request, sessionId);

	SessionDetails sessionDetails;
	sessionDetails.set_session_id(sessionId);
	const auto sessionIt = SessionManager::Instance().sessions().find(sessionId);
	if (sessionIt == SessionManager::Instance().sessions().end()) {
		LOG_ERROR << "Session not found for session id: " << sessionId;
		return ;
	}
    sessionDetails.set_player_id(sessionIt->second.playerGuild);

	if (rpcHandlerMeta .messageSender){
		auto node = ResolveSessionTargetNode(sessionId, rpcHandlerMeta .targetNodeType);
		if (!node)
		{
			LOG_ERROR << "Node not found for session id: " << sessionId << ", message id: " << request->message_id();
			// TODO: Handle connection closure logic here.
			return;
		}

		
		rpcHandlerMeta .messageSender(NodeContextManager::Instance().GetRegistry(rpcHandlerMeta .targetNodeType), 
            *node, 
            *rpcHandlerMeta .requestPrototype, 
            { kSessionBinMetaKey }, 
            SerializeSessionDetails(sessionDetails));
	}
}

bool RpcClientSessionHandler::ValidateClientMessage(Session& session, const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn) const {
	if (!CheckMessageSize(request, conn)) return false;
	if (!CheckMessageLimit(session, request, conn)) return false;
	return true;
}


//// Main request handler, forwards the request to the appropriate service
void RpcClientSessionHandler::DispatchClientRpcMessage(const muduo::net::TcpConnectionPtr& conn,
	const RpcClientMessagePtr& request,
	muduo::Timestamp)
{
	auto sessionId = GetSessionId(conn);
	const auto sessionIt = SessionManager::Instance().sessions().find(sessionId);
	if (sessionIt == SessionManager::Instance().sessions().end()) {
		LOG_ERROR << "[Invalid Session] No session found for conn session_id: " << sessionId
			<< ", message_id: " << request->message_id();
		return;
	}


	if (request->message_id() >= gRpcServiceRegistry.size() || !gClientMessageIdWhitelist.contains(request->message_id())) {
		LOG_ERROR << "Invalid or unauthorized message ID: " << request->message_id();
		return;
	}

	auto& session = sessionIt->second;

    if (!ValidateClientMessage(session, request, conn)) return;

	auto& messageInfo = gRpcServiceRegistry[request->message_id()];
    if (messageInfo.protocolType == PROTOCOL_TCP){
		HandleTcpNodeMessage(session, request, sessionId, conn);
    }else if (messageInfo.protocolType == PROTOCOL_GRPC){
    	HandleGrpcNodeMessage(sessionId, request, conn);
    }
}


void RpcClientSessionHandler::OnNodeRemovePbEventHandler(const OnNodeRemovePbEvent& pb)
{
	auto& registry = NodeContextManager::Instance().GetRegistry(pb.node_type());
	for (auto& session : SessionManager::Instance().sessions())
	{
		if (session.second.GetNodeId(pb.node_type()) != pb.entity()) continue;
		session.second.SetNodeId(pb.node_type(), kInvalidNodeId);
	}
}
