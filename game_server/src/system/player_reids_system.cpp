#include "player_reids_system.h"

#include "src/comp/player_list.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_session.h"
#include "src/network/gate_node.h"
#include "src/network/ms_node.h"
#include "src/network/session.h"
#include "src/system/player_network_system.h"

#include "component_proto/player_async_comp.pb.h"

PlayerDataRedisSystemPtr g_player_data_redis_system;

void OnAsyncLoadPlayerDatabase(Guid player_id, player_database& message)
{
    auto ret = g_players.emplace(player_id, EntityPtr());
    if (!ret.second)
    {
        LOG_ERROR << "server emplace error" << player_id;
        return;
    }
    // on loaded db
    entt::entity player = ret.first->second;
    registry.emplace<Guid>(player, player_id);
    registry.emplace<Vector3>(player, message.pos());
   	
    // on load db complete

   
}

