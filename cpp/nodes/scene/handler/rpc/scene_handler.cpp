
#include "scene_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "table/proto/tip/common_error_tip.pb.h"
#include "muduo/net/InetAddress.h"
#include "node/system/node/node.h"
#include "session/system/session.h"
#include "core/network/message_system.h"
#include "player/system/player_lifecycle.h"
#include "player/system/player_scene.h"
#include "scene/scene/system/game_node_scene.h"
#include "rpc/player_service_interface.h"
#include "network/rpc_session.h"
#include "network/network_constants.h"
#include "network/error_handling_system.h"
#include "proto/common/component/player_async_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"
#include "proto/common/component/player_network_comp.pb.h"
#include "proto/common/base/node.pb.h"
#include "rpc/service_metadata/rpc_event_registry.h"
#include "thread_context/redis_manager.h"
#include "type_alias/player_session_type_alias.h"
#include "core/utils/proto/proto_field_checker.h"
#include "network/network_utils.h"
#include "network/player_message_utils.h"
#include "thread_context/node_context_manager.h"
#include "thread_context/player_manager.h"
#include "core/system/redis.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "proto/db/proto_option.pb.h"
#include "network/node_utils.h"
#include <unordered_map>
#include <string>

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

using namespace muduo::net;

namespace {

MessagePriority GetMethodPriority(const google::protobuf::MethodDescriptor* method)
{
	if (!method) {
		return MESSAGE_PRIORITY_NORMAL;
	}

	const auto& options = method->options();
	if (!options.HasExtension(OptionMessagePriority)) {
		return MESSAGE_PRIORITY_NORMAL;
	}

	return options.GetExtension(OptionMessagePriority);
}

MessagePriority ResolveRoutedMessagePriority(const RoutePlayerMessageRequest& request)
{
	ClientRequest clientRequest;
	if (!clientRequest.ParseFromString(request.body())) {
		return GetMethodPriority(::Scene::descriptor()->FindMethodByName("RoutePlayerStringMsg"));
	}

	if (clientRequest.message_id() >= gRpcServiceRegistry.size()) {
		return GetMethodPriority(::Scene::descriptor()->FindMethodByName("RoutePlayerStringMsg"));
	}

	const auto& messageInfo = gRpcServiceRegistry[clientRequest.message_id()];
	const auto serviceIt = gPlayerService.find(messageInfo.serviceName);
	if (serviceIt == gPlayerService.end()) {
		return GetMethodPriority(::Scene::descriptor()->FindMethodByName("RoutePlayerStringMsg"));
	}

	google::protobuf::Service* service = serviceIt->second->service();
	if (!service || !service->GetDescriptor()) {
		return GetMethodPriority(::Scene::descriptor()->FindMethodByName("RoutePlayerStringMsg"));
	}

	return GetMethodPriority(service->GetDescriptor()->FindMethodByName(messageInfo.methodName));
}

bool IsImportantPriority(const MessagePriority priority)
{
	return priority == MESSAGE_PRIORITY_IMPORTANT;
}

}  // namespace

///<<< END WRITING YOUR CODE

void SceneHandler::PlayerEnterGameNode(::google::protobuf::RpcController* controller, const ::PlayerEnterGameNodeRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_DEBUG << "Handling EnterGs request for player: " << request->player_id();

	// 1 清除玩家会话，处理连续顶号进入情况
	PlayerLifecycleSystem::RemovePlayerSessionSilently(request->player_id());

	auto playerIt = tlsPlayerList.find(request->player_id());

	PlayerGameNodeEnteryInfoPBComponent enterInfo;

	// 2 检查玩家是否已经在线，若在线则直接进入
	if (playerIt != tlsPlayerList.end())
	{
		PlayerLifecycleSystem::EnterScene(playerIt->second, enterInfo);
		return;
	}

	// 3 玩家不在线，加入异步加载列表并尝试异步加载
	tlsRedisSystem.GetPlayerDataRedis()->AsyncLoad(request->player_id(), enterInfo);
///<<< END WRITING YOUR CODE
}

