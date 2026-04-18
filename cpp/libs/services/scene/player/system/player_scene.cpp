#include "player_scene.h"

#include "muduo/base/Logging.h"

#include "core/network/message_system.h"

#include "thread_context/redis_manager.h"

#include "rpc/service_metadata/player_scene_service_metadata.h"

#include "network/player_message_utils.h"
#include "network/node_message_utils.h"
#include "network/network_utils.h"
#include <thread_context/registry_manager.h>
#include "engine/thread_context/node_context_manager.h"
#include "proto/common/component/team_comp.pb.h"
#include "proto/scene_manager/storage.pb.h"
#include "proto/scene_manager/scene_manager_service.pb.h"
#include "grpc_client/scene_manager/scene_manager_service_grpc_client.h"
#include "network/node_utils.h"
#include <modules/scene/comp/scene_comp.h>
#include <modules/scene/comp/scene_node_comp.h>
#include <proto/common/component/player_network_comp.pb.h>
#include "node/system/node/node_util.h"
#include <limits>

// Callback wrapper for Hiredis
void PlayerSceneSystem::OnGetTeamInfo(entt::entity player, void* replyVoid)
{
	redisReply* reply = static_cast<redisReply*>(replyVoid);
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL)
	{
		return;
	}

	if (!tlsEcs.actorRegistry.valid(player))
	{
		return;
	}

	TeamInfo teamInfo;
	if (reply->len > static_cast<size_t>(std::numeric_limits<int>::max()))
	{
		LOG_ERROR << "TeamInfo payload too large for protobuf parser, len=" << reply->len;
		return;
	}

	if (!teamInfo.ParseFromArray(reply->str, static_cast<int>(reply->len)))
	{
		LOG_ERROR << "Failed to parse TeamInfo from Redis for player " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player);
		return;
	}

	uint64_t leaderId = teamInfo.leader_id();
	uint64_t myId = tlsEcs.actorRegistry.get_or_emplace<Guid>(player);

	if (leaderId == 0 || leaderId == myId)
	{
		return; // Not a follower or invalid team
	}

	// Fetch leader location
	auto cb = [player](hiredis::Hiredis* c, redisReply* r) {
		OnGetLeaderLocation(player, r);
	};
	tlsRedis.GetZoneRedis()->command(cb, "GET player:%llu:location", leaderId);
}

void PlayerSceneSystem::OnGetLeaderLocation(entt::entity player, void* replyVoid)
{
	redisReply* reply = static_cast<redisReply*>(replyVoid);
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL)
	{
		return;
	}

	if (!tlsEcs.actorRegistry.valid(player))
	{
		return;
	}

	storage::PlayerLocation loc;
	if (reply->len > static_cast<size_t>(std::numeric_limits<int>::max()))
	{
		LOG_ERROR << "PlayerLocation payload too large for protobuf parser, len=" << reply->len;
		return;
	}

	if (!loc.ParseFromArray(reply->str, static_cast<int>(reply->len)))
	{
		LOG_ERROR << "Failed to parse PlayerLocation from Redis for player " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player);
		return;
	}

	// Compare scenes
	const auto sceneEntity = tlsEcs.actorRegistry.try_get<SceneEntityComp>(player);
	if (!sceneEntity) {
		return;
	}

	const auto sceneInfo = tlsEcs.actorRegistry.try_get<SceneInfoComp>(sceneEntity->sceneEntity);
	if (!sceneInfo) {
		return;
	}

	uint64_t currentSceneId = sceneInfo->scene_id();
	uint64_t leaderSceneId = loc.scene_id();

	if (leaderSceneId != 0 && leaderSceneId != currentSceneId)
	{
		const auto* playerSessionPB = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
		if (!playerSessionPB)
		{
			LOG_ERROR << "PlayerSessionSnapshotComp missing for follower " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player);
			return;
		}

		// Resolve gate info
		NodeId gateNodeId = GetGateNodeId(playerSessionPB->gate_session_id());

		std::string gateInstanceId;
		auto& gateRegistry = tlsNodeContextManager.GetRegistry(eNodeType::GateNodeService);
		entt::entity gateEntity{ gateNodeId };
		if (const auto* gateNodeInfo = gateRegistry.try_get<NodeInfo>(gateEntity))
		{
			gateInstanceId = gateNodeInfo->node_uuid();
		}

		// Find a SceneManager gRPC node
		auto smEntity = GetSceneManagerEntity(playerSessionPB->player_id());
		if (smEntity == entt::null)
		{
			LOG_WARN << "No SceneManager node available, cannot follow leader to scene " << leaderSceneId;
			return;
		}

		auto &smRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneManagerNodeService);

		::scene_manager::EnterSceneRequest req;
		req.set_player_id(playerSessionPB->player_id());
		req.set_scene_id(leaderSceneId);
		req.set_session_id(playerSessionPB->gate_session_id());
		req.set_gate_id(std::to_string(gateNodeId));
		req.set_gate_instance_id(gateInstanceId);
		req.set_gate_zone_id(GetZoneId());
		req.set_zone_id(GetZoneId());

		scene_manager::SendSceneManagerEnterScene(smRegistry, smEntity, req);

		LOG_INFO << "Player " << playerSessionPB->player_id()
				 << " (Follower) requesting SceneManager to switch to Leader's scene " << leaderSceneId;
	}
}

