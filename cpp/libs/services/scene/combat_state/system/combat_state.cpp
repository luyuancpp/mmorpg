#include "combat_state.h"

#include <ranges>

#include "table/code/actoractioncombatstate_table.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "actor/attribute/system/actor_attribute_calculator.h"
#include "combat_state/constants/combat_state.h"
#include "proto/common/component/actor_combat_state_comp.pb.h"
#include "proto/common/event/actor_combat_state_event.pb.h"

#include <thread_context/registry_manager.h>

// Add combat state
void CombatStateSystem::AddCombatState(const CombatStateAddedPbEvent& addEvent) {
    const auto entityId = entt::to_entity(addEvent.actor_entity());
    auto& combatStateCollection = tlsRegistryManager.actorRegistry.get_or_emplace<CombatStateCollectionComp>(entityId);

    if (addEvent.state_type() >= kActorMaxCombatStateType) {
        return;
    }

    auto stateIterator = combatStateCollection.mutable_states()->find(addEvent.state_type());
    if (stateIterator == combatStateCollection.mutable_states()->end()) {
        const auto [newStateIterator, wasInserted] = combatStateCollection.mutable_states()->emplace(
            addEvent.state_type(), CombatStateDetailsComp{});
        if (!wasInserted) {
            return; 
        }
        stateIterator = newStateIterator;
    }

    stateIterator->second.mutable_sources()->emplace(addEvent.source_buff_id(), false);

    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(entityId, kCombatState);
}

// Remove combat state
void CombatStateSystem::RemoveCombatState(const CombatStateRemovedPbEvent& removeEvent) {
    const auto entityId = entt::to_entity(removeEvent.actor_entity());
    auto& combatStateCollection = tlsRegistryManager.actorRegistry.get_or_emplace<CombatStateCollectionComp>(entityId);

    if (removeEvent.state_type() >= kActorMaxCombatStateType) {
        return;
    }

    auto stateIterator = combatStateCollection.mutable_states()->find(removeEvent.state_type());
    if (stateIterator == combatStateCollection.mutable_states()->end()) {
        return;
    }

    stateIterator->second.mutable_sources()->erase(removeEvent.source_buff_id());

    if (stateIterator->second.sources().empty()) {
        combatStateCollection.mutable_states()->erase(stateIterator);
    }

    ActorAttributeCalculatorSystem::MarkAttributeForUpdate(entityId, kCombatState);
}

uint32_t CombatStateSystem::ValidateSkillUsage(const entt::entity entityId, const uint32_t combatAction)
{
    const auto& combatStateCollection = tlsRegistryManager.actorRegistry.get_or_emplace<CombatStateCollectionComp>(entityId);

    if (combatStateCollection.states().empty()) {
        return kSuccess;
    }

    FetchAndValidateActorActionCombatStateTable(combatAction);

    for (const auto& stateKey : combatStateCollection.states() | std::views::keys) {
        const auto& combatState = actorActionCombatStateTable->state(stateKey);

        if (combatState.state_mode() == kCombatStateMutualExclusion) {
            return combatState.state_tip();
        }
    }

    return kSuccess;
}