void SceneHandler::SendMessageToPlayer(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

    LOG_TRACE << "Handling message routing for session ID: " << request->header().session_id()
        << ", message ID: " << request->message_content().message_id();

    const auto it = SessionMap().find(request->header().session_id());
    if (it == SessionMap().end())
    {
        LOG_ERROR << "Session ID not found: " << request->header().session_id()
            << ", message ID: " << request->message_content().message_id();
        return;
    }

    const auto playerIt = tlsPlayerList.find(it->second);
    if (playerIt == tlsPlayerList.end())
    {
        LOG_ERROR << "Player ID not found in common logic: " << it->second;
        return;
    }

    const auto& player = playerIt->second;

    if (request->message_content().message_id() >= gRpcServiceRegistry.size())
    {
        LOG_ERROR << "Invalid message ID: " << request->message_content().message_id();
        return;
    }

    const auto& messageInfo = gRpcServiceRegistry[request->message_content().message_id()];
    const auto serviceIt = gPlayerService.find(messageInfo.serviceName);
    if (serviceIt == gPlayerService.end())
    {
        LOG_ERROR << "PlayerService not found for message ID: " << request->message_content().message_id();
        return;
    }

    const auto& serviceHandler = serviceIt->second;
    google::protobuf::Service* service = serviceHandler->service();
    const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
    if (nullptr == method)
    {
        LOG_ERROR << "Method not found in PlayerService: " << messageInfo.methodName;
        return;
    }

    const MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
    if (!playerRequest->ParsePartialFromArray(request->message_content().serialized_message().data(),
            int32_t(request->message_content().serialized_message().size())))
    {
        LOG_ERROR << "Failed to parse request message for message ID: " << request->message_content().message_id()
                  << " session=" << request->header().session_id();
        return;
    }

    std::string errorDetails;
    if (ProtoFieldChecker::CheckFieldSizes(*playerRequest, kProtoFieldCheckerThreshold, errorDetails)) {
        LOG_ERROR << errorDetails << " message_id=" << request->message_content().message_id()
                  << " session=" << request->header().session_id();
        return;
    }
    if (ProtoFieldChecker::CheckForNegativeInts(*playerRequest, errorDetails)) {
        LOG_ERROR << errorDetails << " message_id=" << request->message_content().message_id()
                  << " session=" << request->header().session_id();
        return;
    }

    const MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
    serviceHandler->CallMethod(method, player, playerRequest.get(), playerResponse.get());

    response->mutable_header()->set_session_id(request->header().session_id());
    response->mutable_message_content()->set_message_id(request->message_content().message_id());

    if (Empty::GetDescriptor() == playerResponse->GetDescriptor())
    {
        return;
    }

    response->mutable_message_content()->set_serialized_message(playerResponse->SerializeAsString());
	///<<< END WRITING YOUR CODE
}

