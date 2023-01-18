#pragma once

#include "src/common_type/common_type.h"
#include "src/game_logic/thread_local/game_registry.h"

using PlayerListMap = std::unordered_map<Guid, EntityPtr>;
extern thread_local PlayerListMap  g_players;
class PlayerList
{
public:
    std::size_t player_size()const { return g_players.size(); }
    bool empty()const { return g_players.empty(); }
    entt::entity GetPlayer(Guid guid);
    EntityPtr GetPlayerPtr(Guid guid);
    bool HasPlayer(Guid guid) const { return g_players.find(guid) != g_players.end(); }

    EntityPtr EnterGame(Guid guid);
    void LeaveGame(Guid guid);
};
extern PlayerList* g_player_list;
