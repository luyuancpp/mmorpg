#include "player_reids_system.h"

#include "src/comp/player_list.h"
#include "src/game_logic/game_registry.h"

PlayerDataRedisSystemPtr g_player_data_redis_system;

void OnAsyncLoadPlayerDatabase(Guid guid, player_database& message)
{
    // on load db
    auto ret = g_players.emplace(guid, EntityPtr());
    if (!ret.second)
    {
        LOG_ERROR << "server emplace erro" << guid;
        return;
    }
    auto player = ret.first->second.entity();
    reg.emplace<Guid>(player, guid);
    reg.emplace<Vector3>(player, message.pos());
    // on load db complete

}

