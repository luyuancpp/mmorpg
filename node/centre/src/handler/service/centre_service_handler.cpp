
#include "centre_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "centre_node.h"
#include "common_error_tip.pb.h"
#include "mainscene_config.h"
#include "game_common_logic/system/session_system.h"
#include "service/player_service.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/InetAddress.h"
#include "network/message_system.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "network/constants/network_constants.h"
#include "network/system/error_handling_system.h"
#include "node/comp/game_node_comp.h"
#include "pbc/login_error_tip.pb.h"
#include "player/system/player_node_system.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "scene/system/player_change_scene_system.h"
#include "scene/system/scene_system.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "service_info/player_common_service_info.h"
#include "service_info/service_info.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_loading.h"
#include "type_alias/player_redis.h"
#include "type_alias/player_session_type_alias.h"
#include "util/defer.h"
#include "util/proto_field_checker.h"
#include "util/stacktrace_system.h"

using namespace muduo;
using namespace muduo::net;

constexpr std::size_t kMaxPlayerSize{50000};

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Guid GetPlayerIDBySessionId(const uint64_t session_id)
{
	const auto session_it = tlsSessions.find(session_id);
	if (session_it == tlsSessions.end())
	{
		LOG_DEBUG << "Cannot find session ID " << session_id << GetStackTraceAsString();
		return kInvalidGuid;
	}
	return session_it->second.player_id();
}

