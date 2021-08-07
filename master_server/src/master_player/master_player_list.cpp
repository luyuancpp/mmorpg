#include "master_player_list.h"

namespace master
{
    entt::entity MasterPlayerList::GetPlayer(common::GameGuid player_id)
    {
        auto it = player_list_.find(player_id);
        if (it == player_list_.end())
        {
            return entt::null;
        }
        return it->second;
    }

}//namespace master
