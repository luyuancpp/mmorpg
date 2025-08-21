#include "player_message_utils.h"

#include "proto/logic/component/player_network_comp.pb.h"
#include "thread_local/redis_manager.h"
#include "service_info/gate_service_service_info.h"
#include "muduo/base/Logging.h"
#include "network_utils.h"
#include "network/rpc_session.h"
#include "service_info/service_info.h"
#include "util/node_utils.h"
#include <service_info/game_service_service_info.h>
#include "thread_local/node_context_manager.h"
#include "thread_local/player_manager.h"
#include <thread_local/registry_manager.h>


void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToClientViaGate(messageId, message, PlayerManager::Instance().GetPlayer(playerId));
}

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity))
	{
		LOG_WARN << "Invalid player entity.";
		return;
	}

	const auto* playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB)
	{
		LOG_WARN << "Player node info not found for player entity " << entt::to_integral(playerEntity);
		return;
	}

	entt::entity gateSessionId{ GetGateNodeId(playerSessionSnapshotPB->gate_session_id()) };
	auto& gateNodeRegistry = NodeContextManager::Instance().GetRegistry(eNodeType::GateNodeService);
	if (!gateNodeRegistry.valid(gateSessionId))
	{
		LOG_ERROR << "Gate session not found for player with session ID " << playerSessionSnapshotPB->gate_session_id();
		return;
	}

	const auto gateSessionPtr = gateNodeRegistry.try_get<RpcSession>(gateSessionId);
	if (!gateSessionPtr)
	{
		LOG_ERROR << "RpcSession not found for gate with session ID " << playerSessionSnapshotPB->gate_session_id();
		return;
	}

	SendMessageToClientViaGate(messageId, message, *gateSessionPtr, playerSessionSnapshotPB->gate_session_id());
}

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message& message, RpcSession& gate, uint64_t sessionId)
{
	NodeRouteMessageRequest request;
	const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_message_content()->mutable_serialized_message()->resize(byteSize);
	message.SerializePartialToArray(request.mutable_message_content()->mutable_serialized_message()->data(), byteSize);
	request.mutable_header()->set_session_id(sessionId);
	request.mutable_message_content()->set_message_id(messageId);
	gate.SendRequest(GateSendMessageToPlayerMessageId, request);
}

void SendMessageToGateById(uint32_t messageId, const google::protobuf::Message& message, NodeId gateNodeId)
{
	entt::entity gateEntity{ gateNodeId };
	auto& gateNodeRegistry = NodeContextManager::Instance().GetRegistry(eNodeType::GateNodeService);
	if (!gateNodeRegistry.valid(gateEntity))
	{
		LOG_ERROR << "Gate not found for NodeId -> " << gateNodeId;
		return;
	}

	const auto gateSessionPtr = gateNodeRegistry.try_get<RpcSession>(gateEntity);
	if (!gateSessionPtr)
	{
		LOG_ERROR << "RpcSession not found for Gate NodeId -> " << gateNodeId;
		return;
	}

	gateSessionPtr->SendRequest(messageId, message);
}


using BroadCastSessionIdList = std::unordered_set<uint64_t>;

template<typename PlayerContainer>
void InternalBroadcast(uint32_t messageId, const google::protobuf::Message& message, const PlayerContainer& playerList)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;
	auto& gateNodeRegistry = NodeContextManager::Instance().GetRegistry(eNodeType::GateNodeService);

	for (auto& player : playerList)
	{
		if (!tlsRegistryManager.actorRegistry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto* playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
		if (!playerSessionSnapshotPB)
		{
			LOG_ERROR << "Player node info not found for player entity: " << tlsRegistryManager.actorRegistry.get<Guid>(player);
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

	// 构造广播请求并发送
	for (auto&& [gateNodeId, sessionIdList] : gateList)
	{
		const auto gateNodeSession = gateNodeRegistry.try_get<RpcSession>(gateNodeId);
		if (!gateNodeSession)
		{
			LOG_ERROR << "RpcSession not found for gate node";
			continue;
		}

		BroadcastToPlayersRequest request;
		request.mutable_message_content()->set_message_id(messageId);
		request.mutable_message_content()->set_serialized_message(message.SerializeAsString());

		for (auto&& sessionId : sessionIdList)
		{
			request.mutable_session_list()->Add(sessionId);
		}

		gateNodeSession->SendRequest(GateBroadcastToPlayersMessageId, request);
	}
}

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityUnorderedSet& playerList)
{
	InternalBroadcast(messageId, message, playerList);
}

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message& message, const EntityVector& playerList)
{
	InternalBroadcast(messageId, message, playerList);
}

