#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/thread_local/common_logic_thread_local_storage.h"
#include "src/system/scene/scene_system.h"
#include "src/network/gate_node.h"
#include "src/network/message_system.h"
#include "src/system/gs_scene_system.h"

void PlayerSceneSystem::EnterScene(entt::entity player, uint64_t guid)
{
	const auto scene = ScenesSystem::GetSceneByGuid(guid);
	if (scene == entt::null)
	{
		LOG_ERROR << "scene not found " << guid;
		return;
	}
	GsSceneSystem::EnterScene({player, scene});
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

