#include "message_system.h"
#include "muduo/base/Logging.h"
#include "comp/scene.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "network/gate_session.h"
#include "network/rpc_session.h"
#include "service/game_service_service.h"
#include "service/gate_service_service.h"
#include "service/service.h"
#include "thread_local/storage.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_define/type_define.h"

void SendToGs(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
	entt::entity gameNodeId{ nodeId };
	if (!tls.gameNodeRegistry.valid(gameNodeId))
	{
		LOG_ERROR << "Game node not found -> " << nodeId;
		return;
	}

	const auto rpcSession = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeId);
	if (!rpcSession)
	{
		LOG_ERROR << "RpcSession not found for game node -> " << nodeId;
		return;
	}

	(*rpcSession)->Send(messageId, message);
}

void SendToGsPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		return;
	}

	entt::entity gameNodeId{ playerNodeInfo->game_node_id() };
	if (!tls.gameNodeRegistry.valid(gameNodeId))
	{
		LOG_ERROR << "Game node not found for player -> " << playerNodeInfo->game_node_id();
		return;
	}

	auto rpcSession = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeId);
	if (!rpcSession)
	{
		LOG_ERROR << "RpcSession not found for game node -> " << playerNodeInfo->game_node_id();
		return;
	}

	NodeRouteMessageRequest request;
	const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_body()->mutable_body()->resize(byteSize);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize);
	request.mutable_body()->set_message_id(messageId);
	request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());
	(*rpcSession)->Send(GameServiceSend2PlayerMsgId, request);
}

void SendToGsPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToGsPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToPlayerViaGs(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToPlayerViaGs(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToPlayerViaGs(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		return;
	}

	entt::entity gameNodeId{ playerNodeInfo->game_node_id() };
	if (tls.gameNodeRegistry.valid(gameNodeId))
	{
		LOG_ERROR << "Game node not found for player -> " << playerNodeInfo->game_node_id();
		return;
	}

	auto rpcSession = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeId);
	if (!rpcSession)
	{
		LOG_ERROR << "RpcSession not found for game node -> " << playerNodeInfo->game_node_id();
		return;
	}

	NodeRouteMessageRequest request;
	const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_body()->mutable_body()->resize(byteSize);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize);
	request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());
	(*rpcSession)->Send(messageId, request);
}

void SendToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		return;
	}

	entt::entity gateId{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
	if (!tls.gateNodeRegistry.valid(gateId))
	{
		LOG_ERROR << "Gate not found for player -> " << playerNodeInfo->gate_session_id();
		return;
	}

	const auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateId);
	if (!gateNode)
	{
		LOG_ERROR << "RpcSession not found for gate -> " << playerNodeInfo->gate_session_id();
		return;
	}

	SendToPlayer(messageId, message, *gateNode, playerNodeInfo->gate_session_id());
}

void SendToPlayer(uint32_t messageId, const google::protobuf::Message& message, RpcSessionPtr& gate, uint64_t sessionId)
{
	NodeRouteMessageRequest request;
	const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_body()->mutable_body()->resize(byteSize);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize);
	request.mutable_head()->set_session_id(sessionId);
	request.mutable_body()->set_message_id(messageId);
	gate->Send(GateServicePlayerMessageMsgId, request);
}

void SendToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToGate(const uint32_t messageId, const google::protobuf::Message& message, NodeId gateNodeId)
{
	entt::entity gateId{ gateNodeId };
	if (!tls.gateNodeRegistry.valid(gateId))
	{
		LOG_ERROR << "Gate not found -> " << gateNodeId;
		return;
	}

	const auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateId);
	if (!gateNode)
	{
		LOG_ERROR << "RpcSession not found for gate -> " << gateNodeId;
		return;
	}

	(*gateNode)->Send(messageId, message);
}

void CallGamePlayerMethod(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		return;
	}

	entt::entity gameNodeId{ playerNodeInfo->game_node_id() };
	if (tls.gameNodeRegistry.valid(gameNodeId))
	{
		return;
	}

	const auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gameNodeId);
	if (!gateNode)
	{
		LOG_ERROR << "RpcSession not found for game node -> " << playerNodeInfo->game_node_id();
		return;
	}

	NodeRouteMessageRequest request;
	const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_body()->mutable_body()->resize(byteSize);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize);
	request.mutable_body()->set_message_id(messageId);
	request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());
	(*gateNode)->CallMethod(GameServiceCallPlayerMsgId, request);
}

void CallGameNodeMethod(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
	entt::entity gameNodeId{ nodeId };
	if (!tls.gameNodeRegistry.valid(gameNodeId))
	{
		return;
	}

	const auto gameNode = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeId);
	if (!gameNode)
	{
		LOG_ERROR << "RpcSession not found for game node -> " << nodeId;
		return;
	}

	(*gameNode)->CallMethod(messageId, message);
}