void SceneHandler::ProcessClientPlayerMessage(::google::protobuf::RpcController* controller, const ::ProcessClientPlayerMessageRequest* request,
	::ProcessClientPlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
  // 新增：语义化的内部处理函数（处理客户端发来的玩家消息包）
		if (!request) return;

		const auto& msg = request->message_content();
		const uint64_t sessionId = request->session_id();

		if (msg.message_id() >= gRpcServiceRegistry.size())
		{
			LOG_ERROR << "ProcessClientPlayerMessage: message_id not found " << msg.message_id();
			return;
		}

		const auto& messageInfo = gRpcServiceRegistry.at(msg.message_id());
		const auto serviceIt = gPlayerService.find(messageInfo.serviceName);
		if (serviceIt == gPlayerService.end())
		{
			LOG_ERROR << "ProcessClientPlayerMessage: player service not found for message id " << msg.message_id();
			return;
		}

		google::protobuf::Service* service = serviceIt->second->service();
		const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
		if (nullptr == method)
		{
			LOG_ERROR << "ProcessClientPlayerMessage: method not found " << messageInfo.methodName;
			return;
		}

		const auto it = SessionMap().find(sessionId);
		if (it == SessionMap().end())
		{
			LOG_ERROR << "ProcessClientPlayerMessage: session id not found " << sessionId << ", message id " << msg.message_id();
			return;
		}

		const auto player = GetPlayer(it->second);
		if (entt::null == player)
		{
			LOG_ERROR << "ProcessClientPlayerMessage: player not loaded for player_id=" << it->second
				<< " message_id=" << msg.message_id();
			return;
		}

		// 解析请求并做字段检查
		const MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
		if (!playerRequest->ParsePartialFromArray(msg.serialized_message().data(),
			static_cast<int32_t>(msg.serialized_message().size())))
		{
			LOG_ERROR << "ProcessClientPlayerMessage: failed to parse client-sent request message_id=" << msg.message_id()
				<< " session=" << sessionId;
			return;
		}

		std::string errorDetails;
		if (ProtoFieldChecker::CheckFieldSizes(*playerRequest, kProtoFieldCheckerThreshold, errorDetails)) {
			LOG_ERROR << "ProcessClientPlayerMessage: " << errorDetails << " message_id=" << msg.message_id()
				<< " session=" << sessionId;
			return;
		}
		if (ProtoFieldChecker::CheckForNegativeInts(*playerRequest, errorDetails)) {
			LOG_ERROR << "ProcessClientPlayerMessage: " << errorDetails << " message_id=" << msg.message_id()
				<< " session=" << sessionId;
			return;
		}

		// 调用具体的 player service 方法
		const MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
		serviceIt->second->CallMethod(method, player, playerRequest.get(), playerResponse.get());

		// 填充同步响应（如果调用方需要）
		if (response != nullptr && Empty::GetDescriptor() != playerResponse->GetDescriptor()) {
			response->mutable_message_content()->set_message_id(msg.message_id());
			response->mutable_message_content()->set_id(msg.id());
			response->set_session_id(sessionId);

			// 序列化响应
			const auto byte_size = playerResponse->ByteSizeLong();
			response->mutable_message_content()->mutable_serialized_message()->resize(byte_size);
			if (!playerResponse->SerializePartialToArray(response->mutable_message_content()->mutable_serialized_message()->data(),
				static_cast<int32_t>(byte_size)))
			{
				LOG_ERROR << "ProcessClientPlayerMessage: Failed to serialize response for message ID: " << msg.message_id();
				// 若需要，可在此填充 error message
			}
		}

///<<< END WRITING YOUR CODE
}

void SceneHandler::CentreSendToPlayerViaGameNode(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const auto it = SessionMap().find(request->header().session_id());
	if (it == SessionMap().end())
	{
		LOG_ERROR << "session id not found " << request->header().session_id() << ","
			<< " message id " << request->message_content().message_id();
		return;
	}

	const auto player = GetPlayer(it->second);
	if (entt::null == player)
	{
		LOG_ERROR << "GatePlayerService player not loading";
		return;
	}

	::SendMessageToClientViaGate(request->message_content().message_id(), request->message_content(), player);
///<<< END WRITING YOUR CODE
}

