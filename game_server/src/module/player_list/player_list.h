#ifndef GAME_SERVER_SRC_MODULE_PLAYEER_LIST_PLAYER_LIST_H_
#define GAME_SERVER_SRC_MODULE_PLAYEER_LIST_PLAYER_LIST_H_

#include <unordered_map>

#include "src/common_type/common_type.h"
#include "src/game_logic/entity_class/entity_class.h"

namespace game
{
using PlayerListMap = std::unordered_map<common::Guid, common::EntityPtr>;

extern thread_local PlayerListMap  g_players;
}

#endif//GAME_SERVER_SRC_MODULE_PLAYEER_LIST_PLAYER_LIST_H_
