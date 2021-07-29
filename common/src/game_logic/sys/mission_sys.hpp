#ifndef COMMON_SRC_GAME_LOGIC_SYS_MISSION_SYS_H_
#define COMMON_SRC_GAME_LOGIC_SYS_MISSION_SYS_H_

#include "entt/src/entt/entity/fwd.hpp"

#include "src/game_logic/op_code.h"

namespace common
{

void CompleteAllMission(entt::entity, uint32_t op);

}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_SYS_MISSION_SYS_H_
