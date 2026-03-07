#include "player_scene.h"

#include "muduo/base/Logging.h"

#include "core/network/message_system.h"

#include "threading/redis_manager.h"

#include "rpc/service_metadata/player_scene_service_metadata.h"

#include "network/player_message_utils.h"
#include <threading/registry_manager.h>
#include "proto/common/component/team_comp.pb.h"
#include "proto/scene_manager/storage.pb.h"
#include "rpc/service_metadata/centre_player_scene_service_metadata.h"
#include "scene/scene/system/scene_common.h"
#include "proto/centre/centre_player_scene.pb.h"
#include "threading/node_context_manager.h"

// Callback wrapper for Hiredis
void PlayerSceneSystem::OnGetTeamInfo(entt::entity player, void* replyVoid)
{
	redisReply* reply = static_cast<redisReply*>(replyVoid);
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL)
	{
		return;
	}

	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		return;
	}

	TeamInfo teamInfo;
	if (!teamInfo.ParseFromArray(reply->str, reply->len))
	{
		LOG_ERROR << "Failed to parse TeamInfo from Redis for player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
		return;
	}

	uint64_t leaderId = teamInfo.leader_id();
	uint64_t myId = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);

	if (leaderId == 0 || leaderId == myId)
	{
		return; // Not a follower or invalid team
	}

	// Fetch leader location
	auto cb = [player](hiredis::Hiredis* c, int status, redisReply* r) {
		OnGetLeaderLocation(player, r);
	};
	tlsReids.GetZoneRedis()->command(cb, "GET player:%llu:location", leaderId);
}

void PlayerSceneSystem::OnGetLeaderLocation(entt::entity player, void* replyVoid)
{
	redisReply* reply = static_cast<redisReply*>(replyVoid);
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL)
	{
		return;
	}

	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		return;
	}

	storage::PlayerLocation loc;
	if (!loc.ParseFromArray(reply->str, reply->len))
	{
		LOG_ERROR << "Failed to parse PlayerLocation from Redis for player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
		return;
	}

	// Compare scenes
	const auto sceneEntity = tlsRegistryManager.actorRegistry.try_get<SceneEntityComp>(player);
	if (!sceneEntity) return;

	const auto sceneInfo = tlsRegistryManager.actorRegistry.try_get<SceneInfoPBComponent>(sceneEntity->sceneEntity);
	if (!sceneInfo) return;

	uint64_t currentSceneId = sceneInfo->guid(); // Assuming guid is scene_id (uint32 vs uint64 issue handled by cast)
	uint64_t leaderSceneId = loc.scene_id();

	if (leaderSceneId != 0 && leaderSceneId != currentSceneId)
	{
		LOG_INFO << "Player " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) 
				 << " (Follower) needs to switch to Leader's scene: " << leaderSceneId;
		
		// Initiate scene change via Centre
		CentreEnterSceneRequest request;
		request.mutable_scene_info()->set_guid(static_cast<uint32_t>(leaderSceneId));
		// Set other fields if necessary
		
		NodeId centreNodeId = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSessionSnapshotPBComp>(player).node_id().at(eNodeType::CentreNodeService);
		CallRemoteMethodOnClient(CentrePlayerSceneEnterSceneMessageId, request, centreNodeId, eNodeType::CentreNodeService);
	}
}

void PlayerSceneSystem::HandleEnterScene(entt::entity player, entt::entity scene)
{
	const auto sceneInfo = tlsRegistryManager.sceneRegistry.try_get<SceneInfoPBComponent>(scene);
	if (sceneInfo == nullptr)
	{
		LOG_ERROR << "Failed to get scene info for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
		return;
	}

	EnterSceneS2C message;
	message.mutable_scene_info()->CopyFrom(*sceneInfo);
	SendMessageToClientViaGate(SceneSceneClientPlayerNotifyEnterSceneMessageId, message, player);

	// Team Follow Logic
	const auto teamIdComp = tlsRegistryManager.actorRegistry.try_get<TeamId>(player);
	if (teamIdComp && teamIdComp->team_id() != 0)
	{
		auto cb = [player](hiredis::Hiredis* c, int status, redisReply* r) {
			OnGetTeamInfo(player, r);
		};
		tlsReids.GetZoneRedis()->command(cb, "GET team:%llu", teamIdComp->team_id());
	}
}

void PlayerSceneSystem::HandleBeforeLeaveScene(entt::entity player)
{

}
