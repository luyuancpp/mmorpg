#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class ActorActionStateUtil
{
public:
    static void InitializeActorComponents(entt::entity entity);

    static uint32_t AddStateForAction(entt::entity actorEntity, uint32_t actorState);

    static uint32_t TryPerformAction(entt::entity actorEntity, uint32_t actorAction);

    static uint32_t CanExecuteActionWithoutStateChange(entt::entity actorEntity, uint32_t actorAction);
    
    static bool HasState(entt::entity actorEntity, uint32_t state);

    static uint32_t GetStateTip(uint32_t actorAction, uint32_t actorState);

    static uint32_t AddState(entt::entity actorEntity, uint32_t actorState);

    static uint32_t RemoveState(entt::entity actorEntity, uint32_t actorState);

};
