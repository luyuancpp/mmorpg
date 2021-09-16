#include "replied_ms2g.h"

#include "muduo/base/Logging.h"

#include "src/comp/master.hpp"
#include "src/game_logic/game_registry.h"

using namespace game;

namespace ms2g
{
void RepliedMs2g::StartGameServerMasterReplied(StartGameMasterRpcRC cp)
{
    auto rsp = cp->s_resp_;
    for (auto e : common::reg().view<MasterClientPtr>())
    {
        auto& master_rpc_client = common::reg().get<MasterClientPtr>(e);
        if (cp->s_reqst_.master_server_addr() == (uint64_t)master_rpc_client.get())
        {
            common::reg().emplace<uint32_t>(e, rsp->master_node_id());
            LOG_INFO << "master server info " << rsp->master_node_id();
            break;
        }
    }
}
}