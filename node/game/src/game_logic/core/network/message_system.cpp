#include "message_system.h"
#include "muduo/base/Logging.h"
#include "network/rpc_client.h"
#include "network/rpc_session.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "thread_local/storage.h"
#include "thread_local/storage_common_logic.h"
#include "game_common_logic/comp/session_comp.h"
#include "game_common_logic/system/session_system.h"
#include "proto/logic/component/player_network_comp.pb.h"

void SendMessageToPlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.registry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
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
	request.mutable_header()->set_session_id(playerNodeInfo->gate_session_id());
	(*gateNode)->SendRequest(GateServiceSendMessageToPlayerMessageId, request);
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToCentrePlayerById(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.registry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(playerEntity);
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
	request.mutable_header()->set_session_id(playerNodeInfo->gate_session_id());
	(*centreNode)->SendRequest(CentreServicePlayerServiceMessageId, request);
}

void SendToCentre(const uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
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

	(*centreNode)->SendRequest(messageId, message);
}

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
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

	(*gateNode)->SendRequest(GateServiceSendMessageToPlayerMessageId, message);
}

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

	(*centreNode)->CallRemoteMethod(messageId, message);
}

void BroadCastToCentre(uint32_t messageId, const google::protobuf::Message& message)
{
	for (auto&& [_, node] : tls.centreNodeRegistry.view<RpcClientPtr>().each())
	{
		node->CallRemoteMethod(messageId, message);
	}
}

void BroadCastToPlayer(const uint32_t messageId, const google::protobuf::Message& message, const EntityUnorderedSet& playerList)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;

	for (auto& player : playerList)
	{
		if (!tls.registry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
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

	BroadcastToPlayersRequest request;
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

		(*gateNode)->SendRequest(GateServiceBroadcastToPlayersMessageId, request);
	}
}

void BroadCastToPlayer(const uint32_t messageId, const google::protobuf::Message& message, const EntityVector& playerList)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;

	for (auto& player : playerList)
	{
		if (!tls.registry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
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

	BroadcastToPlayersRequest request;
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

		(*gateNode)->SendRequest(GateServiceBroadcastToPlayersMessageId, request);
	}
}