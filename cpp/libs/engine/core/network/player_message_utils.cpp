#include "player_message_utils.h"
#include "engine/core/type_define/player_guid_comp.h"

#include "proto/common/component/player_network_comp.pb.h"
#include "thread_context/redis_manager.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "muduo/base/Logging.h"
#include "network_utils.h"
#include "network/rpc_session.h"
#include "network/node_utils.h"
#include "node/system/node/node_util.h"
#include <rpc/service_metadata/scene_service_metadata.h>
#include "thread_context/node_context_manager.h"
#include <thread_context/ecs_context.h>
#include "broadcast_target_codec.h"

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message &message, Guid playerId)
{
	SendMessageToClientViaGate(messageId, message, tlsEcs.GetPlayer(playerId));
}

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message &message, entt::entity playerEntity)
{
	if (!tlsEcs.actorRegistry.valid(playerEntity))
	{
		LOG_WARN << "Invalid player entity.";
		return;
	}

	const auto *playerSessionSnapshotPB = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(playerEntity);
	if (!playerSessionSnapshotPB)
	{
		LOG_WARN << "Player node info not found for player entity " << entt::to_integral(playerEntity);
		return;
	}

	auto gateEntityOpt = ResolveLocalZoneGateEntity(playerSessionSnapshotPB->gate_session_id());
	auto &gateNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
	if (!gateEntityOpt)
	{
		LOG_ERROR << "Gate session not found for player with session ID " << playerSessionSnapshotPB->gate_session_id();
		return;
	}
	entt::entity gateSessionId = *gateEntityOpt;

	const auto gateSessionPtr = gateNodeRegistry.try_get<RpcSession>(gateSessionId);
	if (!gateSessionPtr)
	{
		LOG_ERROR << "RpcSession not found for gate with session ID " << playerSessionSnapshotPB->gate_session_id();
		return;
	}

	// 身份栅栏(R13):玩家实体的 Guid 是权威身份,一并带给 gate 供写 socket 前比对。
	// try_get 而不是 get:极少数内部实体(测试桩 / 未完全构造的玩家)可能没有 Guid,
	// 拿不到时带 0 走兼容位,总比在推送路径上抛异常好。
	const auto *playerGuid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(playerEntity);
	SendMessageToClientViaGate(messageId, message, *gateSessionPtr, playerSessionSnapshotPB->gate_session_id(),
							   playerGuid != nullptr ? playerGuid->value : kInvalidGuid);
}

void SendMessageToClientViaGate(uint32_t messageId, const google::protobuf::Message &message, RpcSession &gate, SessionId sessionId, Guid targetPlayerId)
{
	NodeRouteMessageRequest request;
	const size_t byteSize = message.ByteSizeLong();
	request.mutable_message_content()->mutable_serialized_message()->resize(byteSize);
	if (!message.SerializePartialToArray(request.mutable_message_content()->mutable_serialized_message()->data(), static_cast<int>(byteSize)))
	{
		LOG_ERROR << "Failed to serialize message.";
		return;
	}
	request.mutable_header()->set_session_id(sessionId);
	// kInvalidGuid 与 0 在 gate 侧是同一个意思:"发送方没有玩家身份可带",走兼容位放行。
	// 不能把 kInvalidGuid 原样塞进去 —— 它是 uint64 最大值,会被当成一个真实玩家号去比对,
	// 从而把本该放行的兼容位变成必然失配的丢弃。
	if (targetPlayerId != kInvalidGuid)
	{
		request.mutable_header()->set_target_player_id(targetPlayerId);
	}
	request.mutable_message_content()->set_message_id(messageId);
	gate.SendRequest(GateSendMessageToPlayerMessageId, request);
}

// 广播目标:(session_id, player_id) 成对。编解码收在 broadcast_target_codec.h,
// 与 gate 侧共用同一份顺序契约(R13)—— 两边各写一遍必然某天错开一格,
// 而错开一格的表现是"每个人的身份都被比对成别人的"。
using BroadCastSessionIdList = std::vector<broadcast_targets::Target>;

