#include "centre_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "centre_node.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/mainscene_table.h"
#include "session/system/session.h"
#include "rpc/player_service_interface.h"
#include "muduo/net/Callbacks.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "network/error_handling_system.h"
#include "table/proto/tip/login_error_tip.pb.h"
#include "player/system/player_lifecycle.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "scene/system/player_change_room.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "rpc/service_metadata/service_metadata.h"
#include "threading/redis_manager.h"
#include "type_alias/player_session_type_alias.h"
#include "core/utils/defer/defer.h"
#include "core/utils/proto/proto_field_checker.h"
#include "core/utils/debug/stacktrace_system.h"
#include "player/system/player_tip.h"
#include "rpc/service_metadata/centre_player_scene_service_metadata.h"
#include "type_alias/player_redis.h"
#include "network/network_utils.h"
#include "network/player_message_utils.h"
#include "threading/node_context_manager.h"
#include "threading/player_manager.h"
#include "threading/message_context.h"
#include <modules/scene/comp/room_node_comp.h>
#include <scene/system/room.h>

using namespace muduo;
using namespace muduo::net;

constexpr std::size_t kMaxPlayerSize{50000};

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Guid GetPlayerIDBySessionId(const uint64_t session_id)
{
	const auto session_it = GlobalSessionList().find(session_id);
	if (session_it == GlobalSessionList().end())
	{
		LOG_DEBUG << "Cannot find session ID " << session_id << GetStackTraceAsString();
		return kInvalidGuid;
	}
	return session_it->second;
}

entt::entity GetPlayerEntityBySessionId(uint64_t session_id)
{
	auto player_id = GetPlayerIDBySessionId(session_id);

	LOG_TRACE << "Getting player entity for session ID: " << session_id << ", player ID: " << player_id;

	const auto player_it = tlsPlayerList.find(player_id);
	if (player_it == tlsPlayerList.end())
	{
		LOG_ERROR << "Player not found for session ID: " << session_id << ", player ID: " << player_id;
		return entt::null;
	}

	LOG_TRACE << "Player entity found for session ID: " << session_id << ", player ID: " << player_id;

	return player_it->second;
}


// ---------- refactor helpers (匿名 namespace) ----------
// helpers for login / reconnect / kick decision (single-threaded simplified)
namespace {

	enum class EnterGameDecision {
		FirstLogin,
		Reconnect,
		ReplaceLogin
	};

	// 纯函数：根据当前是否已有 session/ token 判断行为（不做副作用）
	EnterGameDecision DecideEnterGame(bool hasOldSession,
		uint64_t oldSessionId,
		const std::string& oldLoginToken,
		uint64_t newSessionId,
		const std::string& newLoginToken)
	{
		if (!hasOldSession) return EnterGameDecision::FirstLogin;
		if (oldSessionId == newSessionId) return EnterGameDecision::Reconnect;
		if (oldLoginToken == newLoginToken) return EnterGameDecision::Reconnect;
		return EnterGameDecision::ReplaceLogin;
	}

	// 发送踢人消息（幂等）；返回是否实际尝试发送
	bool SendKickForOldSession(uint64_t oldSessionId)
	{
		if (oldSessionId == kInvalidSessionId) return false;
		KickSessionRequest kickMsg;
		kickMsg.set_session_id(oldSessionId);
		SendMessageToGateById(GateKickSessionByCentreMessageId, kickMsg, GetGateNodeId(oldSessionId));
		LOG_INFO << "Requested kick for old session: " << oldSessionId;
		return true;
	}

