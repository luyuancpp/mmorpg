#include "centre_service_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "centre_node.h"
#include "mainscene_config.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "constants/tips_id_constants.h"
#include "proto/logic/constants/node.pb.h"
#include "handler/service/player_service.h"
#include "handler/service/register_handler.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/InetAddress.h"
#include "comp/game_node.h"
#include "network/gate_session.h"
#include "network/rpc_msg_route.h"
#include "service/game_service_service.h"
#include "service/gate_service_service.h"
#include "service/service.h"
#include "game_logic/scene/util/player_change_scene.h"
#include "game_logic/player/util/player_node_util.h"
#include "game_logic/network/message_system.h"
#include "scene/util/scene_util.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_loading.h"
#include "type_alias/player_redis.h"
#include "type_alias/player_session_type_alias.h"
#include "network/rpc_session.h"
#include "util/defer.h"
#include "util/pb.h"

using namespace muduo;
using namespace muduo::net;

constexpr std::size_t kMaxPlayerSize{50000};

Guid GetPlayerIdBySessionId(const uint64_t session_id)
{
	const auto session_it = tlsSessions.find(session_id);
	if (session_it == tlsSessions.end())
	{
		LOG_ERROR << "Cannot find session ID " << session_id;
		return kInvalidGuid;
	}
	return session_it->second.player_id();
}

entt::entity GetPlayerEntityBySessionId(uint64_t session_id)
{
	auto player_id = GetPlayerIdBySessionId(session_id);

	LOG_TRACE << "Getting player entity for session ID: " << session_id << ", player ID: " << player_id;

	const auto player_it = tlsCommonLogic.GetPlayerList().find(player_id);
	if (player_it == tlsCommonLogic.GetPlayerList().end())
	{
		LOG_ERROR << "Player not found for session ID: " << session_id << ", player ID: " << player_id;
		return entt::null;
	}

	LOG_TRACE << "Player entity found for session ID: " << session_id << ", player ID: " << player_id;

	return player_it->second;
}

