#include "server_replied.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/gs_scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/network/ms_node.h"
#include "src/network/rpc_client.h"

using namespace common;


void ServerReplied::StartGSMasterReplied(StartGameMasterRpcRC cp)
{
    auto rsp = cp->s_rp_;
    auto& scenemap = reg.get<SceneMapComp>(global_entity());
    for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
    {
        auto& pb = rsp->scenes_info(i);
        auto e = reg.create();
        reg.emplace<SceneInfo>(e, pb);
        scenemap.emplace(pb.scene_id(), e);
    }
}

void ServerReplied::StartCrossMainGSRegionReplied(StartCrossMainGSReplied cp)
{
	auto rsp = cp->s_rp_;
	auto& scenemap = reg.get<SceneMapComp>(global_entity());
	for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
	{
		auto& pb = rsp->scenes_info(i);
		auto e = reg.create();
		reg.emplace<SceneInfo>(e, pb);
		scenemap.emplace(pb.scene_id(), e);
	}
}

void ServerReplied::StartCrossRoomGSRegionReplied(StartCrossRoomGSReplied cp)
{
	auto rsp = cp->s_rp_;
	auto& scenemap = reg.get<SceneMapComp>(global_entity());
	for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
	{
		auto& pb = rsp->scenes_info(i);
		auto e = reg.create();
		reg.emplace<SceneInfo>(e, pb);
		scenemap.emplace(pb.scene_id(), e);
	}
}
