#ifndef COMMON_SRC_GAME_LOGIC_GLOBAL_ENTITY_H_
#define COMMON_SRC_GAME_LOGIC_GLOBAL_ENTITY_H_

#include "game_registry.h"

namespace common
{
entt::entity& scenes_entity();
entt::entity& gameserver_entity();
}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_GLOBAL_ENTITY_H_
