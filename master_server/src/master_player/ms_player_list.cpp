#include "ms_player_list.h"

using namespace common;

    thread_local PlayerListMap  g_players;

    entt::entity PlayerList::GetPlayer(Guid guid)
    {
        auto it = g_players.find(guid);
        if (it == g_players.end())
        {
            return entt::null;
        }
        return it->second.entity();
    }

