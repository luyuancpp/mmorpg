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
#include "node/scene_node_info.h"
#include "node/comp/node_comp.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "service_info/service_info.h"
#include "util/network_utils.h"

void SendMessageToPlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.actorRegistry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
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

void SendMessageToGrpcPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId){
	SendMessageToGrpcPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

NodeInfo* FindZoneUniqueNodeInfo(uint32_t zoneId, uint32_t nodeType) {
	auto& nodeRegistry = tls.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity());
	auto& nodeList = *nodeRegistry[nodeType].mutable_node_list();
	for (auto& node : nodeList)
	{
		if (node.zone_id() == zoneId)
		{
			return &node;
		}
	}
	return nullptr;
}

inline NodeId GetEffectiveNodeId(
	uint32_t nodeType)
{
	if (IsZoneSingletonNodeType(nodeType)) {
		auto node = FindZoneUniqueNodeInfo(GetNodeInfo().zone_id(), nodeType);
		if (node == nullptr) {
			LOG_ERROR << "Node not found for type: " << nodeType;
			return kInvalidNodeId;
		}

		return node->node_id();
	}

	return kInvalidNodeId;
}

void SendMessageToGrpcPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity){
	if (!tls.actorRegistry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	auto& rpcHandlerMeta  = gRpcServiceRegistry[messageId];

	const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB)
	{
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}
	
	SessionDetails sessionDetails;
	sessionDetails.set_session_id(playerSessionSnapshotPB->gate_session_id());
	sessionDetails.set_player_id(tls.actorRegistry.get<Guid>(playerEntity));
	
	if (rpcHandlerMeta .messageSender){
		auto nodeId = GetEffectiveNodeId(rpcHandlerMeta.targetNodeType);
		entt::entity node{entt::to_entity(nodeId)};
		
		rpcHandlerMeta .messageSender(tls.GetNodeRegistry(rpcHandlerMeta .targetNodeType), 
			node, 
			*rpcHandlerMeta .requestPrototype, 
			{ kSessionBinMetaKey }, 
			SerializeSessionDetails(sessionDetails));
	}
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToCentrePlayerById(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToCentrePlayerById(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.actorRegistry.valid(playerEntity)){
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
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
		if (!tls.actorRegistry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
		if (!playerSessionSnapshotPB)
		{
			LOG_ERROR << "Player node info not found for player entity: " << tls.actorRegistry.get<Guid>(player);
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
		if (!tls.actorRegistry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
		if (!playerSessionSnapshotPB)
		{
			LOG_ERROR << "Player node info not found for player entity: " << tls.actorRegistry.get<Guid>(player);
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