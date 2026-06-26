#include "buff_impl.h"

#include "table/code/buff_table.h"
#include "combat/buff/system/buff.h"
#include "combat_state/constants/combat_state.h"
#include "combat/buff/constants/buff.h"
#include "macros/return_define.h"
#include "proto/common/event/actor_combat_state_event.pb.h"
#include "time/system/time.h"
#include "core/utils/defer/defer.h"

void BuffImplSystem::OnIntervalThink(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable) {    if (buffTable && buffTable->buff_type() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
        TickCombatIdleBuff(parent, buff, buffTable);
    }
}

void BuffImplSystem::OnBuffStart(entt::entity parent, const BuffEntry& buff, const BuffTable* buffTable) {
    if (buffTable && buffTable->buff_type() == kBuffTypeSilence) {
        AddSilenceState(parent, buff);
    }
}

void BuffImplSystem::OnBuffDestroy(entt::entity parent, uint64_t buffId, const BuffTable* buffTable) {
    if (buffTable && buffTable->buff_type() == kBuffTypeSilence) {
        RemoveSilenceState(parent, buffId);
    }
}

void BuffImplSystem::OnBeforeGiveDamage(entt::entity caster, entt::entity target, DamageEventComp& damageEvent) {
    // Apply any "next basic attack" bonus the caster is holding, then drop those buffs.
    UInt64Set buffsToRemove;
    defer(BuffSystem::RemoveBuff(caster, buffsToRemove));

    ECS_GET_OR_VOID(buffList, BuffListComp, caster);
    for (auto& buff : *buffList | std::views::values) {
        LookupBuffOrContinue(buff.buffPb.buff_table_id());
        if (buffRow->buff_type() == kBuffTypeNextBasicAttack) {
            ApplyNextBasicAttackBonus(caster, target, buff, buffRow, damageEvent, buffsToRemove);
        }
    }
}

void BuffImplSystem::OnSkillHit(entt::entity caster, entt::entity target) {
    ResetCombatIdleBuff(caster, target);
}

void BuffImplSystem::ResetCombatIdleBuff(entt::entity caster, entt::entity victim) {
    // The combat-idle buff grants an out-of-combat bonus; taking damage or being
    // hit by a skill resets its timer and strips the bonus sub-buffs.
    UInt64Set buffsToRemove;
    defer(BuffSystem::RemoveBuff(victim, buffsToRemove));

    ECS_GET_OR_VOID(buffList, BuffListComp, victim);
    for (auto& buff : *buffList | std::views::values) {
        LookupBuffOrContinue(buff.buffPb.buff_table_id());
        if (buffRow->buff_type() != kBuffTypeNoDamageOrSkillHitInLastSeconds) {
            continue;
        }
        if (const auto data = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsComp>(buff.dataPbPtr)) {
            data->set_last_time(TimeSystem::NowMilliseconds());
        }
        BuffSystem::RemoveSubBuff(buff, buffsToRemove);
    }
}

void BuffImplSystem::AddSilenceState(entt::entity parent, const BuffEntry& buff) {
    CombatStateAddedEvent event;
    event.set_actor_entity(entt::to_integral(parent));
    event.set_source_buff_id(buff.buffPb.buff_id());
    event.set_state_type(kActorCombatStateSilence);

    tlsEcs.dispatcher.trigger(event);
}

void BuffImplSystem::RemoveSilenceState(entt::entity parent, uint64_t buffId) {
    CombatStateRemovedEvent event;
    event.set_actor_entity(entt::to_integral(parent));
    event.set_source_buff_id(buffId);
    event.set_state_type(kActorCombatStateSilence);

    tlsEcs.dispatcher.trigger(event);
}

void BuffImplSystem::TickCombatIdleBuff(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable) {
    if (buffTable->combat_idle_seconds() <= 0) {
        return;
    }

    const auto data = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsComp>(buff.dataPbPtr);
    if (!data) {
        return;
    }

    const auto idleTime = TimeSystem::NowMilliseconds() - data->last_time();
    if (static_cast<double>(idleTime) > buffTable->combat_idle_seconds()) {
        return;  // idle long enough -- bonus already granted, nothing to do
    }

    BuffSystem::AddSubBuffs(parent, buffTable, buff);
}

void BuffImplSystem::ApplyNextBasicAttackBonus(entt::entity caster, entt::entity target, BuffEntry& buff,
                                               const BuffTable* buffTable, DamageEventComp& damageEvent,
                                               UInt64Set& buffsToRemove) {
    const auto bonusDamage = BuffTableManager::Instance().GetBonusDamage(buffTable->id());
    damageEvent.set_damage(damageEvent.damage() + bonusDamage);

    buffsToRemove.emplace(buff.buffPb.buff_id());

    BuffSystem::AddSubBuffs(caster, buffTable, buff);
    BuffSystem::AddTargetSubBuffs(target, buffTable, buff.skillContext);
}
