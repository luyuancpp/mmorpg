#ifndef COMMON_SRC_GAME_LOGIC_GAME_ENTITY_FACTORIES_H_
#define COMMON_SRC_GAME_LOGIC_GAME_ENTITY_FACTORIES_H_

#include "entt/src/entt/entity/fwd.hpp"

#include "src/common_type/common_type.h"

namespace common
{
entt::entity MakeMission(entt::registry&, uint32_t id);

}//namespace common

#endif // !COMMON_SRC_GAME_LOGIC_GAME_ENTITY_FACTORIES_H_
