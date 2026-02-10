
#include "gate_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "network/network_constants.h"

#include "proto/common/component/player_network_comp.pb.h"
#include <session/manager/session_manager.h>

bool shouldLogProtocolErrorForDisconnectedPlayer(int message_id)
{
	// 在这里定义所有需要记录错误日志的有效 message_id
	return true;// (message_id != 47 && message_id != 21 && message_id != 37);
}

///<<< END WRITING YOUR CODE


void GateHandler::PlayerEnterGameNode(::google::protobuf::RpcController* controller, const ::RegisterGameNodeSessionRequest* request,
	::RegisterGameNodeSessionResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto sessionIt = tlsSessionManager.sessions().find(request->session_info().session_id());
	if (sessionIt == tlsSessionManager.sessions().end())
	{
		LOG_ERROR << "Session ID not found for PlayerEnterGs, session ID: " << request->session_info().session_id();
		return;
	}
	// Handle potential asynchronous issue if the GS sends while Gate is updating GS
	sessionIt->second.SetNodeId(SceneNodeService, request->scene_node_id());
	response->mutable_session_info()->CopyFrom(request->session_info());
	LOG_INFO << "Player entered GS, session ID: " << request->session_info().session_id()
		<< ", game node ID: " << request->scene_node_id();
	///<<< END WRITING YOUR CODE
}



void GateHandler::SendMessageToPlayer(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	auto sessionIt = tlsSessionManager.sessions().find(request->header().session_id());
	if (sessionIt == tlsSessionManager.sessions().end())
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



void GateHandler::KickSessionByCentre(::google::protobuf::RpcController* controller, const ::KickSessionRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const uint64_t sessionId = request->session_id();
	const uint64_t expectedVersion = request->expected_session_version();

	auto it = tlsSessionManager.sessions().find(sessionId);
	if (it == tlsSessionManager.sessions().end()) {
		LOG_DEBUG << "Kick: session not found " << sessionId;
		return; // 幂等
	}
	const SessionInfo& info = it->second;

	// 只有版本相等时才断开，避免旧请求断开新连接
	if (info.sessionVersion != expectedVersion) {
		LOG_INFO << "Kick: version mismatch, ignore. session=" << sessionId << " current=" << info.sessionVersion
			<< " expected=" << expectedVersion;
		return;
	}

	// 执行断开（发送断开通知给 Centre，断开 socket）
	//CloseSession(sessionId);
	// Gate 会发送 GateSessionDisconnect 到 Centre (或 Centre 会收到)
	LOG_INFO << "Session ID kicked by Centre: " << request->session_id();
	///<<< END WRITING YOUR CODE
}



void GateHandler::RouteNodeMessage(::google::protobuf::RpcController* controller, const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}



void GateHandler::RoutePlayerMessage(::google::protobuf::RpcController* controller, const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}



void GateHandler::BroadcastToPlayers(::google::protobuf::RpcController* controller, const ::BroadcastToPlayersRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	for (auto&& sessionId : request->session_list())
	{
		auto sessionIt = tlsSessionManager.sessions().find(sessionId);
		if (sessionIt == tlsSessionManager.sessions().end())
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



void GateHandler::NodeHandshake(::google::protobuf::RpcController* controller, const ::NodeHandshakeRequest* request,
	::NodeHandshakeResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnNodeHandshake(*request, *response);
///<<< END WRITING YOUR CODE
}



void GateHandler::BindSessionToGate(::google::protobuf::RpcController* controller, const ::BindSessionToGateRequest* request,
	::BindSessionToGateResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	SessionInfo info;
	info.playerId = request->player_id();
	info.sessionVersion = request->session_version();
	tlsSessionManager.sessions()[request->session_id()] = info;

	response->set_session_id(request->session_id());
	response->set_session_version(request->session_version());
	response->set_player_id(request->player_id());
///<<< END WRITING YOUR CODE
}


