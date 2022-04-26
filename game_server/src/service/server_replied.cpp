#include "server_replied.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/gs_scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene_factories.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/ms_node.h"
#include "src/network/rpc_client.h"

using namespace common;


void ServerReplied::StartGSMasterReplied(StartGsMasterRpcRC cp)
{
    auto rsp = cp->s_rp_;
    for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
    {
        auto& pb = rsp->scenes_info(i);
		MakeSceneWithGuidP make_scene;
		make_scene.scene_id = pb.scene_id();
		make_scene.scene_confid_ = pb.scene_confid();
		g_scene_sys->MakeSceneByGuid(make_scene);
    }
}

void ServerReplied::StartCrossGsRegionReplied(StartCrossGsReplied cp)
{
	auto rsp = cp->s_rp_;
	for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
	{
		auto& pb = rsp->scenes_info(i);
		MakeSceneWithGuidP make_scene;
		make_scene.scene_id = pb.scene_id();
		make_scene.scene_confid_ = pb.scene_confid();
		g_scene_sys->MakeSceneByGuid(make_scene);
	}
}