	// 更新/创建 player 的 session snapshot（写入新的 sessionId + token）
	// 返回旧的 sessionId 与旧 token（用于判定与后续清理）
	std::pair<uint64_t, std::string> UpdatePlayerSessionSnapshot(entt::entity playerEntity,
		uint64_t newSessionId,
		const std::string& newLoginToken)
	{
		auto& registry = tlsRegistryManager.actorRegistry;
		auto& sessionPB = registry.get_or_emplace<PlayerSessionSnapshotPBComp>(playerEntity);
		uint64_t oldSessionId = sessionPB.gate_session_id();
		std::string oldToken = sessionPB.login_token();
		sessionPB.set_gate_session_id(newSessionId);
		sessionPB.set_login_token(newLoginToken);
		// 持久化 extra fields（如果需要）
		GetPlayerCentreDataRedis()->UpdateExtraData(tlsRegistryManager.actorRegistry.get<Guid>(playerEntity), sessionPB);
		return { oldSessionId, oldToken };
	}

	// 首次登录处理（异步加载/创建玩家数据）
	void HandleFirstLogin(Guid playerGuid, uint64_t sessionId, const std::string& loginToken)
	{
		PlayerSessionSnapshotPBComp sessionPB;
		sessionPB.set_gate_session_id(sessionId);
		sessionPB.set_login_token(loginToken);
		GetPlayerCentreDataRedis()->AsyncLoad(playerGuid, sessionPB);
		GetPlayerCentreDataRedis()->UpdateExtraData(playerGuid, sessionPB);
		LOG_INFO << "HandleFirstLogin for player " << playerGuid;
	}

	// 已有 player 的后续执行（设置进入类型并触发进入场景流程）
	void ApplyEnterGameDecision(entt::entity playerEntity, EnterGameDecision decision)
	{
		auto& registry = tlsRegistryManager.actorRegistry;
		auto& enterComp = registry.get_or_emplace<PlayerEnterGameStatePbComp>(playerEntity);
		switch (decision) {
		case EnterGameDecision::FirstLogin:
			enterComp.set_enter_gs_type(LOGIN_FIRST);
			break;
		case EnterGameDecision::Reconnect:
			enterComp.set_enter_gs_type(LOGIN_RECONNECT);
			break;
		case EnterGameDecision::ReplaceLogin:
			enterComp.set_enter_gs_type(LOGIN_REPLACE);
			break;
		}
		PlayerLifecycleSystem::AddGameNodePlayerToGateNode(playerEntity);
		PlayerLifecycleSystem::ProcessPlayerSessionState(playerEntity);
	}

} // namespace

///<<< END WRITING YOUR CODE


void CentreHandler::GatePlayerService(::google::protobuf::RpcController* controller, const ::GateClientMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}



void CentreHandler::GateSessionDisconnect(::google::protobuf::RpcController* controller, const ::GateSessionDisconnectRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
	//考虑a 断 b 断 a 断 b 断.....(中间不断重连)
	//notice 异步过程 gate 先重连过来，然后断开才收到，也就是会把新来的连接又断了，极端情况，也要测试如果这段代码过去了，会有什么问题
	//玩家已经断开连接了

	// Ensure disconnection is handled by the current gate to prevent async message order issues
	// Check for scenarios where reconnect-disconnect cycles might occur in rapid succession
	// Notice: Asynchronous process: If the gate reconnects first and then disconnects, it might
	// inadvertently disconnect a newly arrived connection. Extreme cases need testing to see

	defer(GlobalSessionList().erase(request->session_info().session_id()));

	auto session_id = request->session_info().session_id();

	auto player_id = GetPlayerIDBySessionId(session_id);

	LOG_TRACE << "Getting player entity for session ID: " << session_id << ", player ID: " << player_id;

	const auto player_it = tlsPlayerList.find(player_id);
	if (player_it == tlsPlayerList.end())
	{
		LOG_TRACE << "Player not found for session ID: " << session_id << ", player ID: " << player_id;
		return ;
	}

	LOG_TRACE << "Player entity found for session ID: " << session_id << ", player ID: " << player_id;

	auto playerEntity =  player_it->second;

	const auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (sessionPB == nullptr)
	{
		LOG_ERROR << "PlayerNodeInfo not found for player entity: " << tlsRegistryManager.actorRegistry.get<Guid>(playerEntity);
		return;
	}

	if (sessionPB->gate_session_id() != session_id)
	{
		LOG_ERROR << "Mismatched gate session ID for player: " << sessionPB->gate_session_id()
			<< ", expected session ID: " << session_id;
		return;
	}

	const auto& nodeIdMap = sessionPB->node_id();
	auto it = nodeIdMap.find(eNodeType::SceneNodeService);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << eNodeType::SceneNodeService
			<< ", player entity: " << entt::to_integral(playerEntity);
		return;
	}

	const entt::entity gameNodeId{ it->second };
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
	if (!registry.valid(gameNodeId))
	{
		LOG_ERROR << "Invalid game node ID found for player: " << tlsRegistryManager.actorRegistry.get<Guid>(playerEntity);
		return;
	}

	const auto gameNode = registry.try_get<RpcSession>(gameNodeId);
	if (gameNode == nullptr)
	{
		LOG_ERROR << "RpcSession not found for game node ID: " << it->second;
		return;
	}

	const auto playerId = tlsRegistryManager.actorRegistry.get<Guid>(playerEntity);

	LOG_INFO << "Handling disconnect for player: " << playerId;

	PlayerLifecycleSystem::HandleNormalExit(playerId);

