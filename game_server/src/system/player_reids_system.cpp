#include "player_reids_system.h"

#include "src/comp/player_list.h"
#include "src/game_logic/game_registry.h"

PlayerDataRedisSystemPtr g_player_data_redis_system;

void OnAsyncLoadPlayerDatabase(Guid player_id, player_database& message)
{
    // on load db
    auto ret = g_players.emplace(player_id, EntityPtr());
    if (!ret.second)
    {
        LOG_ERROR << "server emplace erro" << player_id;
        return;
    }
    auto player = ret.first->second.entity();
    reg.emplace<Guid>(player, player_id);
    reg.emplace<Vector3>(player, message.pos());
    // on load db complete

}

