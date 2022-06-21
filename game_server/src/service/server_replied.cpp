#include "server_replied.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/ms_node.h"
#include "src/network/rpc_client.h"


void ServerReplied::StartGsMasterReplied(StartGsMasterRpcRpc replied)
{
    auto rsp = replied->s_rp_;
    for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
    {
        auto& pb = rsp->scenes_info(i);
		MakeSceneWithGuidP make_scene;
		make_scene.scene_id = pb.scene_id();
		make_scene.scene_confid_ = pb.scene_confid();
		ScenesSystem::GetSingleton().MakeSceneByGuid(make_scene);
    }
}

void ServerReplied::StartCrossGsRegionReplied(StartCrossGsReplied replied)
{
	auto rsp = replied->s_rp_;
	for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
	{
		auto& pb = rsp->scenes_info(i);
		MakeSceneWithGuidP make_scene;
		make_scene.scene_id = pb.scene_id();
		make_scene.scene_confid_ = pb.scene_confid();
		ScenesSystem::GetSingleton().MakeSceneByGuid(make_scene);
	}
}