template <typename PlayerContainer>
void InternalBroadcast(uint32_t messageId, const google::protobuf::Message &message, const PlayerContainer &playerList)
{
	std::unordered_map<entt::entity, BroadCastSessionIdList> gateList;
	auto &gateNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);

	for (auto &player : playerList)
	{
		if (!tlsEcs.actorRegistry.valid(player))
		{
			LOG_ERROR << "Invalid player entity in playerList";
			continue;
		}

		const auto *playerSessionSnapshotPB = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
		if (!playerSessionSnapshotPB)
		{
			const auto* playerGuid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(player);
			LOG_ERROR << "Player node info not found for player entity: " << (playerGuid ? playerGuid->value : 0);
			continue;
		}

		auto gateEntityOpt = ResolveLocalZoneGateEntity(playerSessionSnapshotPB->gate_session_id());
		if (!gateEntityOpt)
		{
			LOG_ERROR << "Gate node not found for player session ID: " << playerSessionSnapshotPB->gate_session_id();
			continue;
		}
		entt::entity gateNodeId = *gateEntityOpt;

		// 身份栅栏(R13):带上玩家 Guid,gate 写每一条 socket 前比对。
		// 拿不到 Guid(极少数未完全构造的实体)时填 0 = 兼容位,gate 放行不校验;
		// 绝不能填 kInvalidGuid —— 那是 uint64 最大值,会被当成一个真实玩家号去比对。
		const auto *playerGuid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(player);
		gateList[gateNodeId].push_back(broadcast_targets::Target{
			playerSessionSnapshotPB->gate_session_id(),
			(playerGuid != nullptr && playerGuid->value != kInvalidGuid) ? playerGuid->value : 0});
	}

	const std::string serializedMessage = message.SerializeAsString();

	for (auto &&[gateNodeId, sessionIdList] : gateList)
	{
		const auto gateNodeSession = gateNodeRegistry.try_get<RpcSession>(gateNodeId);
		if (!gateNodeSession)
		{
			LOG_ERROR << "RpcSession not found for gate node";
			continue;
		}

		BroadcastToPlayersRequest request;
		request.mutable_message_content()->set_message_id(messageId);
		request.mutable_message_content()->set_serialized_message(serializedMessage);

		broadcast_targets::Encode(request, sessionIdList);

		gateNodeSession->SendRequest(GateBroadcastToPlayersMessageId, request);
	}
}

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message &message, const EntityUnorderedSet &playerList)
{
	InternalBroadcast(messageId, message, playerList);
}

void BroadcastMessageToPlayers(uint32_t messageId, const google::protobuf::Message &message, const EntityVector &playerList)
{
	InternalBroadcast(messageId, message, playerList);
}

template <typename PlayerContainer>
void InternalBroadcastToScene(uint32_t messageId, const google::protobuf::Message &message,
							  uint64_t sceneId, const PlayerContainer &scenePlayerList)
{
	auto &gateNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
	std::unordered_set<entt::entity> gateNodes;

	for (auto &player : scenePlayerList)
	{
		if (!tlsEcs.actorRegistry.valid(player))
			continue;
		const auto *snapshot = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
		if (!snapshot)
			continue;
		if (auto gateEntityOpt = ResolveLocalZoneGateEntity(snapshot->gate_session_id()); gateEntityOpt)
			gateNodes.insert(*gateEntityOpt);
	}

	BroadcastToSceneRequest request;
	request.set_scene_id(sceneId);
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->set_serialized_message(message.SerializeAsString());

	for (auto gateNodeId : gateNodes)
	{
		const auto gateSession = gateNodeRegistry.try_get<RpcSession>(gateNodeId);
		if (!gateSession)
			continue;
		gateSession->SendRequest(GateBroadcastToSceneMessageId, request);
	}
}

void BroadcastMessageToScene(uint32_t messageId, const google::protobuf::Message &message,
							 uint64_t sceneId, const EntityUnorderedSet &scenePlayerList)
{
	InternalBroadcastToScene(messageId, message, sceneId, scenePlayerList);
}

void BroadcastMessageToScene(uint32_t messageId, const google::protobuf::Message &message,
							 uint64_t sceneId, const EntityVector &scenePlayerList)
{
	InternalBroadcastToScene(messageId, message, sceneId, scenePlayerList);
}

void BroadcastMessageToAll(uint32_t messageId, const google::protobuf::Message &message)
{
	auto &gateNodeRegistry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);

	BroadcastToAllRequest request;
	request.mutable_message_content()->set_message_id(messageId);
	request.mutable_message_content()->set_serialized_message(message.SerializeAsString());

	auto view = gateNodeRegistry.view<RpcSession>();
	for (auto entity : view)
	{
		auto &gateSession = view.get<RpcSession>(entity);
		gateSession.SendRequest(GateBroadcastToAllMessageId, request);
	}
}
