#include "actor_attribute_calculator.h"

#include <array>
#include <ranges>

#include "actor/attribute/comp/actor_attribute_comp.h"
#include "macros/return_define.h"

#include "table/code/buff_table.h"
#include "actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "combat/buff/comp/buff_comp.h"
#include "player/comp/player_frozen_comp.h"  // cross-zone-readiness-audit.md §11.2
#include "proto/scene/player_state_attribute_sync.pb.h"
#include "proto/common/component/actor_combat_state_comp.pb.h"
#include <generated/attribute/actorbaseattributess2c_attribute_sync.h>
#include <thread_context/ecs_context.h>  // tlsEcs + entt::exclude (see movement.cpp pattern)

void UpdateVelocity(entt::entity entity) {
    auto& velocity = tlsEcs.actorRegistry.get_or_emplace<Velocity>(entity);
    velocity.Clear();

    ECS_GET_OR_VOID(buffListPtr, BuffListComp, entity);
    for (const auto &buffCompPb : *buffListPtr | std::views::values)
    {
        LookupBuffOrContinue(buffCompPb.buffPb.buff_table_id());

        velocity.set_x(velocity.x() + buffRow->movement_speed_boost());
        velocity.set_y(velocity.y() + buffRow->movement_speed_boost());
        velocity.set_z(velocity.z() + buffRow->movement_speed_boost());

        velocity.set_x(velocity.x() - buffRow->movement_speed_reduction());
        velocity.set_y(velocity.y() - buffRow->movement_speed_reduction());
        velocity.set_z(velocity.z() - buffRow->movement_speed_reduction());
    }

    // Set runtime dirty bit (do not write dirty bits back to persisted proto)
    SetActorBaseAttributesS2CAttrDirtyBit(entity, static_cast<std::size_t>(ActorBaseAttributesS2C::kVelocityFieldNumber));
}

void UpdateHealth(entt::entity actorEntity) {
    // TODO: Implement health recalculation from base stats + buff modifiers
}

void UpdateEnergy(entt::entity actorEntity) {
    // TODO: Implement energy recalculation from base stats + buff modifiers
}

void ResetCombatStateFlags(entt::entity actorEntity) {
    const auto *combatStates = tlsEcs.actorRegistry.try_get<CombatStateCollectionComp>(actorEntity);
    auto& syncData = tlsEcs.actorRegistry.get_or_emplace<ActorBaseAttributesS2C>(actorEntity);
    auto* stateFlags = syncData.mutable_combat_state_flags()->mutable_state_flags();

    stateFlags->clear();

    if (combatStates)
    {
        for (const auto &stateKey : combatStates->states() | std::views::keys)
        {
            stateFlags->emplace(stateKey, false);
        }
    }
}

std::array<AttributeCalculatorConfig, kAttributeCalculatorMax> kAttributeConfigs = { {
    {kVelocity, UpdateVelocity},
    {kHealth, UpdateHealth},
    {kEnergy, UpdateEnergy},
    {kCombatState, ResetCombatStateFlags}
} };

void ActorAttributeCalculatorSystem::MarkAttributeForUpdate(const entt::entity actorEntity, const uint32_t attributeBit) {
    auto& attributeBits = tlsEcs.actorRegistry.get_or_emplace<AttributeDirtyFlagsComp>(actorEntity).attributeBits;
    attributeBits.set(attributeBit);
}

void ActorAttributeCalculatorSystem::Update()
{
    // PlayerFrozenComp exclude: a frozen player's attributes were
    // marshalled into PlayerAllData at HandleCrossZoneTransfer time.
    // Recomputing them on the source side now is wasted work — and
    // worse, the values would diverge from what the destination zone
    // reconstructs from the snapshot. Skip frozen entities until the
    // ACK / reaper-declared-failure clears the marker.
    // cross-zone-readiness-audit.md §11.2.
    for (auto&& [entity, dirtyFlags] : tlsEcs.actorRegistry.view<AttributeDirtyFlagsComp>(
            entt::exclude<PlayerFrozenComp>).each())
    {
        auto& attributeBits = dirtyFlags.attributeBits;
        for (const auto& [attributeIndex, updateFunction] : kAttributeConfigs) {
            if (updateFunction && attributeBits.test(attributeIndex)) {
                updateFunction(entity);
                attributeBits.reset(attributeIndex);
            }
        }
    }
}
