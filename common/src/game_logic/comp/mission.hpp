#ifndef COMMON_SRC_GAME_LOIGC_COMP_H_
#define COMMON_SRC_GAME_LOIGC_COMP_H_

#include <unordered_map>

#include "src/game_logic/game_registry.h"

namespace common
{
using MissionMap = std::unordered_map<uint32_t, entt::entity>;

}//namespace common

#endif//COMMON_SRC_GAME_LOIGC_COMP_H_
