#pragma once

#include <unordered_map>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

using PlayerList = std::unordered_map<Guid, EntityPtr>;
extern thread_local PlayerList  g_players;

using PlayerList = std::unordered_map<Guid, EntityPtr>;
extern thread_local PlayerList  g_loading_players;