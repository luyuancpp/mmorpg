#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"

#include "src/pb/pbc/component_proto/player_async_comp.pb.h"


void PlayerSceneSystem::EnterScene(entt::entity player, const EnterGsInfo& enter_info, uint64_t session_id)
{
	//��һ�ν�������,����gate��û����
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

	//todo������gate Ȼ��ſ��Կ�ʼ���Ը��ͻ��˷�����Ϣ��, gs��Ϣ˳������Ҫע�⣬����a, �ٽ���b gs����ͻ�����Ϣ��˳��һ��

}

