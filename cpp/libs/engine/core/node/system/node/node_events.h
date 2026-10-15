#pragma once
#include <cstdint>
#include "entt/src/entt/entity/entity.hpp"

// node 属于 tlsNodeContextManager.GetRegistry(nodeType),只在所属 loop 线程派发。
struct OnNodeRemoveEvent
{
    entt::entity node{entt::null};
    uint32_t nodeType{0};
    uint32_t sceneNodeType{0};
};
