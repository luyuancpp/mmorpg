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
#include <modules/scene/comp/scene_comp.h>
#include <proto/common/component/player_network_comp.pb.h>
#include <proto/common/base/node.pb.h>
#include <proto/common/base/common.pb.h>

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
	auto cb = [player](hiredis::Hiredis* c, redisReply* r) {
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
		const auto* playerSessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
		if (!playerSessionPB)
		{
			LOG_ERROR << "PlayerSessionSnapshotPBComp missing for follower " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
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
		auto& smRegistry = tlsNodeContextManager.GetRegistry(eNodeType::SceneManagerNodeService);
		entt::entity smEntity = entt::null;
		for (const auto& [e, info] : smRegistry.view<NodeInfo>().each())
		{
			smEntity = e;
			break;
		}
		if (smEntity == entt::null)
		{
			LOG_WARN << "No SceneManager node available, cannot follow leader to scene " << leaderSceneId;
			return;
		}

		::scene_manager::EnterSceneRequest req;
		req.set_player_id(playerSessionPB->player_id());
		req.set_scene_id(leaderSceneId);
		req.set_session_id(playerSessionPB->gate_session_id());
		req.set_gate_id(std::to_string(gateNodeId));
		req.set_gate_instance_id(gateInstanceId);

		scene_manager::SendSceneManagerEnterScene(smRegistry, smEntity, req);

		LOG_INFO << "Player " << playerSessionPB->player_id()
				 << " (Follower) requesting SceneManager to switch to Leader's scene " << leaderSceneId;
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
		auto cb = [player](hiredis::Hiredis* c, redisReply* r) {
			OnGetTeamInfo(player, r);
		};
		tlsReids.GetZoneRedis()->command(cb, "GET team:%llu", teamIdComp->team_id());
	}
}

void PlayerSceneSystem::HandleBeforeLeaveScene(entt::entity player)
{

}

