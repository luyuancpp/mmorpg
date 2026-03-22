#include "actor_action_state.h"

#include <ranges>

#include "table/code/actoractionstate_table.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "actor/action_state/constants/actor_state.h"
#include "engine/core/macros/return_define.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/event/actor_event.pb.h"
#include "engine/thread_context/dispatcher_manager.h"

namespace {
    // Check if an action conflicts with the current state; return error code on conflict
    uint32_t CheckForStateConflict(const ActorActionStateTable* actorActionStateTable, uint32_t actorState) {
        if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
            return kSuccess;
        }

        if (const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
            state.state_mode() == kActionStateMutualExclusion) {
            return state.state_tip();
        }
        return kSuccess;
    }

    // Interrupt current state and perform the action
    bool InterruptAndPerformAction(const ActorActionStateTable* actorActionStateTable, uint32_t actorState, entt::entity actorEntity) {
        if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
            return false;
        }

        if (const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
            state.state_mode() == kActionStateInterrupt) {
            // Trigger interrupt event
            InterruptCurrentStatePbEvent interruptEvent;
            interruptEvent.set_actor_entity(entt::to_integral(actorEntity));
            interruptEvent.set_actor_state(actorState);
            dispatcher.trigger(interruptEvent);

            RETURN_FALSE_ON_ERROR(ActorActionStateSystem::RemoveState(actorEntity, actorState));
            
            return true;
        }

        return false;
    }
}

uint32_t ActorActionStateSystem::TryPerformAction(entt::entity actorEntity, uint32_t actorAction, uint32_t successState) {
    FetchAndValidateActorActionStateTable(actorAction);

    const auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get_or_emplace<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        RETURN_ON_ERROR(CheckForStateConflict(actorActionStateTable, actorState));
    }

    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        if (InterruptAndPerformAction(actorActionStateTable, actorState, actorEntity)) {
            continue;
        }
    }

    RETURN_ON_ERROR(AddState(actorEntity, successState));

    return kSuccess;
}


uint32_t ActorActionStateSystem::CanExecuteActionWithoutStateChange(entt::entity actorEntity, uint32_t actorAction) {
    FetchAndValidateActorActionStateTable(actorAction);
    
    const auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get_or_emplace<ActorStatePbComponent>(actorEntity);
    for (const auto& actorState : actorStatePbComponent.state_list() | std::views::keys) {
        RETURN_ON_ERROR(CheckForStateConflict(actorActionStateTable, actorState));
    }

    return kSuccess;
}

bool ActorActionStateSystem::HasState(const entt::entity actorEntity, const uint32_t state) {
    const auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get_or_emplace<ActorStatePbComponent>(actorEntity);
    if (state >= kActorStateActorStateMax) {
        return false;
    }

    return actorStatePbComponent.state_list().contains(state);
}

uint32_t ActorActionStateSystem::GetStateTip(const uint32_t actorAction, const uint32_t actorState) {
    FetchAndValidateActorActionStateTable(actorAction);

    if (actorState >= static_cast<uint32_t>(actorActionStateTable->state_size())) {
        return kInvalidParameter;
    }

    const auto& state = actorActionStateTable->state(static_cast<int32_t>(actorState));
    return state.state_tip();
}

uint32_t ActorActionStateSystem::AddState(const entt::entity actorEntity, uint32_t actorState) {
    auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get_or_emplace<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax){
        return kInvalidParameter; 
    }

    if (actorStatePbComponent.state_list().contains(actorState)){
        return kSuccess;
    }

    actorStatePbComponent.mutable_state_list()->emplace(std::make_pair(actorState, true));
    return kSuccess;
}

uint32_t ActorActionStateSystem::RemoveState(entt::entity actorEntity, uint32_t actorState) {
    auto& actorStatePbComponent = tlsRegistryManager.actorRegistry.get_or_emplace<ActorStatePbComponent>(actorEntity);
    if (actorState >= kActorStateActorStateMax ||
        !actorStatePbComponent.state_list().contains(actorState)) {
        return kInvalidParameter;
    }

    actorStatePbComponent.mutable_state_list()->erase(actorState);
    return kSuccess;
}

