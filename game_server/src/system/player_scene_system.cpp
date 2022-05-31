#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"

#include "src/pb/pbc/component_proto/player_async_comp.pb.h"


void PlayerSceneSystem::EnterScene(entt::entity player, const EnterGsInfo& enter_info, uint64_t session_id)
{
	//第一次进入世界,但是gate还没进入
	auto scene = ScenesSystem::GetSingleton().get_scene(enter_info.scenes_info().scene_id());
	if (scene == entt::null)
	{
		LOG_ERROR << "scene not found " << enter_info.scenes_info().scene_id() <<  "," << enter_info.scenes_info().scene_confid();
		return;
	}
	EnterSceneParam ep;
	ep.enterer_ = player;
	ep.scene_ = scene;
	ScenesSystem::GetSingleton().EnterScene(ep);

	//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样

}