entt::entity GetPlayerEntityBySessionId(uint64_t session_id)
{
	auto player_id = GetPlayerIDBySessionId(session_id);

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


void CentreServiceHandler::GatePlayerService(::google::protobuf::RpcController* controller,const ::GateClientMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::GateSessionDisconnect(::google::protobuf::RpcController* controller,const ::GateSessionDisconnectRequest* request,
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

	defer(tlsSessions.erase(request->session_info().session_id()));

	auto session_id = request->session_info().session_id();

	auto player_id = GetPlayerIDBySessionId(session_id);

	LOG_TRACE << "Getting player entity for session ID: " << session_id << ", player ID: " << player_id;

	const auto player_it = tlsCommonLogic.GetPlayerList().find(player_id);
	if (player_it == tlsCommonLogic.GetPlayerList().end())
	{
		LOG_TRACE << "Player not found for session ID: " << session_id << ", player ID: " << player_id;
		return ;
	}

	LOG_TRACE << "Player entity found for session ID: " << session_id << ", player ID: " << player_id;

	auto playerEntity =  player_it->second;

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
	if (playerNodeInfo == nullptr)
	{
		LOG_ERROR << "PlayerNodeInfo not found for player entity: " << tls.registry.get<Guid>(playerEntity);
		return;
	}

	if (playerNodeInfo->gate_session_id() != session_id)
	{
		LOG_ERROR << "Mismatched gate session ID for player: " << playerNodeInfo->gate_session_id()
			<< ", expected session ID: " << session_id;
		return;
	}

	const entt::entity gameNodeId{ playerNodeInfo->scene_node_id() };
	if (!tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(gameNodeId))
	{
		LOG_ERROR << "Invalid game node ID found for player: " << tls.registry.get<Guid>(playerEntity);
		return;
	}

	const auto gameNode = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<RpcSession>(gameNodeId);
	if (gameNode == nullptr)
	{
		LOG_ERROR << "RpcSession not found for game node ID: " << playerNodeInfo->scene_node_id();
		return;
	}

	const auto playerId = tls.registry.get<Guid>(playerEntity);

	LOG_INFO << "Handling disconnect for player: " << playerId;

	PlayerNodeSystem::HandleNormalExit(playerId);

///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::LoginNodeAccountLogin(::google::protobuf::RpcController* controller,const ::CentreLoginRequest* request,
	::CentreLoginResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    
	if (tlsCommonLogic.GetPlayerList().size() >= kMaxPlayerSize)
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




void CentreServiceHandler::LoginNodeEnterGame(::google::protobuf::RpcController* controller,const ::CentrePlayerGameNodeEntryRequest* request,
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

	PlayerSessionPBComponent playerSessionPBComponent;
	playerSessionPBComponent.set_player_id(clientMsgBody.player_id());
	tlsSessions.emplace(sessionId, playerSessionPBComponent);

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
		if (auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
			playerNodeInfo != nullptr)
		{
			// Handle session takeover (顶号)
			LOG_INFO << "Player reconnected: Player ID " << clientMsgBody.player_id();

			TipInfoMessage beKickTip;
			beKickTip.set_id(kLoginBeKickByAnOtherAccount);
			SendMessageToPlayer(PlayerClientCommonServiceKickPlayerMessageId, beKickTip, clientMsgBody.player_id());

			auto oldSessionId = playerNodeInfo->gate_session_id();

			defer(tlsSessions.erase(oldSessionId));

			KickSessionRequest message;
			message.set_session_id(sessionId);
			SendMessageToGateById(GateServiceKickSessionByCentreMessageId, message, GetGateNodeId(playerNodeInfo->gate_session_id()));

			playerNodeInfo->set_gate_session_id(sessionId);
		}
		else
		{
			LOG_INFO << "Existing player login: Player ID " << clientMsgBody.player_id();

			tls.registry.emplace_or_replace<PlayerNodeInfoPBComponent>(player).set_gate_session_id(sessionId);
		}

		//连续顶几次,所以用emplace_or_replace
		LOG_INFO << "Player login type: " << (tls.registry.any_of<EnterGameNodeInfoPBComponent>(player) ? "Replace" : "New");

		// Register player to gate node
		tls.registry.emplace_or_replace<EnterGameNodeInfoPBComponent>(player).set_enter_gs_type(LOGIN_REPLACE);
		PlayerNodeSystem::AddGameNodePlayerToGateNode(player);

		PlayerNodeSystem::ProcessPlayerSessionState(player);
	}


	///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::LoginNodeLeaveGame(::google::protobuf::RpcController* controller,const ::LoginNodeLeaveGameRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	//todo error
	PlayerNodeSystem::HandleNormalExit(GetPlayerIDBySessionId(tlsCommonLogic.GetSessionId()));
	//todo statistics
///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::LoginNodeSessionDisconnect(::google::protobuf::RpcController* controller,const ::GateSessionDisconnectRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	
	defer(tlsSessions.erase(request->session_info().session_id()));
	const auto player_id = GetPlayerIDBySessionId(request->session_info().session_id());
	PlayerNodeSystem::HandleNormalExit(player_id);
///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::PlayerService(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto it = tlsSessions.find(request->header().session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "Session not found: " << request->header().session_id() << " message id :" << request->message_content().message_id();
        SendErrorToClient(*request, *response, kSessionNotFound);
		return;
	}

	const auto playerId = it->second.player_id();
	const auto player = tlsCommonLogic.GetPlayer(playerId);
	if (!tls.registry.valid(player))
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
	
	if (const auto tipInfoMessage = tls.globalRegistry.try_get<TipInfoMessage>(GlobalEntity());
		nullptr != tipInfoMessage)
	{
		response->mutable_message_content()->mutable_error_message()->CopyFrom(*tipInfoMessage);
		tipInfoMessage->Clear();
	}

	LOG_TRACE << "Successfully processed message ID: " << request->message_content().message_id()
		<< " for player ID: " << playerId;

	///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::EnterGsSucceed(::google::protobuf::RpcController* controller,const ::EnterGameNodeSuccessRequest* request,
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

	auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Player session info not found for player: " << playerId;
		return;
	}

	playerNodeInfo->set_scene_node_id(request->scene_node_id());

	PlayerNodeSystem::AddGameNodePlayerToGateNode(player);

	PlayerChangeSceneUtil::SetChangeGsStatus(player, ChangeSceneInfoPBComponent::eEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(player);

	LOG_INFO << "Player " << playerId << " successfully entered game node " << request->scene_node_id();

///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	// Clean up previous routing information
	defer(tlsCommonLogic.SetNextRouteNodeType(UINT32_MAX));
	defer(tlsCommonLogic.SeNextRouteNodeId(UINT32_MAX));
	defer(tlsCommonLogic.SetCurrentSessionId(kInvalidSessionId));

	// Set current session ID
	tlsCommonLogic.SetCurrentSessionId(request->session_id());

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

	if (tlsCommonLogic.GetNextRouteNodeType() == UINT32_MAX)
	{
		const auto byte_size = static_cast<int32_t>(current_node_response->ByteSizeLong());
		response->mutable_body()->resize(byte_size);
		current_node_response->SerializePartialToArray(response->mutable_body()->data(), byte_size);

		// Copy route data list to response
		for (const auto& request_data_it : request->route_nodes())
		{
			*response->add_route_nodes() = request_data_it;
		}

		response->set_session_id(tlsCommonLogic.GetSessionId());
		response->set_id(request->id());
		return;
	}

	// Need to route to the next node
	auto* next_route_data = mutable_request->add_route_nodes();
	next_route_data->CopyFrom(tlsCommonLogic.GetRoutingNodeInfo());
	next_route_data->mutable_node_info()->CopyFrom(gNode->GetNodeInfo());
	mutable_request->set_body(tlsCommonLogic.RouteMsgBody());
	mutable_request->set_id(request->id());

	switch (tlsCommonLogic.GetNextRouteNodeType())
	{
	case GateNodeService:
	{
		entt::entity gate_node_id{ tlsCommonLogic.GetNextRouteNodeId() };
		if (!tls.GetNodeRegistry(eNodeType::GateNodeService).valid(gate_node_id))
		{
			LOG_ERROR << "Gate node not found: " << tlsCommonLogic.GetNextRouteNodeId();
			return;
		}
		const auto sceneNodeSession = tls.GetNodeRegistry(eNodeType::GateNodeService).try_get<RpcSession>(gate_node_id);
		if (!sceneNodeSession)
		{
			LOG_ERROR << "Gate node not found: " << tlsCommonLogic.GetNextRouteNodeId();
			return;
		}
		sceneNodeSession->RouteMessageToNode(GateServiceRouteNodeMessageMessageId, *mutable_request);
		break;
	}
	case SceneNodeService:
	{
		entt::entity game_node_id{ tlsCommonLogic.GetNextRouteNodeId() };
		if (!tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(game_node_id))
		{
			LOG_ERROR << "Game node not found: " << tlsCommonLogic.GetNextRouteNodeId() << ", " << request->DebugString();
			return;
		}
		const auto sceneNodeSession = tls.GetNodeRegistry(eNodeType::SceneNodeService).try_get<RpcSession>(game_node_id);
		if (!sceneNodeSession)
		{
			LOG_ERROR << "Game node not found: " << tlsCommonLogic.GetNextRouteNodeId() << ", " << request->DebugString();
			return;
		}
		sceneNodeSession->RouteMessageToNode(GameServiceRouteNodeStringMsgMessageId, *mutable_request);
		break;
	}
	default:
	{
		LOG_ERROR << "Invalid next route node type: " << request->DebugString() << ", " << tlsCommonLogic.GetNextRouteNodeType();
		break;
	}
	}
	///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::InitSceneNode(::google::protobuf::RpcController* controller,const ::InitSceneNodeRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
 
    auto sceneNodeId = entt::entity{ request->node_id() };

    // Check if the scene node ID is valid
    if (!tls.GetNodeRegistry(eNodeType::SceneNodeService).valid(sceneNodeId))
    {
        LOG_ERROR << "Invalid scene node ID: " << request->node_id();
        return;
    }

	// Search for a matching client connection and register the game node
    AddMainSceneNodeComponent(tls.GetNodeRegistry(eNodeType::SceneNodeService), sceneNodeId);

    if (request->scene_node_type() == eSceneNodeType::kMainSceneCrossNode)
    {
    	tls.GetNodeRegistry(eNodeType::SceneNodeService).remove<MainSceneNode>(sceneNodeId);
    	tls.GetNodeRegistry(eNodeType::SceneNodeService).emplace<CrossMainSceneNode>(sceneNodeId);
    	LOG_INFO << "Scene node " << request->node_id() << " updated to CrossMainSceneNode.";
    }
    else if (request->scene_node_type() == eSceneNodeType::kRoomNode)
    {
    	tls.GetNodeRegistry(eNodeType::SceneNodeService).remove<MainSceneNode>(sceneNodeId);
    	tls.GetNodeRegistry(eNodeType::SceneNodeService).emplace<RoomSceneNode>(sceneNodeId);
    	LOG_INFO << "Scene node " << request->node_id() << " updated to RoomSceneNode.";
    }
    else if (request->scene_node_type() == eSceneNodeType::kRoomSceneCrossNode)
    {
    	tls.GetNodeRegistry(eNodeType::SceneNodeService).remove<MainSceneNode>(sceneNodeId);
    	tls.GetNodeRegistry(eNodeType::SceneNodeService).emplace<CrossRoomSceneNode>(sceneNodeId);
    	LOG_INFO << "Scene node " << request->node_id() << " updated to CrossRoomSceneNode.";
    }
///<<< END WRITING YOUR CODE

}




void CentreServiceHandler::RegisterNodeSession(::google::protobuf::RpcController* controller,const ::RegisterNodeSessionRequest* request,
	::RegisterNodeSessionResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	gNode->HandleNodeRegistration(*request, *response);
///<<< END WRITING YOUR CODE

}



