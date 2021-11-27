#include "replied_ms2g.h"

#include "muduo/base/Logging.h"

#include "src/comp/master.hpp"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/comp/server_list.hpp"
#include "src/game_logic/game_registry.h"

using namespace common;
using namespace game;

namespace ms2g
{
void RepliedMs2g::StartGameServerMasterReplied(StartGameMasterRpcRC cp)
{
    auto rsp = cp->s_resp_;
    //LOG_INFO << "master server info " << rsp->DebugString().c_str();
    for (auto e : reg().view<MasterClientPtr>())
    {
        auto& master_rpc_client = reg().get<MasterClientPtr>(e);
        if (cp->s_reqst_.master_server_addr() == (uint64_t)master_rpc_client.get())
        {
            reg().emplace<uint32_t>(e, rsp->master_node_id());
            LOG_INFO << "master server info " << rsp->master_node_id();
            break;
        }
    }

    auto& scenemap = reg().get<SceneMap>(game::global_entity());
    for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
    {
        auto& pb = rsp->scenes_info(i);
        auto e = reg().create();
        reg().emplace<SceneConfig>(e, pb.scene_config_id());
        reg().emplace<Guid>(e, pb.scene_id());
        scenemap.emplace(pb.scene_id(), e);
    }
}
}