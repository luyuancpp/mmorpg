#include "gate_service_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "network/network_constants.h"
#include "thread_local/storage_gate.h"
#include "util/pb.h"

#include "proto/logic/component/player_network_comp.pb.h"

bool shouldLogProtocolErrorForDisconnectedPlayer(int message_id)
{
	// 在这里定义所有需要记录错误日志的有效 message_id
	return (message_id != 47 && message_id != 21 && message_id != 37);
}

///<<< END WRITING YOUR CODE
void GateServiceHandler::RegisterGame(::google::protobuf::RpcController* controller,const ::RegisterGameNodeRequest* request,
	     ::Empty* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	// Centre server notification
	entt::entity requestGameNodeId{ request->scene_node_id() };
	if (tls.sceneNodeRegistry.valid(requestGameNodeId))
	{
		LOG_ERROR << "Game node reconnect";
		return;
	}
	Destroy(tls.sceneNodeRegistry, requestGameNodeId);
	auto gameNodeId = tls.sceneNodeRegistry.create(requestGameNodeId);
	if (gameNodeId != requestGameNodeId)
	{
		LOG_ERROR << "Create game node failed";
		return;
	}

	InetAddress gameServiceAddr(request->rpc_server().ip(), request->rpc_server().port());
	auto& gameNode = tls.sceneNodeRegistry.emplace<RpcClientPtr>(gameNodeId,
		std::make_unique<RpcClientPtr::element_type>(
			EventLoop::getEventLoopOfCurrentThread(),
			gameServiceAddr));
	gameNode->registerService(&gGateNode->GetServiceHandler());
	gameNode->connect();

	LOG_INFO << "On game register: " << MessageToJsonString(request);
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::UnRegisterGame(::google::protobuf::RpcController* controller,const ::UnregisterGameNodeRequest* request,
	     ::Empty* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	entt::entity requestGameNodeId{ request->scene_node_id() };
	Destroy(tls.sceneNodeRegistry, requestGameNodeId);
	LOG_INFO << "On game unregister: " << MessageToJsonString(request);
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::PlayerEnterGameNode(::google::protobuf::RpcController* controller,const ::RegisterGameNodeSessionRequest* request,
	     ::RegisterGameNodeSessionResponse* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto sessionIt = tls_gate.sessions().find(request->session_info().session_id());
	if (sessionIt == tls_gate.sessions().end())
	{
		LOG_ERROR << "Session ID not found for PlayerEnterGs, session ID: " << request->session_info().session_id();
		return;
	}
	// Handle potential asynchronous issue if the GS sends while Gate is updating GS
	sessionIt->second.gameNodeId = request->scene_node_id();
	response->mutable_session_info()->set_session_id(request->session_info().session_id());
	LOG_INFO << "Player entered GS, session ID: " << request->session_info().session_id()
		<< ", game node ID: " << request->scene_node_id();
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::SendMessageToPlayer(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	     ::Empty* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto sessionIt = tls_gate.sessions().find(request->header().session_id());
	if (sessionIt == tls_gate.sessions().end())
	{
		if (shouldLogProtocolErrorForDisconnectedPlayer(request->message_content().message_id()))
		{
			LOG_ERROR << "Connection ID not found for PlayerMessage, session ID: " << request->header().session_id() << ", message ID:" << request->message_content().message_id();
		}

		return;
	}
	gGateNode->SendMessageToClient(sessionIt->second.conn, request->message_content());
	//LOG_TRACE << "Player message routed, session ID: " << request->head().session_id();
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::KickSessionByCentre(::google::protobuf::RpcController* controller,const ::KickSessionRequest* request,
	     ::Empty* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	Destroy(tls.sceneRegistry, entt::entity{ request->session_id() });
	LOG_INFO << "Session ID kicked by Centre: " << request->session_id();
	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::RouteNodeMessage(::google::protobuf::RpcController* controller,const ::RouteMessageRequest* request,
	     ::RouteMessageResponse* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::RoutePlayerMessage(::google::protobuf::RpcController* controller,const ::RoutePlayerMessageRequest* request,
	     ::RoutePlayerMessageResponse* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	///<<< END WRITING YOUR CODE
}

void GateServiceHandler::BroadcastToPlayers(::google::protobuf::RpcController* controller,const ::BroadcastToPlayersRequest* request,
	     ::Empty* response,
	     ::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	for (auto&& sessionId : request->session_list())
	{
		auto sessionIt = tls_gate.sessions().find(sessionId);
		if (sessionIt == tls_gate.sessions().end())
		{
			if (shouldLogProtocolErrorForDisconnectedPlayer(request->message_content().message_id()))
			{
				LOG_ERROR << "Connection ID not found for BroadCast2PlayerMessage, session ID: " << sessionId << ", message ID:" << request->message_content().message_id();
			}

			continue;
		}
		gGateNode->SendMessageToClient(sessionIt->second.conn, request->message_content());
		//LOG_TRACE << "Broadcast message sent to session ID: " << sessionId;
	}
	///<<< END WRITING YOUR CODE
}