void PlayerSceneSystem::HandleEnterScene(entt::entity player, entt::entity scene)
{
	const auto sceneInfo = tlsEcs.sceneRegistry.try_get<SceneInfoComp>(scene);
	if (sceneInfo == nullptr)
	{
		LOG_ERROR << "HandleEnterScene: scene info not found for player: "
		          << tlsEcs.actorRegistry.get_or_emplace<Guid>(player);
		return;
	}

	// 0. Idempotency: if player is already in the target scene, skip.
	auto *oldSceneComp = tlsEcs.actorRegistry.try_get<SceneEntityComp>(player);
	if (oldSceneComp != nullptr && oldSceneComp->sceneEntity == scene)
	{
		LOG_INFO << "HandleEnterScene: player " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player)
		         << " already in scene_id=" << sceneInfo->scene_id() << ", skipping";
		return;
	}

	// 1. Leave old scene if player is already in one.
	if (oldSceneComp != nullptr && oldSceneComp->sceneEntity != entt::null
	    && oldSceneComp->sceneEntity != scene)
	{
		auto *oldPlayers = tlsEcs.sceneRegistry.try_get<ScenePlayers>(oldSceneComp->sceneEntity);
		if (oldPlayers)
		{
			oldPlayers->erase(player);
		}
	}

	// 2. Bind player to the new scene entity.
	tlsEcs.actorRegistry.get_or_emplace<SceneEntityComp>(player).sceneEntity = scene;

	// 3. Track player in the scene's player set.
	auto *scenePlayers = tlsEcs.sceneRegistry.try_get<ScenePlayers>(scene);
	if (scenePlayers)
	{
		scenePlayers->emplace(player);
	}

	// 4. Notify client of scene entry.
	EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, message, player);

	LOG_INFO << "HandleEnterScene: player " << tlsEcs.actorRegistry.get_or_emplace<Guid>(player)
			 << " entered scene_id=" << sceneInfo->scene_id();

	// 5. Team Follow Logic: if player is in a team, check leader location.
	const auto teamIdComp = tlsEcs.actorRegistry.try_get<TeamId>(player);
	if (teamIdComp && teamIdComp->team_id() != 0)
	{
		auto cb = [player](hiredis::Hiredis* c, redisReply* r) {
			OnGetTeamInfo(player, r);
		};
		tlsRedis.GetZoneRedis()->command(cb, "GET team:%llu", teamIdComp->team_id());
	}
}

