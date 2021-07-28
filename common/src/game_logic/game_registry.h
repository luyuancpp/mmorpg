#ifndef COMMON_SRC_GAME_ECS_GAME_REGISTRY_H_
#define COMMON_SRC_GAME_ECS_GAME_REGISTRY_H_

#include "entt/src/entt/entity/registry.hpp"

namespace common
{
entt::registry& reg();
entt::entity& error_entity();
}//namespace common

#endif//COMMON_SRC_GAME_ECS_GAME_REGISTRY_H_
