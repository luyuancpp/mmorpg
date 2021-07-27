#ifndef COMMON_SRC_GAME_LOGIC_MISSIONS_MISSION_H_
#define COMMON_SRC_GAME_LOGIC_MISSIONS_MISSION_H_

#include <unordered_map>

#include "src/game_ecs/game_registry.h"

namespace common
{

struct Missions 
{
public:
    using MissionsType = std::unordered_map<uint32_t, entt::entity>;


};

}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_MISSIONS_MISSION_H_