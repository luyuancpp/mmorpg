#ifndef COMMON_SRC_GAME_LOIGC_COMP_H_
#define COMMON_SRC_GAME_LOIGC_COMP_H_

#include <unordered_map>
#include <vector>

#include "src/game_logic/game_registry.h"

namespace common
{
enum eCondtionType : uint16_t
{
    E_CONDITION_KILL_MONSTER,
    E_CONDITION_TALK_WITH_NPC,
    E_CONDITION_COMPLELETE_CONDITION
};

using EntityV = std::vector<entt::entity>;
using TypeMissionEntityMap = std::unordered_map<eCondtionType, EntityV>;

}//namespace common

#endif//COMMON_SRC_GAME_LOIGC_COMP_H_
