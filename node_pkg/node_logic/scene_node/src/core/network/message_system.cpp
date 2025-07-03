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
#include "proto/common/node.pb.h"

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

	const auto* playerSessionSnapshotPB = tls.registry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB)
	{
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}

	entt::entity gateNodeId{ GetGateNodeId(playerSessionSnapshotPB->gate_session_id()) };
	auto& gateNodeRegistry = tls.GetNodeRegistry(eNodeType::GateNodeService);
	if (!gateNodeRegistry.valid(gateNodeId))
	{
		LOG_ERROR << "Gate node not found for player";
		return;
	}

	const auto gateNode = gateNodeRegistry.try_get<RpcSession>(gateNodeId);
	if (!gateNode)
	{
		LOG_ERROR << "RpcSession not found for gate node";
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->set_serialized_message(message.SerializeAsString());
	request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());

	gateNode->SendRequest(GateSendMessageToPlayerMessageId, request);
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToCentrePlayerById(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.registry.valid(playerEntity)){
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerSessionSnapshotPB = tls.registry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB){
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}

	entt::entity centreNodeId{ playerSessionSnapshotPB->centre_node_id() };
	auto& centreNodeRegistry = tls.GetNodeRegistry(eNodeType::CentreNodeService);
	if (!centreNodeRegistry.valid(centreNodeId)){
		LOG_ERROR << "Central node not found for player";
		return;
	}

	const auto node = centreNodeRegistry.try_get<RpcClientPtr>(centreNodeId);
	if (!node){
		LOG_ERROR << "RpcClientPtr not found for central node";
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->set_serialized_message(message.SerializeAsString());
	request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());
	(*node)->SendRequest(CentrePlayerServiceMessageId, request);
}

void SendToCentre(const uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
	entt::entity centreNodeId{ nodeId };
	auto& centreNodeRegistry = tls.GetNodeRegistry(eNodeType::CentreNodeService);
	if (!centreNodeRegistry.valid(centreNodeId))
	{
		LOG_ERROR << "Central node not found: " << nodeId;
		return;
	}

	const auto node = centreNodeRegistry.try_get<RpcClientPtr>(centreNodeId);
	if (!node)
	{
		LOG_ERROR << "RpcClientPtr not found for central node: " << nodeId;
		return;
	}

	(*node)->SendRequest(messageId, message);
}

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
	entt::entity gateNodeId{ GetGateNodeId(nodeId) };
	auto& gateNodeRegistry = tls.GetNodeRegistry(eNodeType::GateNodeService);
	if (!gateNodeRegistry.valid(gateNodeId))
	{
		LOG_ERROR << "Gate node not found for nodeId: " << nodeId;
		return;
	}

	const auto gateNodeSession = gateNodeRegistry.try_get<RpcSession>(gateNodeId);
	if (!gateNodeSession)
	{
		LOG_ERROR << "RpcSession not found for gate node: " << nodeId;
		return;
	}

	gateNodeSession->SendRequest(GateSendMessageToPlayerMessageId, message);
}

void CallCentreNodeMethod(uint32_t messageId, const google::protobuf::Message& message, const NodeId nodeId)
{
	entt::entity nodeEntity{ nodeId };
	auto& centreNodeRegistry = tls.GetNodeRegistry(eNodeType::CentreNodeService);
	if (!centreNodeRegistry.valid(nodeEntity))
	{
		LOG_ERROR << "Central node not found: " << nodeId;
		return;
	}

	const auto node = centreNodeRegistry.try_get<RpcClientPtr>(nodeEntity);
	if (!node)
	{
		LOG_ERROR << "RpcClientPtr not found for central node: " << nodeId;
		return;
	}

	(*node)->CallRemoteMethod(messageId, message);
}

void BroadCastToCentre(uint32_t messageId, const google::protobuf::Message& message)
{
	auto& centreNodeRegistry = tls.GetNodeRegistry(eNodeType::CentreNodeService);
	for (auto&& [_, node] : centreNodeRegistry.view<RpcClientPtr>().each())
	{
		node->CallRemoteMethod(messageId, message);
	}
}

void BroadCastToPlayer(const uint32_t messageId, const google::protobuf::Message& message, const EntityUnorderedSet& playerList)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;

	auto& gateNodeRegistry = tls.GetNodeRegistry(eNodeType::GateNodeService);

	for (auto& player : playerList)
	{
		if (!tls.registry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerSessionSnapshotPB = tls.registry.try_get<PlayerSessionSnapshotPBComp>(player);
		if (!playerSessionSnapshotPB)
		{
			LOG_ERROR << "Player node info not found for player entity: " << tls.registry.get<Guid>(player);
			continue;
		}

		entt::entity gateNodeId{ GetGateNodeId(playerSessionSnapshotPB->gate_session_id()) };
		if (!gateNodeRegistry.valid(gateNodeId))
		{
			LOG_ERROR << "Gate node not found for player session ID: " << playerSessionSnapshotPB->gate_session_id();
			continue;
		}

		gateList[gateNodeId].emplace(playerSessionSnapshotPB->gate_session_id());
	}

	BroadcastToPlayersRequest request;
	for (auto&& [gateNodeId, sessionIdList] : gateList)
	{
		const auto gateNodeSession = gateNodeRegistry.try_get<RpcSession>(gateNodeId);
		if (!gateNodeSession)
		{
			LOG_ERROR << "RpcSession not found for gate node";
			continue;
		}

		request.mutable_message_content()->set_message_id(messageId);
		request.mutable_message_content()->set_serialized_message(message.SerializeAsString());
		for (auto&& sessionId : sessionIdList)
		{
			request.mutable_session_list()->Add(sessionId);
		}

		gateNodeSession->SendRequest(GateBroadcastToPlayersMessageId, request);
	}
}

void BroadCastToPlayer(const uint32_t messageId, const google::protobuf::Message& message, const EntityVector& playerList)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;
	auto& gateNodeRegistry = tls.GetNodeRegistry(eNodeType::GateNodeService);

	for (auto& player : playerList)
	{
		if (!tls.registry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerSessionSnapshotPB = tls.registry.try_get<PlayerSessionSnapshotPBComp>(player);
		if (!playerSessionSnapshotPB)
		{
			LOG_ERROR << "Player node info not found for player entity: " << tls.registry.get<Guid>(player);
			continue;
		}

		entt::entity gateNodeId{ GetGateNodeId(playerSessionSnapshotPB->gate_session_id()) };
		if (!gateNodeRegistry.valid(gateNodeId))
		{
			LOG_ERROR << "Gate node not found for player session ID: " << playerSessionSnapshotPB->gate_session_id();
			continue;
		}

		gateList[gateNodeId].emplace(playerSessionSnapshotPB->gate_session_id());
	}

	BroadcastToPlayersRequest request;
	for (auto&& [gateNodeId, sessionIdList] : gateList)
	{
		const auto gateNodeSession = gateNodeRegistry.try_get<RpcSession>(gateNodeId);
		if (!gateNodeSession)
		{
			LOG_ERROR << "RpcSession not found for gate node";
			continue;
		}

		request.mutable_message_content()->set_message_id(messageId);
		request.mutable_message_content()->set_serialized_message(message.SerializeAsString());
		for (auto&& sessionId : sessionIdList)
		{
			request.mutable_session_list()->Add(sessionId);
		}

		gateNodeSession->SendRequest(GateBroadcastToPlayersMessageId, request);
	}
}