void SceneHandler::InvokePlayerService(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const auto it = SessionMap().find(request->header().session_id());
	if (it == SessionMap().end())
	{
		LOG_ERROR << "session id not found " << request->header().session_id() << ","
			<< " message id " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kSessionNotFound);
		return;
	}

	const auto player = GetPlayer(it->second);
	if (entt::null == player)
	{
		LOG_ERROR << "GatePlayerService player not loading";
		SendErrorToClient(*request, *response, kPlayerNotFoundInSession);
		return;
	}

	if (request->message_content().message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "message_id not found " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kMessageIdNotFound);
		return;
	}

	const auto& messageInfo = gRpcServiceRegistry[request->message_content().message_id()];
	const auto serviceIt = gPlayerService.find(messageInfo.serviceName);
	if (serviceIt == gPlayerService.end())
	{
		LOG_ERROR << "PlayerService service not found " << request->header().session_id()
			<< "," << request->message_content().message_id();
		return;
	}

	const auto& serviceHandler = serviceIt->second;
	google::protobuf::Service* service = serviceHandler->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (nullptr == method)
	{
		LOG_ERROR << "PlayerService method not found " << request->message_content().message_id();
		return;
	}

	MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
	if (!playerRequest->ParsePartialFromArray(request->message_content().serialized_message().data(), int32_t(request->message_content().serialized_message().size())))
	{
		LOG_ERROR << "ParsePartialFromArray " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kRequestMessageParseError);
		return;
	}

    std::string errorDetails;

    // 检查字段大小
    if (ProtoFieldChecker::CheckFieldSizes(*playerRequest, kProtoFieldCheckerThreshold, errorDetails)) {
        LOG_ERROR << errorDetails << " Failed to check request for message ID: "
            << request->message_content().message_id();
        SendErrorToClient(*request, *response, kArraySizeTooLargeInMessage);
        return;
    }

    // 检查负数
    if (ProtoFieldChecker::CheckForNegativeInts(*playerRequest, errorDetails)) {
        LOG_ERROR << errorDetails << " Failed to check request for message ID: "
            << request->message_content().message_id();
        SendErrorToClient(*request, *response, kNegativeValueInMessage);
        return;
    }

	MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
	serviceHandler->CallMethod(method, player, playerRequest.get(), playerResponse.get());

    response->mutable_header()->set_session_id(request->header().session_id());
    response->mutable_message_content()->set_message_id(request->message_content().message_id());

    if (Empty::GetDescriptor() == playerResponse->GetDescriptor()) {
        return;
    }

	auto& tipInfoMessage = tlsRegistryManager.globalRegistry.get_or_emplace<TipInfoMessage>(GlobalEntity());
	response->mutable_message_content()->mutable_error_message()->CopyFrom(tipInfoMessage);
	tipInfoMessage.Clear();

	const auto byte_size = playerResponse->ByteSizeLong();
	response->mutable_message_content()->mutable_serialized_message()->resize(byte_size);
	if (!playerResponse->SerializePartialToArray(response->mutable_message_content()->mutable_serialized_message()->data(),
	                                             static_cast<int32_t>(byte_size)))
	{
		LOG_ERROR << "Failed to serialize response for message ID: " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kResponseMessageParseError);
		return;
	}

///<<< END WRITING YOUR CODE
}

void SceneHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller, const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller, const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	if (!request || !response) {
		return;
	}

	// Always mirror request in response so callers can correlate route result.
	response->set_body(request->body());
	response->mutable_player_info()->CopyFrom(request->player_info());
	response->mutable_node_list()->CopyFrom(request->node_list());

	// Local fast path: player already loaded on this scene and no further route nodes.
	const Guid playerId = request->player_info().player_id();
	const MessagePriority routedPriority = ResolveRoutedMessagePriority(*request);
	const bool importantRoute = IsImportantPriority(routedPriority);
	if (playerId != kInvalidGuid) {
		const auto localPlayer = GetPlayer(playerId);
		if (localPlayer != entt::null && request->node_list_size() == 0) {
			LOG_TRACE << "RoutePlayerStringMsg local hit, player_id=" << playerId;
			return;
		}
	}

	if (request->node_list_size() == 0) {
		if (importantRoute) {
			LOG_ERROR << "RoutePlayerStringMsg IMPORTANT route dropped: no next node, player_id=" << playerId;
			// Centre decommissioned: no fallback relay. TODO: query player_locator and re-route via Kafka.
		} else {
			LOG_WARN << "RoutePlayerStringMsg NORMAL route dropped: no next node, player_id=" << playerId;
		}
		return;
	}

	RoutePlayerMessageRequest nextRequest(*request);
	// Consume the current hop before forwarding.
	nextRequest.mutable_node_list()->DeleteSubrange(0, 1);

	const auto& nextNode = request->node_list(0);
	entt::entity nextNodeEntity{ nextNode.node_id() };
	auto& nextRegistry = tlsNodeContextManager.GetRegistry(nextNode.node_type());
	if (!nextRegistry.valid(nextNodeEntity)) {
		LOG_ERROR << "RoutePlayerStringMsg route dropped: next node not found, node_type=" << nextNode.node_type()
				  << ", node_id=" << nextNode.node_id() << ", player_id=" << playerId;
		return;
	}

	const auto nextSession = nextRegistry.try_get<RpcSession>(nextNodeEntity);
	if (!nextSession) {
		LOG_ERROR << "RoutePlayerStringMsg route dropped: next node session missing, node_type=" << nextNode.node_type()
				  << ", node_id=" << nextNode.node_id() << ", player_id=" << playerId;
		return;
	}

	switch (nextNode.node_type()) {
	case eNodeType::SceneNodeService:
		nextSession->SendRequest(SceneRoutePlayerStringMsgMessageId, nextRequest);
		return;
	case eNodeType::GateNodeService:
		nextSession->SendRequest(GateRoutePlayerMessageMessageId, nextRequest);
		return;
	default:
		LOG_WARN << "RoutePlayerStringMsg route dropped: unsupported next node_type=" << nextNode.node_type()
				 << ", node_id=" << nextNode.node_id() << ", player_id=" << playerId;
		return;
	}
///<<< END WRITING YOUR CODE
}

void SceneHandler::UpdateSessionDetail(::google::protobuf::RpcController* controller, const ::RegisterPlayerSessionRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerLifecycleSystem::RemovePlayerSession(request->player_id());

	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
	if (const entt::entity gateNodeId{ GetGateNodeId(request->session_id()) };
		!registry.valid(gateNodeId))
	{
		LOG_ERROR << "Gate not found " << GetGateNodeId(request->session_id());
		return;
	}

	const auto player = GetPlayer(request->player_id());
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player not found " << request->player_id();
		return;
	}

	SessionMap().emplace(request->session_id(), request->player_id());

	tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSessionSnapshotPBComp>(player).set_gate_session_id(request->session_id());

	PlayerLifecycleSystem::HandleBindPlayerToGateOK(player);
///<<< END WRITING YOUR CODE
}

void SceneHandler::EnterScene(::google::protobuf::RpcController* controller, const ::Centre2GsEnterSceneRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

    auto player = GetPlayer(request->player_id());
    if (player == entt::null)
    {
        LOG_ERROR << "EnterScene: Player entity not found for player_id " << request->player_id();
        return;
    }

    LOG_INFO << "Player with ID " << request->player_id() << " entering scene " << request->scene_id();

    entt::entity sceneEntity{ request->scene_id() };
    // 可选：检查 sceneEntity 是否有效（SceneCommon 内部通常会校验）
    if (!tlsRegistryManager.actorRegistry.valid(sceneEntity) && !tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService).valid(sceneEntity)) {
        LOG_ERROR << "EnterScene: invalid scene entity " << request->scene_id();
        return;
    }

    PlayerSceneSystem::HandleEnterScene(player, sceneEntity);
	///<<< END WRITING YOUR CODE
}

void SceneHandler::CreateScene(::google::protobuf::RpcController* controller, const ::CreateSceneRequest* request,
	::CreateSceneResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneHandler::NodeHandshake(::google::protobuf::RpcController* controller, const ::NodeHandshakeRequest* request,
	::NodeHandshakeResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnNodeHandshake(*request, *response);
///<<< END WRITING YOUR CODE
}
