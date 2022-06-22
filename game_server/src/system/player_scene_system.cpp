#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/system/entity_scene_system.h"

#include "logic_proto/scene_client_player.pb.h"

#include "src/network/message_system.h"

void PlayerSceneSystem::EnterScene(entt::entity player, uint64_t scene_id)
{
	auto scene = ScenesSystem::GetSingleton().get_scene(scene_id);
	if (scene == entt::null)
	{
		LOG_ERROR << "scene not found " << scene_id;
		return;
	}
	EnterSceneParam ep;
	ep.enterer_ = player;
	ep.scene_ = scene;
	EntitySceneSystem::EnterScene(ep);
}

void PlayerSceneSystem::OnEnterScene(entt::entity player, entt::entity scene)
{
	EnterSeceneS2C message;
	message.mutable_scene_info()->CopyFrom(registry.get<SceneInfo>(scene));
	Send2Player(message, player);
}

void PlayerSceneSystem::LeaveScene(entt::entity player)
{
	LeaveSceneParam lp;
	lp.leaver_ = player;
	EntitySceneSystem::LeaveScene(player);
}

