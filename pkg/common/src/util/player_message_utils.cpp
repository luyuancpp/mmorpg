#include "player_message_utils.h"

#include "proto/logic/component/player_network_comp.pb.h"
#include "thread_local/storage.h"
#include "thread_local/storage_common_logic.h"
#include "service_info/gate_service_service_info.h"
#include "muduo/base/Logging.h"
#include "util/network_utils.h"
#include "network/rpc_session.h"

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity playerEntity)
{
	if (!tls.actorRegistry.valid(playerEntity))
	{
		LOG_WARN << "Invalid player entity.";
		return;
	}

	const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!playerSessionSnapshotPB)
	{
		LOG_WARN << "Player node info not found for player entity " << entt::to_integral(playerEntity);
		return;
	}

	entt::entity gateSessionId{ GetGateNodeId(playerSessionSnapshotPB->gate_session_id()) };
	auto& gateNodeRegistry = tls.GetNodeRegistry(eNodeType::GateNodeService);
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

	SendMessageToPlayer(messageId, message, *gateSessionPtr, playerSessionSnapshotPB->gate_session_id());
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, RpcSession& gate, uint64_t sessionId)
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
	auto& gateNodeRegistry = tls.GetNodeRegistry(eNodeType::GateNodeService);
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
