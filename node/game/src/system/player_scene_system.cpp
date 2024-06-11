#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/system/scene/scene_system.h"
#include "src/network/message_system.h"
#include "src/system/gs_scene_system.h"

void PlayerSceneSystem::EnterScene(entt::entity player, Guid scene)
{
	GsSceneSystem::EnterScene({ player, entt::entity{scene} });
}

void PlayerSceneSystem::OnEnterScene(entt::entity player, entt::entity scene)
{
	
}

void PlayerSceneSystem::LeaveScene(entt::entity player)
{
	LeaveSceneParam lp;
	lp.leaver_ = player;
	GsSceneSystem::LeaveScene(player);
}

