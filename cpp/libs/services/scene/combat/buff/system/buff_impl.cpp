#include "buff_impl.h"

#include "table/code/buff_table.h"
#include "combat/buff/system/buff.h"
#include "combat_state/constants/combat_state.h"
#include "combat/buff/constants/buff.h"
#include "macros/return_define.h"
#include "proto/common/event/actor_combat_state_event.pb.h"
#include "time/system/time.h"
#include "core/utils/defer/defer.h"

bool BuffImplSystem::OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable) {
    if (!buffTable) return false;

    switch (buffTable->buff_type()) {
        case kBuffTypeNoDamageOrSkillHitInLastSeconds:
            return HandleIntervalNoDamageOrSkillHit(parent, buffComp, buffTable);
        default:
            return false;
    }
}

bool BuffImplSystem::OnBuffStart(entt::entity parent, const BuffEntry& buffComp, const BuffTable* buffTable) {
    if (!buffTable) return false;

    switch (buffTable->buff_type()) {
        case kBuffTypeSilence:
            return HandleBuffStartSilence(parent, buffComp);
        default:
            return false;
    }
}

bool BuffImplSystem::OnBuffDestroy(entt::entity parent, uint64_t buffId, const BuffTable* buffTable) {
    if (!buffTable) return false;

    switch (buffTable->buff_type()) {
        case kBuffTypeSilence:
            return HandleBuffDestroySilence(parent, buffId);
        default:
            return false;
    }
}

void BuffImplSystem::OnBeforeGiveDamage(entt::entity casterEntity, entt::entity targetEntity, DamageEventComp& damageEvent) {
    HandleBuffEffectsOnDamage(casterEntity, targetEntity, damageEvent);
}

void BuffImplSystem::OnSkillHit(entt::entity casterEntity, entt::entity targetEntity) {
    UpdateLastDamageOrSkillHitTime(casterEntity, targetEntity);
}

void BuffImplSystem::UpdateLastDamageOrSkillHitTime(entt::entity casterEntity, entt::entity targetEntity) {
    UInt64Set buffsToRemoveTarget;

    defer(BuffSystem::RemoveBuff(targetEntity, buffsToRemoveTarget));

    ECS_GET_OR_VOID(buffList, BuffListComp, targetEntity);
    for (auto &buffComp : *buffList | std::views::values)
    {
        LookupBuffOrContinue(buffComp.buffPb.buff_table_id());

        if (buffRow->buff_type() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
            if (const auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsComp>(buffComp.dataPbPtr)) {
                dataPtr->set_last_time(TimeSystem::NowMilliseconds());
            }

            BuffSystem::RemoveSubBuff(buffComp, buffsToRemoveTarget);
        }
    }
}

bool BuffImplSystem::HandleBuffStartSilence(entt::entity parent, const BuffEntry& buffComp) {
    CombatStateAddedEvent event;
    event.set_actor_entity(entt::to_integral(parent));
    event.set_source_buff_id(buffComp.buffPb.buff_id());
    event.set_state_type(kActorCombatStateSilence);

    tlsEcs.dispatcher.trigger(event);
    return true;
}

bool BuffImplSystem::HandleBuffDestroySilence(entt::entity parent, uint64_t buffId) {
    CombatStateRemovedEvent event;
    event.set_actor_entity(entt::to_integral(parent));
    event.set_source_buff_id(buffId);
    event.set_state_type(kActorCombatStateSilence);

    tlsEcs.dispatcher.trigger(event);
    return true;
}

bool BuffImplSystem::HandleIntervalNoDamageOrSkillHit(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable) {
    if (!buffTable || buffTable->combat_idle_seconds() <= 0) return false;

    auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsComp>(buffComp.dataPbPtr);
    if (!dataPtr) return false;

    auto elapsedTime = TimeSystem::NowMilliseconds() - dataPtr->last_time();
    if (static_cast<double>(elapsedTime) > buffTable->combat_idle_seconds()) {
        return true;
    }

    BuffSystem::AddSubBuffs(parent, buffTable, buffComp);
    return false;
}

void BuffImplSystem::HandleBuffEffectsOnDamage(entt::entity casterEntity, entt::entity targetEntity, DamageEventComp& damageEvent) {
    UInt64Set buffsToRemoveCaster;

    defer(BuffSystem::RemoveBuff(casterEntity, buffsToRemoveCaster));

    ECS_GET_OR_VOID(buffList, BuffListComp, casterEntity);
    for (auto &buffComp : *buffList | std::views::values)
    {
        LookupBuffOrContinue(buffComp.buffPb.buff_table_id());

        switch (buffRow->buff_type()) {
            case kBuffTypeNextBasicAttack:
                ApplyNextBasicAttackBuff(buffComp, buffRow, damageEvent, buffsToRemoveCaster, casterEntity, targetEntity);
                break;
            default:
                break;
        }
    }
}

void BuffImplSystem::ApplyNextBasicAttackBuff(BuffEntry& buffComp, const BuffTable* buffTable, DamageEventComp& damageEvent, UInt64Set& buffsToRemoveCaster, entt::entity casterEntity, entt::entity targetEntity) {
    const auto bonusDamage = BuffTableManager::Instance().GetBonusDamage(buffTable->id());
    damageEvent.set_damage(damageEvent.damage() + bonusDamage);

    buffsToRemoveCaster.emplace(buffComp.buffPb.buff_id());

    BuffSystem::AddSubBuffs(casterEntity, buffTable, buffComp);
    BuffSystem::AddTargetSubBuffs(targetEntity, buffTable, buffComp.skillContext);
}
