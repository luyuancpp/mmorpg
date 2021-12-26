#include "ms_player_list.h"

using namespace common;

namespace master
{
    entt::entity MasterPlayerList::GetPlayer(Guid guid)
    {
        auto it = player_list_.find(guid);
        if (it == player_list_.end())
        {
            return entt::null;
        }
        return it->second;
    }

}//namespace master
