#include "master_player_list.h"

namespace master
{

void MasterPlayerList::EnterGame(common::GameGuid player_id, entt::entity entity_id)
{
    player_list_.emplace(player_id, entity_id);
}

}//namespace master
