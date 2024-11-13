#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class ActorActionStateUtil
{
public:
    static void InitializeActorComponents(entt::entity entity);

    static uint32_t TryAction(entt::entity actorEntity, uint32_t action);

    static uint32_t CanDoAction(entt::entity actorEntity, uint32_t action);
    
    static bool InState(entt::entity actorEntity, uint32_t state);

    static uint32_t EnterState(entt::entity actorEntity, uint32_t state);

    static uint32_t InterruptAction(entt::entity actorEntity, uint32_t state);
};
