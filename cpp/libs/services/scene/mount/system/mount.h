#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class MountSystem
{
public:
    static uint32_t MountActor(entt::entity actorEntity);
    
    static uint32_t UnmountActor(entt::entity actorEntity);

    static uint32_t ForceUnmountActor(entt::entity actorEntity);

    static uint32_t InterruptAndUnmountActor(entt::entity actorEntity);
};
