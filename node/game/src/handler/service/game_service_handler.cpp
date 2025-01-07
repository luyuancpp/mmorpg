#include "game_service_handler.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/net/InetAddress.h"

#include "game_node.h"
#include "game_common_logic/system/session_system.h"
#include "game_logic/core/network/message_system.h"
#include "game_logic/player/system/player_node_system.h"
#include "game_logic/player/system/player_scene_system.h"
#include "game_logic/scene/system/game_node_scene_system.h"
#include "handler/service/player/player_service.h"
#include "network/rpc_session.h"
#include "proto/logic/component/player_async_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "scene/system/scene_system.h"
#include "service_info/service_info.h"
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage_game.h"
#include "type_alias/player_session_type_alias.h"
#include "util/pb.h"

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

using namespace muduo::net;

///<<< END WRITING YOUR CODE
void GameServiceHandler::PlayerEnterGameNode(::google::protobuf::RpcController* controller,const ::PlayerEnterGameNodeRequest* request,
	     Empty* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling EnterGs request for player: " << request->player_id()
		<< ", centre_node_id: " << request->centre_node_id();

	// 1 清除玩家会话，处理连续顶号进入情况
	PlayerNodeSystem::RemovePlayerSessionSilently(request->player_id());

	const auto& playerList = tlsCommonLogic.GetPlayerList();
	auto playerIt = playerList.find(request->player_id());

	// 2 检查玩家是否已经在线，若在线则直接进入
	if (playerIt != playerList.end())
	{
		PlayerGameNodeEnteryInfoPBComponent enterInfo;
		enterInfo.set_centre_node_id(request->centre_node_id());
		PlayerNodeSystem::EnterGs(playerIt->second, enterInfo);
		return;
	}

	PlayerGameNodeEnteryInfoPBComponent enterInfo;
	enterInfo.set_centre_node_id(request->centre_node_id());
	auto asyncPlayerIt = tlsGame.playerNodeEntryInfoList.emplace(request->player_id(), enterInfo);

	// 3 异步加载过程中处理玩家断开连接的情况
	if (!asyncPlayerIt.second)
	{
		LOG_ERROR << "Failed to emplace player in asyncPlayerList: " << request->player_id();
		return;
	}

	// 4 玩家不在线，加入异步加载列表并尝试异步加载

	tlsGame.playerRedis->AsyncLoad(request->player_id());
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::SendMessageToPlayer(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	     NodeRouteMessageResponse* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	LOG_TRACE << "Handling message routing for session ID: " << request->head().session_id()
		<< ", message ID: " << request->body().message_id();

	const auto it = tlsSessions.find(request->head().session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "Session ID not found: " << request->head().session_id()
			<< ", message ID: " << request->body().message_id();
		return;
	}

	const auto playerIt = tlsCommonLogic.GetPlayerList().find(it->second.player_id());
	if (playerIt == tlsCommonLogic.GetPlayerList().end())
	{
		LOG_ERROR << "Player ID not found in common logic: " << it->second.player_id();
		return;
	}

	const auto& player = playerIt->second;

	if (request->body().message_id() >= gMessageInfo.size())
	{
		LOG_ERROR << "Invalid message ID: " << request->body().message_id();
		return;
	}

	const auto& messageInfo = gMessageInfo[request->body().message_id()];

	const auto serviceIt = g_player_service.find(messageInfo.serviceName);
	if (serviceIt == g_player_service.end())
	{
		LOG_ERROR << "PlayerService not found for message ID: " << request->body().message_id();
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
	if (!playerRequest->ParsePartialFromArray(request->body().body().data(), int32_t(request->body().body().size())))
	{
		LOG_ERROR << "Failed to parse request message for message ID: " << request->body().message_id();
		return;
	}

	const MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());

	serviceHandler->CallMethod(method, player, playerRequest.get(), playerResponse.get());

    response->mutable_head()->set_session_id(request->head().session_id());
    response->mutable_body()->set_message_id(request->body().message_id());

	if (Empty::GetDescriptor() == playerResponse->GetDescriptor())
	{
		return;
	}

	response->mutable_body()->set_body(playerResponse->SerializeAsString());

    ///<<< END WRITING YOUR CODE
}

void GameServiceHandler::ClientSendMessageToPlayer(::google::protobuf::RpcController* controller,const ::ClientSendMessageToPlayerRequest* request,
	     ClientSendMessageToPlayerResponse* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	if (request->message_body().message_id() >= gMessageInfo.size())
	{
		LOG_ERROR << "message_id not found " << request->message_body().message_id();
		return;
	}

	const auto& messageInfo = gMessageInfo.at(request->message_body().message_id());
	const auto serviceIt = g_player_service.find(messageInfo.serviceName);
	if (serviceIt == g_player_service.end())
	{
		LOG_ERROR << "GatePlayerService message id not found " << request->message_body().message_id();
		return;
	}

	google::protobuf::Service* service = serviceIt->second->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (nullptr == method)
	{
		LOG_ERROR << "GatePlayerService message id not found " << request->message_body().message_id();
		return;
	}

	const auto it = tlsSessions.find(request->session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "session id not found " << request->session_id() << ","
			<< " message id " << request->message_body().message_id();
		return;
	}

	const auto player = tlsCommonLogic.GetPlayer(it->second.player_id());
	if (entt::null == player)
	{
		LOG_ERROR << "GatePlayerService player not loading " << request->message_body().message_id()
			<< "player_id" << it->second.player_id();
		return;
	}

	const MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
	playerRequest->ParseFromArray(request->message_body().body().data(), static_cast<int32_t>(request->message_body().body().size()));

	const MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
	serviceIt->second->CallMethod(method, player, playerRequest.get(), playerResponse.get());

	
	response->mutable_message_body()->set_message_id(request->message_body().message_id());
	response->mutable_message_body()->set_id(request->message_body().id());
	response->set_session_id(request->session_id());

	if (Empty::GetDescriptor() == playerResponse->GetDescriptor()) {
		return;
	}

	response->mutable_message_body()->set_body(playerResponse->SerializeAsString());
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::RegisterGateNode(::google::protobuf::RpcController* controller,const ::RegisterGateNodeRequest* request,
	     Empty* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const InetAddress sessionAddr(request->rpc_client().ip(), request->rpc_client().port());

	for (const auto& [e, session] : tls.networkRegistry.view<RpcSession>().each())
	{
		if (session.connection->peerAddress().toIpPort() != sessionAddr.toIpPort())
		{
			continue;
		}

		const auto gateNodeId = tls.gateNodeRegistry.create(entt::entity{ request->gate_node_id() });
		tls.gateNodeRegistry.emplace<RpcSessionPtr>(gateNodeId, std::make_shared<RpcSessionPtr::element_type>(session.connection));
		assert(gateNodeId == entt::entity{ request->gate_node_id() });

		LOG_INFO << "Registered gate node: " << MessageToJsonString(request);

		break;
	}
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::CentreSendToPlayerViaGameNode(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	     Empty* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const auto it = tlsSessions.find(request->head().session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "session id not found " << request->head().session_id() << ","
			<< " message id " << request->body().message_id();
		return;
	}

	const auto player = tlsCommonLogic.GetPlayer(it->second.player_id());
	if (entt::null == player)
	{
		LOG_ERROR << "GatePlayerService player not loading";
		return;
	}

	::SendMessageToPlayer(request->body().message_id(), request->body(), player);
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::InvokePlayerService(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	     NodeRouteMessageResponse* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	const auto it = tlsSessions.find(request->head().session_id());
	if (it == tlsSessions.end())
	{
		LOG_ERROR << "session id not found " << request->head().session_id() << ","
			<< " message id " << request->body().message_id();
		return;
	}

	const auto player = tlsCommonLogic.GetPlayer(it->second.player_id());
	if (entt::null == player)
	{
		LOG_ERROR << "GatePlayerService player not loading";
		return;
	}

	if (request->body().message_id() >= gMessageInfo.size())
	{
		LOG_ERROR << "message_id not found " << request->body().message_id();
		return;
	}

	const auto& messageInfo = gMessageInfo[request->body().message_id()];
	const auto serviceIt = g_player_service.find(messageInfo.serviceName);
	if (serviceIt == g_player_service.end())
	{
		LOG_ERROR << "PlayerService service not found " << request->head().session_id()
			<< "," << request->body().message_id();
		return;
	}

	const auto& serviceHandler = serviceIt->second;
	google::protobuf::Service* service = serviceHandler->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (nullptr == method)
	{
		LOG_ERROR << "PlayerService method not found " << request->body().message_id();
		return;
	}

	MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
	if (!playerRequest->ParsePartialFromArray(request->body().body().data(), int32_t(request->body().body().size())))
	{
		LOG_ERROR << "ParsePartialFromArray " << request->body().message_id();
		return;
	}

	MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
	serviceHandler->CallMethod(method, player, playerRequest.get(), playerResponse.get());


    response->mutable_head()->set_session_id(request->head().session_id());
    response->mutable_body()->set_message_id(request->body().message_id());
	
    if (Empty::GetDescriptor() == playerResponse->GetDescriptor()) {
        return;
    }

	if (const auto tipInfoMessage = tls.globalRegistry.try_get<TipInfoMessage>(GlobalEntity());
		nullptr != tipInfoMessage)
	{
		response->mutable_body()->mutable_error_message()->CopyFrom(*tipInfoMessage);
		tipInfoMessage->Clear();
	}
	
	response->mutable_body()->set_body(playerResponse->SerializeAsString());
        
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,const ::RouteMsgStringRequest* request,
	     RouteMsgStringResponse* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,const ::RoutePlayerMsgStringRequest* request,
	     RoutePlayerMsgStringResponse* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::UpdateSessionDetail(::google::protobuf::RpcController* controller,const ::RegisterPlayerSessionRequest* request,
	     Empty* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerNodeSystem::RemovePlayerSession(request->player_id());

	if (const entt::entity gateNodeId{ GetGateNodeId(request->session_id()) };
		!tls.gateNodeRegistry.valid(gateNodeId))
	{
		LOG_ERROR << "Gate not found " << GetGateNodeId(request->session_id());
		return;
	}

	const auto player = tlsCommonLogic.GetPlayer(request->player_id());
	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player not found " << request->player_id();
		return;
	}

	PlayerSessionPBComponent sessionInfo;
	sessionInfo.set_player_id(request->player_id());
	tlsSessions.emplace(request->session_id(), sessionInfo);

	if (auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player); nullptr == playerNodeInfo)
	{
		tls.registry.emplace_or_replace<PlayerNodeInfoPBComponent>(player).set_gate_session_id(request->session_id());
	}
	else
	{
		playerNodeInfo->set_gate_session_id(request->session_id());
	}

	PlayerNodeSystem::HandleGameNodePlayerRegisteredAtGateNode(player);
///<<< END WRITING YOUR CODE
}

void GameServiceHandler::EnterScene(::google::protobuf::RpcController* controller,const ::Centre2GsEnterSceneRequest* request,
	     Empty* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
    //todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	auto player = tlsCommonLogic.GetPlayer(request->player_id());
	if (player == entt::null)
	{
		LOG_ERROR << "Error: Player entity not found for player_id " << request->player_id();
		return;
	}

	LOG_INFO << "Player with ID " << request->player_id() << " entering scene " << request->scene_id();

	entt::entity sceneEntity{ request->scene_id() };
	SceneUtil::EnterScene({ .scene = sceneEntity, .enter = player });
	
	PlayerSceneSystem::HandleEnterScene(player, sceneEntity);

///<<< END WRITING YOUR CODE
}

void GameServiceHandler::CreateScene(::google::protobuf::RpcController* controller,const ::CreateSceneRequest* request,
	     CreateSceneResponse* response,
	     ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

