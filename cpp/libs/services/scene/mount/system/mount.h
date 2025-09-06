#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class MountSystem
{
public:
    static uint32_t MountActor(entt::entity actorEntity);        // 让角色上坐骑
    
    static uint32_t UnmountActor(entt::entity actorEntity);      // 让角色下坐骑

    static uint32_t ForceUnmountActor(entt::entity actorEntity); // 强制让角色下坐骑

    static uint32_t InterruptAndUnmountActor(entt::entity actorEntity); // 打断骑乘状态并下坐骑
};