///<<< END WRITING YOUR CODE
void CentreServiceHandler::RegisterGame(::google::protobuf::RpcController* controller,
	const ::RegisterGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	// Log that we have received a RegisterGame request
	LOG_INFO << "Received RegisterGame request.";

	// Parse client and server addresses
	const InetAddress clientAddr(request->rpc_client().ip(), request->rpc_client().port());
	const InetAddress serverAddr(request->rpc_server().ip(), request->rpc_server().port());
	const entt::entity gameNodeId{ request->game_node_id() };

	// Log client and server addresses along with game node ID
	LOG_INFO << "Client address: " << clientAddr.toIpPort();
	LOG_INFO << "Server address: " << serverAddr.toIpPort();
	LOG_INFO << "Game node ID: " << request->game_node_id();

	// Search for a matching client connection and register the game node
	bool clientFound = false;
	for (const auto& [entity, session] : tls.networkRegistry.view<RpcSession>().each())
	{
		if (session.conn_->peerAddress().toIpPort() == clientAddr.toIpPort())
		{
			LOG_INFO << "Found matching client connection for registration.";
			clientFound = true;

			const auto newGameNode = tls.gameNodeRegistry.create(gameNodeId);
			if (newGameNode == entt::null)
			{
				LOG_ERROR << "Failed to create game node " << request->game_node_id();
				return;
			}

			// Create game node pointer and add components
			auto gameNodePtr = std::make_shared<RpcSessionPtr::element_type>(session.conn_);
			AddMainSceneNodeComponent(tls.gameNodeRegistry, newGameNode);
			tls.gameNodeRegistry.emplace<RpcSessionPtr>(newGameNode, gameNodePtr);
			tls.gameNodeRegistry.emplace<InetAddress>(newGameNode, serverAddr);
			LOG_INFO << "Game node " << request->game_node_id() << " created and registered.";
			break;
		}
	}

	// If no matching client connection found, log a warning and return
	if (!clientFound)
	{
		LOG_WARN << "Client not found for registration. Ignoring request.";
		return;
	}

	LOG_INFO << "Game registered: " << MessageToJsonString(request);

	// Update game node type based on server type
	if (request->server_type() == eGameNodeType::kMainSceneCrossNode)
	{
		tls.gameNodeRegistry.remove<MainSceneNode>(gameNodeId);
		tls.gameNodeRegistry.emplace<CrossMainSceneNode>(gameNodeId);
		LOG_INFO << "Game node " << request->game_node_id() << " updated to CrossMainSceneNode.";
	}
	else if (request->server_type() == eGameNodeType::kRoomNode)
	{
		tls.gameNodeRegistry.remove<MainSceneNode>(gameNodeId);
		tls.gameNodeRegistry.emplace<RoomSceneNode>(gameNodeId);
		LOG_INFO << "Game node " << request->game_node_id() << " updated to RoomSceneNode.";
	}
	else if (request->server_type() == eGameNodeType::kRoomSceneCrossNode)
	{
		tls.gameNodeRegistry.remove<MainSceneNode>(gameNodeId);
		tls.gameNodeRegistry.emplace<CrossRoomSceneNode>(gameNodeId);
		LOG_INFO << "Game node " << request->game_node_id() << " updated to CrossRoomSceneNode.";
	}

	// Broadcast game registration to all gates
	for (auto gate : tls.gateNodeRegistry.view<RpcSessionPtr>())
	{
		gCentreNode->BroadCastRegisterGameToGate(gameNodeId, gate);
		LOG_INFO << "Broadcasted game registration for node " << request->game_node_id() << " to gate.";
	}
	///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::RegisterGate(::google::protobuf::RpcController* controller,
	const ::RegisterGateRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
// Extract client address and gate ID from the request
	InetAddress clientAddress(request->rpc_client().ip(), request->rpc_client().port());
	entt::entity gateId{ request->gate_node_id() };

	// Search for a matching client connection in network registry
	bool foundMatchingClient = false;
	for (const auto& [entity, session] : tls.networkRegistry.view<RpcSession>().each())
	{
		if (session.conn_->peerAddress().toIpPort() == clientAddress.toIpPort())
		{
			// Found matching client connection, create gate node
			const auto createdGateId = tls.gateNodeRegistry.create(gateId);
			if (createdGateId != gateId)
			{
				LOG_ERROR << "Failed to create gate " << request->gate_node_id();
				return;
			}

			// Register gate node and associate with client session
			tls.gateNodeRegistry.emplace<RpcSessionPtr>(gateId,
				std::make_shared<RpcSessionPtr::element_type>(session.conn_));
			LOG_INFO << "Gate registered: " << MessageToJsonString(request);
			foundMatchingClient = true;
			break;
		}
	}

	if (!foundMatchingClient)
	{
		LOG_ERROR << "No matching client session found for gate registration: " << MessageToJsonString(request);
		return;
	}

	// Broadcast registration to all game nodes
	for (const auto& entity : tls.gameNodeRegistry.view<RpcSessionPtr>())
	{
		gCentreNode->BroadCastRegisterGameToGate(entity, gateId);
		LOG_DEBUG << "Broadcasted gate registration to game node: " << entt::to_integral(entity) << ", gate ID: " << request->gate_node_id();
	}
    ///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::GatePlayerService(::google::protobuf::RpcController* controller,
	const ::GateClientMessageRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::GateSessionDisconnect(::google::protobuf::RpcController* controller,
	const ::GateSessionDisconnectRequest* request,
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

	defer(tlsSessions.erase(request->session_id()));

	const auto playerEntity = GetPlayerEntityBySessionId(request->session_id());
	if (playerEntity == entt::null)
	{
		LOG_ERROR << "Player entity not found for session ID: " << request->session_id();
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
	if (playerNodeInfo == nullptr)
	{
		LOG_ERROR << "PlayerNodeInfo not found for player entity: " << tls.registry.get<Guid>(playerEntity);
		return;
	}

	if (playerNodeInfo->gate_session_id() != request->session_id())
	{
		LOG_ERROR << "Mismatched gate session ID for player: " << playerNodeInfo->gate_session_id()
			<< ", expected session ID: " << request->session_id();
		return;
	}

	const entt::entity gameNodeId{ playerNodeInfo->game_node_id() };
	if (!tls.gameNodeRegistry.valid(gameNodeId))
	{
		LOG_ERROR << "Invalid game node ID found for player: " << tls.registry.get<Guid>(playerEntity);
		return;
	}

	const auto gameNode = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeId);
	if (gameNode == nullptr)
	{
		LOG_ERROR << "RpcSessionPtr not found for game node ID: " << playerNodeInfo->game_node_id();
		return;
	}

	const auto playerId = tls.registry.get<Guid>(playerEntity);

	LOG_INFO << "Handling disconnect for player: " << playerId;

	GameNodeDisconnectRequest disconnectRequest;
	disconnectRequest.set_player_id(playerId);
	(*gameNode)->CallMethod(GameServiceDisconnectMsgId, disconnectRequest);

	PlayerNodeUtil::HandlePlayerLeave(playerId);
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::LsLoginAccount(::google::protobuf::RpcController* controller,
	const ::LoginRequest* request,
	::LoginResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    
	if (tlsCommonLogic.GetPlayerList().size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是账号排队，还是角色排队>???
		response->mutable_error()->set_id(kRetLoginAccountPlayerFull);
		return;
	}
	//排队
    //todo 排队队列里面有同一个人的两个链接
	//如果不是同一个登录服务器,踢掉已经登录的账号
	//告诉客户端登录中
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::OnLoginEnterGame(::google::protobuf::RpcController* controller,
	const ::EnterGameL2Ctr* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	//顶号
	//todo正常或者顶号进入场景
	//todo 断线重连进入场景，断线重连分时间
	//todo 返回login session 删除了后能返回客户端吗?数据流程对吗
	auto& clientMsgBody = request->client_msg_body();
	auto sessionId = request->session_info().session_id();

	LOG_INFO << "Player login attempt: Player ID " << clientMsgBody.player_id() << ", Session ID " << sessionId;

	PlayerSessionInfo sessionInfo;
	sessionInfo.set_player_id(clientMsgBody.player_id());
	tlsSessions.emplace(sessionId, sessionInfo);

	// TODO: Disconnect old connection
	// todo 快速登录两次
	if (const auto playerIt = tlsCommonLogic.GetPlayerList().find(clientMsgBody.player_id());
		playerIt == tlsCommonLogic.GetPlayerList().end())
	{
		LOG_INFO << "New player login: Player ID " << clientMsgBody.player_id();

		tls.globalRegistry.get<PlayerLoadingInfoList>(GlobalEntity()).emplace(
			clientMsgBody.player_id(), *request);
		tls.globalRegistry.get<PlayerRedis>(GlobalEntity())->AsyncLoad(clientMsgBody.player_id());
	}
	else
	{
		auto player = playerIt->second;
		//顶号,断线重连 记得gate 删除 踢掉老gate,但是是同一个gate就不用了
		//顶号的时候已经在场景里面了,不用再进入场景了
		//todo换场景的过程中被顶了
		//告诉账号被顶
		//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
		//区分顶号和断线重连
		// 
		// Handle session takeover (顶号) or reconnect scenario
		if (auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
			playerNodeInfo != nullptr)
		{
			// Handle session takeover (顶号)
			LOG_INFO << "Player reconnected: Player ID " << clientMsgBody.player_id();

			extern const uint32_t ClientPlayerCommonServiceBeKickMsgId;
			TipMessage beKickTip;
			beKickTip.mutable_tip_info()->set_id(kRetLoginBeKickByAnOtherAccount);
			SendMessageToPlayer(ClientPlayerCommonServiceBeKickMsgId, beKickTip, clientMsgBody.player_id());

			defer(tlsSessions.erase(playerNodeInfo->gate_session_id()));

			GateNodeKickConnRequest message;
			message.set_session_id(sessionId);
			SendToGateById(GateServiceKickConnByCentreMsgId, message, GetGateNodeId(playerNodeInfo->gate_session_id()));

			playerNodeInfo->set_gate_session_id(sessionId);
		}
		else
		{
			LOG_INFO << "Existing player login: Player ID " << clientMsgBody.player_id();

			tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(sessionId);
		}

		//连续顶几次,所以用emplace_or_replace
		LOG_INFO << "Player login type: " << (tls.registry.any_of<EnterGsFlag>(player) ? "Replace" : "New");

		// Register player to gate node
		tls.registry.emplace_or_replace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_REPLACE);
		PlayerNodeUtil::RegisterPlayerToGateNode(player);
	}

	///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::LsLeaveGame(::google::protobuf::RpcController* controller,
	const ::CtrlLsLeaveGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerNodeUtil::HandlePlayerLeave(GetPlayerIdBySessionId(tlsCommonLogic.session_id()));
	//todo statistics
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::LsDisconnect(::google::protobuf::RpcController* controller,
	const ::CtrlLsDisconnectRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	defer(Destroy(tls.registry, entt::entity{ tlsCommonLogic.session_id() }));
	const auto player_id = GetPlayerIdBySessionId(tlsCommonLogic.session_id());
	PlayerNodeUtil::HandlePlayerLeave(player_id);
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::GsPlayerService(::google::protobuf::RpcController* controller,
	const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto it = tlsSessions.find(request->head().session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "Session not found: " << request->head().session_id();
		return;
	}

	const auto playerId = it->second.player_id();
	const auto player = tlsCommonLogic.GetPlayer(playerId);
	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player not found: " << playerId;
		return;
	}

	if (request->body().message_id() >= g_message_info.size())
	{
		LOG_ERROR << "Message ID not found: " << request->body().message_id();
		return;
	}

	const auto& message_info = g_message_info.at(request->body().message_id());

	const auto service_it = g_player_service.find(message_info.service);
	if (service_it == g_player_service.end())
	{
		LOG_ERROR << "Player service not found: " << message_info.service;
		return;
	}

	const auto& service_handler = service_it->second;
	google::protobuf::Service* service = service_handler->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
	if (!method)
	{
		LOG_ERROR << "Method not found: " << message_info.method;
		// TODO: Handle client error
		return;
	}

	const MessagePtr player_request(service->GetRequestPrototype(method).New());
	if (!player_request->ParsePartialFromArray(request->body().body().data(),
		request->body().body().size()))
	{
		LOG_ERROR << "Failed to parse request for message ID: " << request->body().message_id();
		// TODO: Handle client error
		return;
	}

	const MessagePtr player_response(service->GetResponsePrototype(method).New());

	// Call method on player service handler
	service_handler->CallMethod(method, player, get_pointer(player_request), get_pointer(player_response));

	// If response is nullptr, no need to send a reply
	if (!response)
	{
		return;
	}

	response->mutable_head()->set_session_id(request->head().session_id());
	const int32_t byte_size = response->ByteSizeLong();
	response->mutable_body()->mutable_body()->resize(byte_size);
	if (!response->SerializePartialToArray(response->mutable_body()->mutable_body()->data(), byte_size))
	{
		LOG_ERROR << "Failed to serialize response for message ID: " << request->body().message_id();
		// TODO: Handle message serialization error
		return;
	}

	response->mutable_body()->set_message_id(request->body().message_id());

	LOG_TRACE << "Successfully processed message ID: " << request->body().message_id()
		<< " for player ID: " << playerId;

	///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::EnterGsSucceed(::google::protobuf::RpcController* controller,
	const ::EnterGameNodeSucceedRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "EnterGsSucceed request received.";

	const auto playerId = request->player_id();
	const auto player = tlsCommonLogic.GetPlayer(playerId);
	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player not found: " << playerId;
		return;
	}

	auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Player session info not found for player: " << playerId;
		return;
	}

	playerNodeInfo->set_game_node_id(request->game_node_id());

	PlayerNodeUtil::RegisterPlayerToGateNode(player);

	PlayerChangeSceneSystem::SetChangeGsStatus(player, CentreChangeSceneInfo::eEnterGsSceneSucceed);
	PlayerChangeSceneSystem::ProcessChangeSceneQueue(player);

	LOG_INFO << "Player " << playerId << " successfully entered game node " << request->game_node_id();

///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	// Clean up previous routing information
	defer(tlsCommonLogic.set_next_route_node_type(UINT32_MAX));
	defer(tlsCommonLogic.set_next_route_node_id(UINT32_MAX));
	defer(tlsCommonLogic.set_current_session_id(kInvalidSessionId));

	// Set current session ID
	tlsCommonLogic.set_current_session_id(request->session_id());

	if (request->route_data_list_size() >= kMaxRouteSize)
	{
		LOG_ERROR << "Route message size exceeds maximum limit: " << request->DebugString();
		return;
	}

	if (request->route_data_list().empty())
	{
		LOG_ERROR << "Route data list is empty: " << request->DebugString();
		return;
	}

	auto& route_data = request->route_data_list(request->route_data_list_size() - 1);

	if (route_data.message_id() >= g_message_info.size())
	{
		LOG_ERROR << "Message ID not found: " << route_data.message_id();
		return;
	}

	const auto& message_info = g_message_info[route_data.message_id()];

	if (!message_info.service_impl_instance_)
	{
		LOG_ERROR << "Message service implementation not found for message ID: " << route_data.message_id();
		return;
	}

	const auto it = g_server_service.find(message_info.service);
	if (it == g_server_service.end())
	{
		LOG_ERROR << "Server service handler not found for message ID: " << route_data.message_id();
		return;
	}

	const auto& service = it->second;

	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.method);
	if (!method)
	{
		LOG_ERROR << "Method not found: " << message_info.method;
		return;
	}

	const std::unique_ptr<google::protobuf::Message> current_node_request(service->GetRequestPrototype(method).New());

	if (!current_node_request->ParsePartialFromArray(request->body().data(),
		static_cast<int32_t>(request->body().size())))
	{
		LOG_ERROR << "Failed to parse request body: " << request->DebugString();
		return;
	}

	const std::unique_ptr<google::protobuf::Message> current_node_response(service->GetResponsePrototype(method).New());

	service->CallMethod(method, nullptr, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);

	auto* mutable_request = const_cast<::RouteMsgStringRequest*>(request);

	if (tlsCommonLogic.next_route_node_type() == UINT32_MAX)
	{
		const auto byte_size = static_cast<int32_t>(current_node_response->ByteSizeLong());
		response->mutable_body()->resize(byte_size);
		current_node_response->SerializePartialToArray(response->mutable_body()->data(), byte_size);

		// Copy route data list to response
		for (const auto& request_data_it : request->route_data_list())
		{
			*response->add_route_data_list() = request_data_it;
		}

		response->set_session_id(tlsCommonLogic.session_id());
		response->set_id(request->id());
		return;
	}

	// Need to route to the next node
	auto* next_route_data = mutable_request->add_route_data_list();
	next_route_data->CopyFrom(tlsCommonLogic.route_data());
	next_route_data->mutable_node_info()->CopyFrom(gCentreNode->GetNodeInfo());
	mutable_request->set_body(tlsCommonLogic.route_msg_body());
	mutable_request->set_id(request->id());

	switch (tlsCommonLogic.next_route_node_type())
	{
	case kGateNode:
	{
		entt::entity gate_node_id{ tlsCommonLogic.next_route_node_id() };
		if (!tls.gateNodeRegistry.valid(gate_node_id))
		{
			LOG_ERROR << "Gate node not found: " << tlsCommonLogic.next_route_node_id();
			return;
		}
		const auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_node_id);
		if (!gate_node)
		{
			LOG_ERROR << "Gate node not found: " << tlsCommonLogic.next_route_node_id();
			return;
		}
		(*gate_node)->Route2Node(GateServiceRouteNodeStringMsgMsgId, *mutable_request);
		break;
	}
	case kGameNode:
	{
		entt::entity game_node_id{ tlsCommonLogic.next_route_node_id() };
		if (!tls.gameNodeRegistry.valid(game_node_id))
		{
			LOG_ERROR << "Game node not found: " << tlsCommonLogic.next_route_node_id() << ", " << request->DebugString();
			return;
		}
		const auto game_node = tls.gameNodeRegistry.try_get<RpcSessionPtr>(game_node_id);
		if (!game_node)
		{
			LOG_ERROR << "Game node not found: " << tlsCommonLogic.next_route_node_id() << ", " << request->DebugString();
			return;
		}
		(*game_node)->Route2Node(GameServiceRouteNodeStringMsgMsgId, *mutable_request);
		break;
	}
	default:
	{
		LOG_ERROR << "Invalid next route node type: " << request->DebugString() << ", " << tlsCommonLogic.next_route_node_type();
		break;
	}
	}
	///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void CentreServiceHandler::UnRegisterGame(::google::protobuf::RpcController* controller,
	const ::UnRegisterGameRequest* request,
	::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    for (const auto& [e, gate_node]: tls.gateNodeRegistry.view<RpcSessionPtr>().each())
    {
		UnRegisterGameRequest message;
		message.set_game_node_id(request->game_node_id());
        gate_node->Send(GateServiceRegisterGameMsgId, message);
    }
///<<< END WRITING YOUR CODE
}

