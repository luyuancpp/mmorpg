#ifndef COMMON_SRC_GAME_ENTITY_GAME_REGISTRY_H_
#define COMMON_SRC_GAME_ENTITY_GAME_REGISTRY_H_

#include "entt/src/entt/entity/registry.hpp"

namespace common
{
    static entt::registry& reg()
    {
        static entt::registry singleton;
        return singleton;
    }

}//namespace common

#endif//COMMON_SRC_GAME_ENTITY_GAME_REGISTRY_H_
