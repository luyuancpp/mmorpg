#include "master_player_list.h"

namespace master
{
    entt::entity MasterPlayerList::GetPlayer(common::Guid guid)
    {
        auto it = player_list_.find(guid);
        if (it == player_list_.end())
        {
            return entt::null;
        }
        return it->second;
    }

}//namespace master
