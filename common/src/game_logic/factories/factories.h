#ifndef COMMON_SRC_GAME_LOGIC_GAME_ENTITY_FACTORIES_H_
#define COMMON_SRC_GAME_LOGIC_GAME_ENTITY_FACTORIES_H_

#include "entt/src/entt/entity/fwd.hpp"

#include "src/common_type/common_type.h"

namespace common
{
entt::entity MakeMissionMap(entt::registry&);
entt::entity MakePlayerMission(entt::registry&);
entt::entity MakeMission(entt::registry&, entt::entity parent_id, uint32_t id);
entt::entity MakeRadomMission(entt::registry&, entt::entity parent_id, uint32_t id);

}//namespace common

#endif // !COMMON_SRC_GAME_LOGIC_GAME_ENTITY_FACTORIES_H_
