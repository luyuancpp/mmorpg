
#include "gate_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "gate_codec.h"
#include "node/system/node/node.h"
#include "network/network_constants.h"
#include "network/rpc_client.h"
#include "thread_context/node_context_manager.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "proto/common/base/message.pb.h"

#include "proto/common/component/player_network_comp.pb.h"
#include <session/manager/session_manager.h>

bool shouldLogProtocolErrorForDisconnectedPlayer(int message_id)
{
	return true;
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
	// Resolve protocol node_id -> local registry entity before binding the session.
	if (const auto sceneNodeEntity = NodeUtils::FindNodeEntityByNodeId(SceneNodeService, request->scene_node_id()); sceneNodeEntity)
	{
		sessionIt->second.SetNodeId(SceneNodeService, entt::to_integral(*sceneNodeEntity));
	}
	else
	{
		LOG_ERROR << "PlayerEnterGs: scene node not found in registry, session_id="
				  << request->session_info().session_id()
				  << ", scene_node_id=" << request->scene_node_id();
	}
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
			LOG_WARN << "Connection ID not found for PlayerMessage, session ID: " << request->header().session_id() << ", message ID:" << request->message_content().message_id();
		}
		return;
	}
	GetGateCodec().send(sessionIt->second.conn, request->message_content());
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
		GetGateCodec().send(targetSessionIt->second.conn, outbound);
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

	const auto *nextClient = nextRegistry.try_get<RpcClientPtr>(nextNodeEntity);
	if (!nextClient || !*nextClient) {
		LOG_ERROR << "RoutePlayerMessage: next node RpcClient missing, node_type=" << nextNode.node_type()
				  << ", node_id=" << nextNode.node_id() << ", player_id=" << playerId;
		return;
	}

	switch (nextNode.node_type()) {
	case eNodeType::SceneNodeService:
		(*nextClient)->CallRemoteMethod(SceneRoutePlayerStringMsgMessageId, nextRequest);
		return;
	case eNodeType::GateNodeService:
		(*nextClient)->CallRemoteMethod(GateRoutePlayerMessageMessageId, nextRequest);
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
	auto sendToSession = [&](uint32_t sessionId)
	{
		auto sessionIt = tlsSessionManager.sessions().find(sessionId);
		if (sessionIt == tlsSessionManager.sessions().end())
		{
			if (shouldLogProtocolErrorForDisconnectedPlayer(request->message_content().message_id()))
			{
				LOG_DEBUG << "Connection ID not found for BroadCast2PlayerMessage, session ID: " << sessionId << ", message ID:" << request->message_content().message_id();
			}
			return;
		}
		GetGateCodec().send(sessionIt->second.conn, request->message_content());
	};

	if (!request->session_bitmap().empty())
	{
		const uint32_t base = request->session_bitmap_base();
		const auto &bitmap = request->session_bitmap();
		for (size_t i = 0; i < bitmap.size(); ++i)
		{
			const uint8_t byte = static_cast<uint8_t>(bitmap[i]);
			for (int bit = 0; bit < 8; ++bit)
			{
				if (byte & (1 << bit))
				{
					sendToSession(base + static_cast<uint32_t>(i * 8 + bit));
				}
			}
		}
	}
	else
	{
		for (auto &&sessionId : request->session_list())
		{
			sendToSession(sessionId);
		}
	}
	///<<< END WRITING YOUR CODE
}

void GateHandler::BroadcastToScene(::google::protobuf::RpcController* controller, const ::BroadcastToSceneRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const uint64_t sceneId = request->scene_id();
	for (auto &[sessionId, info] : tlsSessionManager.sessions())
	{
		if (info.sceneId == sceneId && info.conn)
		{
			GetGateCodec().send(info.conn, request->message_content());
		}
	}
	///<<< END WRITING YOUR CODE
}

void GateHandler::BroadcastToAll(::google::protobuf::RpcController* controller, const ::BroadcastToAllRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	for (auto &[sessionId, info] : tlsSessionManager.sessions())
	{
		if (info.conn)
		{
			GetGateCodec().send(info.conn, request->message_content());
		}
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

void GateHandler::GmGracefulShutdown(::google::protobuf::RpcController* controller, const ::GmGracefulShutdownRequest* request,
	::GmGracefulShutdownResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	LOG_INFO << "GM graceful shutdown requested by operator=" << request->operator_()
			 << " reason=" << request->reason();

	auto& sessions = tlsSessionManager.sessions();
	uint32_t count = 0;
	for (auto& [sessionId, info] : sessions)
	{
		if (info.conn)
		{
			info.conn->forceClose();
			++count;
		}
	}

	LOG_INFO << "GM graceful shutdown: closed " << count << " client sessions, scheduling node shutdown.";
	response->set_affected_count(count);

	// Reply to GM first, then shutdown.
	done->Run();

	gNode->Shutdown();
	return;

///<<< END WRITING YOUR CODE
}
