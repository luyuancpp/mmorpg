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
	EnterSceneS2C requset;
	Send2Player(ClientPlayerSceneServicePushEnterSceneS2CMsgId, requset, player);
}

void PlayerSceneSystem::LeaveScene(entt::entity player)
{
	LeaveSceneParam lp;
	lp.leaver_ = player;
	GameNodeSceneSystem::LeaveScene(player);
}

