#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class ActorActionStateUtil
{
public:
    static void InitializeActorComponents(entt::entity entity);

    static uint32_t TryToPerformAction(entt::entity actorEntity, uint32_t action);

    static uint32_t CanPerformAction(entt::entity actorEntity, uint32_t action);
    
    static bool IsInState(entt::entity actorEntity, uint32_t state);

    static uint32_t EnterState(entt::entity actorEntity, uint32_t state);

    static uint32_t InterruptCurrentAction(entt::entity actorEntity, uint32_t state);
};
