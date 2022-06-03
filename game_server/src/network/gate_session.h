#pragma once

#include <unordered_map>

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

using GateSessionList = std::unordered_map<uint64_t, entt::entity>;
using SessionPlayerList = std::unordered_map <uint64_t, uint64_t>;
extern SessionPlayerList g_player_session_map;//ª·ª∞µΩplayer”≥…‰
extern GateSessionList g_gate_sessions;