void SendMessageToPlayerOnGrpcNode(uint32_t messageId, const google::protobuf::Message& message, Guid playerId) {
	SendMessageToPlayerOnGrpcNode(messageId, message, PlayerManager::Instance().GetPlayer(playerId));
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

void SendMessageToPlayerOnGrpcNode(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity) {
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity is not valid";
		return;
	}

	auto& rpcHandlerMeta = gRpcServiceRegistry[messageId];

	const auto* playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB)
	{
		LOG_ERROR << "Player node info not found for player entity";
		return;
	}

	SessionDetails sessionDetails;
	sessionDetails.set_session_id(playerSessionSnapshotPB->gate_session_id());
	sessionDetails.set_player_id(tlsRegistryManager.actorRegistry.get<Guid>(playerEntity));

	if (!rpcHandlerMeta.messageSender) {
		LOG_ERROR << "Message sender not found for message ID: " << messageId;
		return;
	}

	auto nodeId = GetEffectiveNodeId(rpcHandlerMeta.targetNodeType);
	entt::entity node{ entt::to_entity(nodeId) };
	if (!NodeContextManager::Instance().GetRegistry(rpcHandlerMeta.targetNodeType).valid(node)) {
		LOG_ERROR << "Node not found for type: " << rpcHandlerMeta.targetNodeType;
		return;
	}
	rpcHandlerMeta.messageSender(NodeContextManager::Instance().GetRegistry(rpcHandlerMeta.targetNodeType),
		node,
		*rpcHandlerMeta.requestPrototype,
		{ kSessionBinMetaKey },
		SerializeSessionDetails(sessionDetails));
}


void SendMessageToPlayerOnSceneNode(uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity)
{
	SendMessageToPlayerOnNode(
		/* wrappedMessageId */ SceneSendMessageToPlayerMessageId,
		/* nodeType */ eNodeType::SceneNodeService,
		/* messageId */ messageId,
		message,
		playerEntity);
}

void SendMessageToPlayerOnSceneNode(uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId)
{
	SendMessageToPlayerOnSceneNode(messageId, message, PlayerManager::Instance().GetPlayer(playerId));
}


void SendMessageToPlayerOnNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId) {
	SendMessageToPlayerOnNode(wrappedMessageId, nodeType, messageId, message, PlayerManager::Instance().GetPlayer(playerId));
}


void SendMessageToPlayerOnNode(uint32_t wrappedMessageId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity playerEntity)
{
	if (!tlsRegistryManager.actorRegistry.valid(playerEntity))
	{
		LOG_ERROR << "Invalid player entity -> " << entt::to_integral(playerEntity);
		return;
	}

	const auto* playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB)
	{
		LOG_ERROR << "Player session info not found -> " << entt::to_integral(playerEntity);
		return;
	}

	const auto& nodeIdMap = playerSessionSnapshotPB->node_id();
	auto it = nodeIdMap.find(nodeType);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << nodeType
			<< ", player entity: " << entt::to_integral(playerEntity);
		return;
	}

	entt::entity nodeEntity{ it->second };

	auto& registry = NodeContextManager::Instance().GetRegistry(nodeType);
	if (!registry.valid(nodeEntity))
	{
		LOG_ERROR << "Node entity invalid for player -> " << entt::to_integral(playerEntity)
			<< ", node ID: " << entt::to_integral(nodeEntity);
		return;
	}

	const auto session = registry.try_get<RpcSession>(nodeEntity);
	if (!session)
	{
		LOG_ERROR << "RpcSession not found for node: " << entt::to_integral(nodeEntity);
		return;
	}

	NodeRouteMessageRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->set_serialized_message(message.SerializeAsString());
	request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());

	session->SendRequest(wrappedMessageId, request);
}


void CallMethodOnPlayerNode(
	uint32_t remoteMethodId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	Guid playerId) {
	CallMethodOnPlayerNode(remoteMethodId, nodeType, messageId, message, PlayerManager::Instance().GetPlayer(playerId));
}

void CallMethodOnPlayerNode(
	uint32_t remoteMethodId,
	uint32_t nodeType,
	uint32_t messageId,
	const google::protobuf::Message& message,
	entt::entity player)
{
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "Invalid player entity.";
		return;
	}

	const auto* playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
	if (!playerSessionSnapshotPB)
	{
		LOG_ERROR << "PlayerSessionSnapshotPBComp not found for player -> " << entt::to_integral(player);
		return;
	}

	const auto& nodeIdMap = playerSessionSnapshotPB->node_id();
	auto it = nodeIdMap.find(nodeType);
	if (it == nodeIdMap.end()) {
		LOG_ERROR << "Node type not found in player session snapshot: " << nodeType
			<< ", player entity: " << entt::to_integral(player);
		return;
	}

	entt::entity targetNodeEntity{ it->second };

	auto& registry = NodeContextManager::Instance().GetRegistry(nodeType);

	if (!registry.valid(targetNodeEntity))
	{
		LOG_ERROR << "Invalid target node for player -> " << entt::to_integral(player)
			<< ", node_id: " << it->second;
		return;
	}

	const auto session = registry.try_get<RpcSession>(targetNodeEntity);
	if (!session)
	{
		LOG_ERROR << "RpcSession not found for node -> " << it->second;
		return;
	}

	NodeRouteMessageRequest request;
	const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_message_content()->mutable_serialized_message()->resize(byteSize);

	if (!message.SerializePartialToArray(request.mutable_message_content()->mutable_serialized_message()->data(), byteSize))
	{
		LOG_ERROR << "Failed to serialize message.";
		return;
	}

	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());

	session->CallRemoteMethod(remoteMethodId, request);
}
