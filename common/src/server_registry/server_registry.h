#ifndef COMMON_SRC_SERVER_REGISTRY_SERVER_REGISTRY_LIST_H_
#define COMMON_SRC_SERVER_REGISTRY_SERVER_REGISTRY_LIST_H_

#include "entt/src/entt/entity/registry.hpp"

namespace common
{
    static entt::registry& reg()
    {
        static entt::registry singleton;
        return singleton;
    }
}//namespace common

#endif//COMMON_SRC_SERVER_REGISTRY_SERVER_REGISTRY_LIST_H_