///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeAccountLogin(::google::protobuf::RpcController* controller, const ::CentreLoginRequest* request,
	::CentreLoginResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    
	if (tlsPlayerList.size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是账号排队，还是角色排队>???
		response->mutable_error_message()->set_id(kLoginAccountPlayerFull);
		return;
	}
	//排队
    //todo 排队队列里面有同一个人的两个链接
	//如果不是同一个登录服务器,踢掉已经登录的账号
	//告诉客户端登录中
///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeEnterGame(::google::protobuf::RpcController* controller, const ::CentrePlayerGameNodeEntryRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto& clientMsg = request->client_msg_body();
	const auto& sessionInfo = request->session_info();
	Guid playerGuid = clientMsg.player_id();
	uint64_t sessionId = sessionInfo.session_id();
	const std::string loginToken = clientMsg.login_token();

	LOG_INFO << "LoginNodeEnterGame request: player=" << playerGuid << " session=" << sessionId;

	// 在单线程模型下，直接插入全局 session map
	GlobalSessionList()[sessionId] = playerGuid;

	// 是否在内存中已有 player 对象
	auto it = tlsPlayerList.find(playerGuid);
	if (it == tlsPlayerList.end()) {
		// 首次登录：load data 并返回（后续流程由异步回调继续）
		HandleFirstLogin(playerGuid, sessionId, loginToken);
		return;
	}

	entt::entity playerEntity = it->second;
	bool hasOldSession = tlsRegistryManager.actorRegistry.any_of<PlayerSessionSnapshotPBComp>(playerEntity);

	// 更新 snapshot，获取旧 session/token 以便决定是否踢人
	auto old = UpdatePlayerSessionSnapshot(playerEntity, sessionId, loginToken);
	uint64_t oldSessionId = old.first;
	std::string oldToken = std::move(old.second);

	// 决策（pure）
	EnterGameDecision decision = DecideEnterGame(hasOldSession, oldSessionId, oldToken, sessionId, loginToken);

	// 如果是 ReplaceLogin，则发送踢人请求（Gate 层需以 sessionId 校验幂等）
	if (decision == EnterGameDecision::ReplaceLogin && oldSessionId != kInvalidSessionId && oldSessionId != sessionId) {
		SendKickForOldSession(oldSessionId);
		// 清理老映射，防止残留（单线程，安全）
		GlobalSessionList().erase(oldSessionId);
	}

	// 执行进入场景相关副作用
	ApplyEnterGameDecision(playerEntity, decision);
	///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeLeaveGame(::google::protobuf::RpcController* controller, const ::LoginNodeLeaveGameRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	if (GlobalSessionList().find(request->session_info().session_id()) == GlobalSessionList().end()) {
		return;
	}

	defer(GlobalSessionList().erase(request->session_info().session_id()));
	const auto player_id = GetPlayerIDBySessionId(request->session_info().session_id());
	PlayerLifecycleSystem::HandleNormalExit(player_id);
	//todo statistics
