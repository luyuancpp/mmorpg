#pragma once
#include <cstdint>
#include "entt/src/entt/entity/entity.hpp"

// actor 属于 actorRegistry;派发和监听均在本进程 loop 线程。
// 必需派生投影已由创建流程直接初始化;此事件只通知观察者,不承担组件创建。
struct InitializeActorCompsEvent { entt::entity actor{entt::null}; };
struct CombatStateAddedEvent
{
    entt::entity actor{entt::null};
    uint64_t sourceBuffId{0};
    uint32_t stateType{0};
};
struct CombatStateRemovedEvent
{
    entt::entity actor{entt::null};
    uint64_t sourceBuffId{0};
    uint32_t stateType{0};
};
