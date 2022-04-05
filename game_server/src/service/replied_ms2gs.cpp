#include "replied_ms2gs.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/network/ms_node.h"
#include "src/server_common/rpc_client.h"

using namespace common;

namespace ms2gs
{

void RepliedMs2g::StartGSMasterReplied(StartGameMasterRpcRC cp)
{
    auto rsp = cp->s_rp_;
    //LOG_INFO << "master server info " << rsp->DebugString().c_str();

    auto& scenemap = reg.get<SceneMapComp>(global_entity());
    for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
    {
        auto& pb = rsp->scenes_info(i);
        auto e = reg.create();
        reg.emplace<ConfigIdComp>(e, pb.scene_confid());
        reg.emplace<Guid>(e, pb.scene_id());
        scenemap.emplace(pb.scene_id(), e);
    }
}
}