///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeSessionDisconnect(::google::protobuf::RpcController* controller, const ::GateSessionDisconnectRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	
	if (GlobalSessionList().find(request->session_info().session_id()) == GlobalSessionList().end()){
		return;
	}

	defer(GlobalSessionList().erase(request->session_info().session_id()));
	const auto player_id = GetPlayerIDBySessionId(request->session_info().session_id());
	PlayerLifecycleSystem::HandleNormalExit(player_id);
///<<< END WRITING YOUR CODE
}



void CentreHandler::PlayerService(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto it = GlobalSessionList().find(request->header().session_id());
	if (it == GlobalSessionList().end())
	{
		if (request->message_content().message_id() != CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteMessageId)
		{
			LOG_ERROR << "Session not found: " << request->header().session_id() << " message id :" << request->message_content().message_id();
			SendErrorToClient(*request, *response, kSessionNotFound);
		}
		return;
	}

	const auto playerId = it->second;
	auto playerEntityIt = tlsPlayerList.find(playerId);
	if (tlsPlayerList.end() == playerEntityIt)
	{
		LOG_ERROR << "Player not found: " << playerId;
		SendErrorToClient(*request, *response, kPlayerNotFoundInSession);
		return;
	}

	const auto player = playerEntityIt->second;
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player not found: " << playerId;
		SendErrorToClient(*request, *response, kPlayerNotFoundInSession);
		return;
	}

	if (request->message_content().message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "Message ID not found: " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kMessageIdNotFound);
		return;
	}

	const auto& message_info = gRpcServiceRegistry.at(request->message_content().message_id());

	const auto service_it = gPlayerService.find(message_info.serviceName);
	if (service_it == gPlayerService.end())
	{
		LOG_ERROR << "Player service not found: " << message_info.serviceName;
		return;
	}

	const auto& service_handler = service_it->second;
	google::protobuf::Service* service = service_handler->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.methodName);
	if (!method)
	{
		LOG_ERROR << "Method not found: " << message_info.methodName;
		return;
	}

	const MessagePtr playerRequest(service->GetRequestPrototype(method).New());
	if (!playerRequest->ParsePartialFromArray(request->message_content().serialized_message().data(),
		request->message_content().serialized_message().size()))
	{
		LOG_ERROR << "Failed to parse request for message ID: " << request->message_content().message_id();
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

	const MessagePtr playerResponse(service->GetResponsePrototype(method).New());

	// Call method on player service handler
	service_handler->CallMethod(method, player, get_pointer(playerRequest), get_pointer(playerResponse));

	// If response is nullptr, no need to send a reply
	if (!response)
	{
		return;
	}

	if (Empty::GetDescriptor() == playerResponse->GetDescriptor()) {
		return;
	}

	response->mutable_header()->set_session_id(request->header().session_id());
	const auto byte_size = playerResponse->ByteSizeLong();
	response->mutable_message_content()->mutable_serialized_message()->resize(byte_size);
	if (!playerResponse->SerializePartialToArray(response->mutable_message_content()->mutable_serialized_message()->data(),
												 static_cast<int32_t>(byte_size)))
	{
		LOG_ERROR << "Failed to serialize response for message ID: " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kResponseMessageParseError);
		return;
	}

	response->mutable_message_content()->set_message_id(request->message_content().message_id());
	
	if (const auto tipInfoMessage = tlsRegistryManager.globalRegistry.try_get<TipInfoMessage>(GlobalEntity());
		nullptr != tipInfoMessage)
	{
		response->mutable_message_content()->mutable_error_message()->CopyFrom(*tipInfoMessage);
		tipInfoMessage->Clear();
	}

	LOG_TRACE << "Successfully processed message ID: " << request->message_content().message_id()
		<< " for player ID: " << playerId;

	///<<< END WRITING YOUR CODE
}



