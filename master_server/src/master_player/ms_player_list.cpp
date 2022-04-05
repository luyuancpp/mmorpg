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

	void PlayerList::EnterGame(common::Guid guid, const common::EntityPtr&& entity_id)
	{
		g_players.emplace(guid, entity_id);
        reg.emplace_or_replace<Guid>(entity_id.entity(), guid);
	}

