#ifndef GAME_SERVER_SRC_MODULE_PLAYEER_LIST_PLAYER_LIST_H_
#define GAME_SERVER_SRC_MODULE_PLAYEER_LIST_PLAYER_LIST_H_

#include <unordered_map>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

using PlayerList = std::unordered_map<Guid, EntityPtr>;
extern thread_local PlayerList  g_players;

#endif//GAME_SERVER_SRC_MODULE_PLAYEER_LIST_PLAYER_LIST_H_
