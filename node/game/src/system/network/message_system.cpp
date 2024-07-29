#include "message_system.h"
#include "muduo/base/Logging.h"
#include "network/gate_session.h"
#include "network/rpc_client.h"
#include "network/rpc_session.h"
#include "service/centre_service_service.h"
#include "service/gate_service_service.h"
#include "thread_local/storage.h"
#include "thread_local/storage_common_logic.h"
#include "comp/session.h"
#include "comp/player.h"
#include "proto/logic/component/player_network_comp.pb.h"


void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}

	entt::entity gateNodeId{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
	if (!tls.gateNodeRegistry.valid(gateNodeId))
	{
		LOG_ERROR << "Gate node not found for player";
		return;
	}

	const auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateNodeId);
	if (!gateNode)
	{
		LOG_ERROR << "RpcSessionPtr not found for gate node";
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_body()->set_message_id(messageId);
	request.mutable_body()->set_body(message.SerializeAsString());
	request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());
	(*gateNode)->Send(GateServicePlayerMessageMsgId, request);
}

void SendToCentrePlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToCentrePlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToCentrePlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.registry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}

	entt::entity centreNodeId{ playerNodeInfo->centre_node_id() };
	if (!tls.centreNodeRegistry.valid(centreNodeId))
	{
		LOG_ERROR << "Central node not found for player";
		return;
	}

	const auto centreNode = tls.centreNodeRegistry.try_get<RpcClientPtr>(centreNodeId);
	if (!centreNode)
	{
		LOG_ERROR << "RpcClientPtr not found for central node";
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_body()->set_message_id(messageId);
	request.mutable_body()->set_body(message.SerializeAsString());
	request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());
	(*centreNode)->Send(CentreServiceGsPlayerServiceMsgId, request);
}

void Send2Centre(const uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
	entt::entity centreNodeId{ nodeId };
	if (!tls.centreNodeRegistry.valid(centreNodeId))
	{
		LOG_ERROR << "Central node not found: " << nodeId;
		return;
	}

	const auto centreNode = tls.centreNodeRegistry.try_get<RpcClientPtr>(centreNodeId);
	if (!centreNode)
	{
		LOG_ERROR << "RpcClientPtr not found for central node: " << nodeId;
		return;
	}

	(*centreNode)->Send(messageId, message);
}

// Sends a message to a gate node identified by nodeId
void SendToGate(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
	entt::entity gateNodeId{ GetGateNodeId(nodeId) };
	if (!tls.gateNodeRegistry.valid(gateNodeId))
	{
		LOG_ERROR << "Gate node not found for nodeId: " << nodeId;
		return;
	}

	const auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateNodeId);
	if (!gateNode)
	{
		LOG_ERROR << "RpcSessionPtr not found for gate node: " << nodeId;
		return;
	}

	(*gateNode)->Send(GateServicePlayerMessageMsgId, message);
}

// Calls a method on a central node identified by nodeId
void CallCentreNodeMethod(uint32_t messageId, const google::protobuf::Message& message, const NodeId nodeId)
{
	entt::entity centreNodeId{ nodeId };
	if (!tls.centreNodeRegistry.valid(centreNodeId))
	{
		LOG_ERROR << "Central node not found: " << nodeId;
		return;
	}

	const auto centreNode = tls.centreNodeRegistry.try_get<RpcClientPtr>(centreNodeId);
	if (!centreNode)
	{
		LOG_ERROR << "RpcClientPtr not found for central node: " << nodeId;
		return;
	}

	(*centreNode)->CallMethod(messageId, message);
}

// Broadcasts a message to all registered central nodes
void BroadCastToCentre(uint32_t messageId, const google::protobuf::Message& message)
{
	for (auto&& [_, node] : tls.centreNodeRegistry.view<RpcClientPtr>().each())
	{
		node->CallMethod(messageId, message);
	}
}

// Broadcasts a message to a list of players
void BroadCastToPlayer(const EntityUnorderedSet& playerList, const uint32_t messageId, const google::protobuf::Message& message)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;

	// Collect player session IDs grouped by their associated gate nodes
	for (auto& player : playerList)
	{
		if (!tls.registry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
		if (!playerNodeInfo)
		{
			LOG_ERROR << "Player node info not found for player entity: " << tls.registry.get<Guid>(player);
			continue;
		}

		entt::entity gateNodeId{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
		if (!tls.gateNodeRegistry.valid(gateNodeId))
		{
			LOG_ERROR << "Gate node not found for player session ID: " << playerNodeInfo->gate_session_id();
			continue;
		}

		gateList[gateNodeId].emplace(playerNodeInfo->gate_session_id());
	}

	// Send broadcast message to each gate node with associated player session IDs
	BroadCast2PlayerRequest request;
	for (auto&& [gateNodeId, sessionIdList] : gateList)
	{
		const auto gateNode = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateNodeId);
		if (!gateNode)
		{
			LOG_ERROR << "RpcSessionPtr not found for gate node";
			continue;
		}

		request.mutable_body()->set_message_id(messageId);
		request.mutable_body()->set_body(message.SerializeAsString());
		for (auto&& sessionId : sessionIdList)
		{
			request.mutable_session_list()->Add(sessionId);
		}

		(*gateNode)->Send(GateServiceBroadCast2PlayerMessageMsgId, request);
	}
}
