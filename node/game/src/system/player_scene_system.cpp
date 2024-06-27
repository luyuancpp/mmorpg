#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "thread_local/thread_local_storage_common_logic.h"
#include "system/scene/scene_system.h"
#include "network/message_system.h"
#include "system/game_node_scene_system.h"

#include "service/scene_client_player_service.h"

void PlayerSceneSystem::EnterScene(entt::entity player, Guid scene)
{
	GameNodeSceneSystem::EnterScene({ entt::entity{scene}, player,  });
}

void PlayerSceneSystem::OnEnterScene(entt::entity player, entt::entity scene)
{
	auto scene_info = tls.scene_registry.try_get<SceneInfo>(scene);
	if (nullptr == scene_info)
	{
		LOG_ERROR << "enter scene error" << tls.registry.get<Guid>(player);
		return;
	}
	EnterSceneS2C requset;
	requset.mutable_scene_info()->CopyFrom(*scene_info);
	Send2Player(ClientPlayerSceneServicePushEnterSceneS2CMsgId, requset, player);
}

void PlayerSceneSystem::LeaveScene(entt::entity player)
{
	LeaveSceneParam lp;
	lp.leaver_ = player;
	GameNodeSceneSystem::LeaveScene(player);
}