void CentreHandler::EnterGsSucceed(::google::protobuf::RpcController* controller, const ::EnterGameNodeSuccessRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_TRACE << "Enter Scene Node Succeed request received.";

	const auto playerId = request->player_id();

	auto playerEntityIt = tlsPlayerList.find(playerId);
	if (tlsPlayerList.end() == playerEntityIt)
	{
		LOG_ERROR << "Player not found: " << playerId;
		return;
	}

	const auto player = playerEntityIt->second;
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player not found: " << playerId;
		return;
	}

	auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
	if (!sessionPB)
	{
		LOG_ERROR << "Player session info not found for player: " << playerId;
		return;
	}

	auto& nodeIdMap = *sessionPB->mutable_node_id();
	nodeIdMap[eNodeType::SceneNodeService] = request->scene_node_id();

	PlayerLifecycleSystem::AddGameNodePlayerToGateNode(player);

	PlayerChangeRoomUtil::SetCurrentChangeSceneState(player, ChangeRoomInfoPBComponent::eEnterSucceed);
	PlayerChangeRoomUtil::ProgressSceneChangeState(player);

	LOG_INFO << "Player " << playerId << " successfully entered game node " << request->scene_node_id();

///<<< END WRITING YOUR CODE
}



void CentreHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller, const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	// Clean up previous routing information
	defer(tlsMessageContext.SetNextRouteNodeType(UINT32_MAX));
	defer(tlsMessageContext.SetNextRouteNodeId(UINT32_MAX));
	defer(tlsMessageContext.SetCurrentSessionId(kInvalidSessionId));

	// Set current session ID
	tlsMessageContext.SetCurrentSessionId(request->session_id());

	if (request->route_nodes_size() >= kMaxRouteSize)
	{
		LOG_ERROR << "Route message size exceeds maximum limit: " << request->DebugString();
		return;
	}

	if (request->route_nodes().empty())
	{
		LOG_ERROR << "Route data list is empty: " << request->DebugString();
		return;
	}

	auto& route_data = request->route_nodes(request->route_nodes_size() - 1);

	if (route_data.message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "Message ID not found: " << route_data.message_id();
		return;
	}

	const auto& messageInfo = gRpcServiceRegistry[route_data.message_id()];

	if (!messageInfo.handlerInstance)
	{
		LOG_ERROR << "Message service implementation not found for message ID: " << route_data.message_id();
		return;
	}

	const auto it = gNodeService.find(messageInfo.serviceName);
	if (it == gNodeService.end())
	{
		LOG_ERROR << "Server service handler not found for message ID: " << route_data.message_id();
		return;
	}

	const auto& service = it->second;

	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (!method)
	{
		LOG_ERROR << "Method not found: " << messageInfo.methodName;
		return;
	}

	const std::unique_ptr<google::protobuf::Message> current_node_request(service->GetRequestPrototype(method).New());

	if (!current_node_request->ParsePartialFromArray(request->body().data(),
		static_cast<int32_t>(request->body().size())))
	{
		LOG_ERROR << "Failed to parse request serialized_message: " << request->DebugString();
		return;
	}

	const std::unique_ptr<google::protobuf::Message> current_node_response(service->GetResponsePrototype(method).New());

	service->CallMethod(method, nullptr, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);

	auto* mutable_request = const_cast<::RouteMessageRequest*>(request);

	if (tlsMessageContext.GetNextRouteNodeType() == UINT32_MAX)
	{
		const auto byte_size = static_cast<int32_t>(current_node_response->ByteSizeLong());
		response->mutable_body()->resize(byte_size);
		current_node_response->SerializePartialToArray(response->mutable_body()->data(), byte_size);

		// Copy route data list to response
		for (const auto& request_data_it : request->route_nodes())
		{
			*response->add_route_nodes() = request_data_it;
		}

		response->set_session_id(tlsMessageContext.GetSessionId());
		response->set_id(request->id());
		return;
	}

	// Need to route to the next node
	auto* next_route_data = mutable_request->add_route_nodes();
	next_route_data->CopyFrom(tlsMessageContext.GetRoutingNodeInfo());
	next_route_data->mutable_node_info()->CopyFrom(gNode->GetNodeInfo());
	mutable_request->set_body(tlsMessageContext.RouteMsgBody());
	mutable_request->set_id(request->id());

	switch (tlsMessageContext.GetNextRouteNodeType())
	{
	case GateNodeService:
	{
		entt::entity gate_node_id{ tlsMessageContext.GetNextRouteNodeId() };
		auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
		if (!registry.valid(gate_node_id))
		{
			LOG_ERROR << "Gate node not found: " << tlsMessageContext.GetNextRouteNodeId();
			return;
		}
		const auto sceneNodeSession = registry.try_get<RpcSession>(gate_node_id);
		if (!sceneNodeSession)
		{
			LOG_ERROR << "Gate node not found: " << tlsMessageContext.GetNextRouteNodeId();
			return;
		}
		sceneNodeSession->RouteMessageToNode(GateRouteNodeMessageMessageId, *mutable_request);
		break;
	}
	case SceneNodeService:
	{
		entt::entity game_node_id{ tlsMessageContext.GetNextRouteNodeId() };
		auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
		if (!registry.valid(game_node_id))
		{
			LOG_ERROR << "Game node not found: " << tlsMessageContext.GetNextRouteNodeId() << ", " << request->DebugString();
			return;
		}
		const auto sceneNodeSession = registry.try_get<RpcSession>(game_node_id);
		if (!sceneNodeSession)
		{
			LOG_ERROR << "Game node not found: " << tlsMessageContext.GetNextRouteNodeId() << ", " << request->DebugString();
			return;
		}
		sceneNodeSession->RouteMessageToNode(SceneRouteNodeStringMsgMessageId, *mutable_request);
		break;
	}
	default:
	{
		LOG_ERROR << "Invalid next route node type: " << request->DebugString() << ", " << tlsMessageContext.GetNextRouteNodeType();
		break;
	}
	}
	///<<< END WRITING YOUR CODE
}



void CentreHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller, const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}



void CentreHandler::InitSceneNode(::google::protobuf::RpcController* controller, const ::InitSceneNodeRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    auto sceneNodeId = entt::entity{ request->node_id() };
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

    // Check if the scene node ID is valid
    if (!registry.valid(sceneNodeId))
    {
        LOG_ERROR << "Invalid scene node ID: " << request->node_id();
        return;
    }

	// Search for a matching client connection and register the game node
    AddMainRoomToNodeComponent(registry, sceneNodeId);

	LOG_INFO << "Add Scene node " << request->node_id() << " SceneNodeType : " << eSceneNodeType_Name(request->scene_node_type());

    if (request->scene_node_type() == eSceneNodeType::kMainSceneCrossNode)
    {
		registry.remove<MainRoomNode>(sceneNodeId);
		registry.emplace<CrossMainSceneNode>(sceneNodeId);
    	
    }
    else if (request->scene_node_type() == eSceneNodeType::kRoomNode)
    {
		registry.remove<MainRoomNode>(sceneNodeId);
		registry.emplace<RoomSceneNode>(sceneNodeId);
    }
    else if (request->scene_node_type() == eSceneNodeType::kRoomSceneCrossNode)
    {
		registry.remove<MainRoomNode>(sceneNodeId);
		registry.emplace<CrossRoomSceneNode>(sceneNodeId);
    }
///<<< END WRITING YOUR CODE
}



void CentreHandler::NodeHandshake(::google::protobuf::RpcController* controller, const ::NodeHandshakeRequest* request,
	::NodeHandshakeResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnNodeHandshake(*request, *response);
///<<< END WRITING YOUR CODE
}



