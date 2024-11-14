#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class MountUtil
{
public:
    static uint32_t MountActor(entt::entity actorEntity);     // 让角色上坐骑
    
    static uint32_t UnmountActor(entt::entity actorEntity);   // 让角色下坐骑
};

