
#include "gate_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "gate_globals.h"
#include "node/system/node/node.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "threading/node_context_manager.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "proto/common/base/message.pb.h"

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
	gGateCodec->send(sessionIt->second.conn, request->message_content());
	//LOG_TRACE << "Player message routed, session ID: " << request->head().session_id();
	///<<< END WRITING YOUR CODE
}

void GateHandler::KickSessionByCentre(::google::protobuf::RpcController* controller, const ::KickSessionRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	// DEPRECATED: Centre has been decommissioned. Kick is now handled via Kafka KickPlayer GateCommand.
	// This RPC handler is kept as a no-op stub for proto ABI compatibility.
	LOG_DEBUG << "KickSessionByCentre called but Centre is decommissioned. session_id=" << request->session_id();
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
	if (!request || !response) {
		return;
	}

	response->set_body(request->body());
	response->mutable_player_info()->CopyFrom(request->player_info());
	response->mutable_node_list()->CopyFrom(request->node_list());

	const Guid playerId = request->player_info().player_id();
	if (request->node_list_size() == 0) {
		auto targetSessionIt = tlsSessionManager.sessions().end();
		for (auto it = tlsSessionManager.sessions().begin(); it != tlsSessionManager.sessions().end(); ++it) {
			if (it->second.playerId == playerId) {
				targetSessionIt = it;
				break;
			}
		}

		if (targetSessionIt == tlsSessionManager.sessions().end()) {
			LOG_WARN << "RoutePlayerMessage: target player session not found on gate, player_id=" << playerId;
			return;
		}

		ClientRequest routedClientRequest;
		if (!routedClientRequest.ParseFromString(request->body())) {
			LOG_ERROR << "RoutePlayerMessage: failed to parse ClientRequest body, player_id=" << playerId;
			return;
		}

		MessageContent outbound;
		outbound.set_id(routedClientRequest.id());
		outbound.set_message_id(routedClientRequest.message_id());
		outbound.set_serialized_message(routedClientRequest.body());
		gGateCodec->send(targetSessionIt->second.conn, outbound);
		return;
	}

	RoutePlayerMessageRequest nextRequest(*request);
	nextRequest.mutable_node_list()->DeleteSubrange(0, 1);

	const auto& nextNode = request->node_list(0);
	const entt::entity nextNodeEntity{ nextNode.node_id() };
	auto& nextRegistry = tlsNodeContextManager.GetRegistry(nextNode.node_type());
	if (!nextRegistry.valid(nextNodeEntity)) {
		LOG_ERROR << "RoutePlayerMessage: next node not found, node_type=" << nextNode.node_type()
				  << ", node_id=" << nextNode.node_id() << ", player_id=" << playerId;
		return;
	}

	const auto nextSession = nextRegistry.try_get<RpcSession>(nextNodeEntity);
	if (!nextSession) {
		LOG_ERROR << "RoutePlayerMessage: next node session missing, node_type=" << nextNode.node_type()
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
		LOG_ERROR << "RoutePlayerMessage: unsupported next node_type=" << nextNode.node_type()
				  << ", node_id=" << nextNode.node_id() << ", player_id=" << playerId;
		return;
	}
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
		gGateCodec->send(sessionIt->second.conn, request->message_content());
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
