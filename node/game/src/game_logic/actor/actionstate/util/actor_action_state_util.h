#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class ActorActionStateUtil
{
public:
    static void InitializeActorComponents(entt::entity entity);

    static uint32_t TryToPerformAction(entt::entity actorEntity, uint32_t actorAction);

    static uint32_t CanPerformAction(entt::entity actorEntity, uint32_t actorAction);
    
    static bool IsInState(entt::entity actorEntity, uint32_t state);

    static uint32_t GetStatusCode(entt::entity actorEntity, uint32_t actorAction, uint32_t actorState);

    static uint32_t EnterState(entt::entity actorEntity, uint32_t actorState);

    static uint32_t ExitState(entt::entity actorEntity, uint32_t actorState);

    static uint32_t InterruptCurrentAction(entt::entity actorEntity, uint32_t actorAction);
};
