#pragma once

#include "table/code/buff_table.h"
#include "combat/buff/system/buff.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "combat_state/constants/combat_state.h"
#include "combat/buff/comp/buff.h"
#include "combat/buff/constants/buff.h"
#include "proto/common/event/actor_combat_state_event.pb.h"

#include "time/system/time.h"
#include "core/utils/defer/defer.h"
#include <thread_context/registry_manager.h>
#include "thread_context/dispatcher_manager.h"

// BuffImplSystem: Handles buff lifecycle logic
class BuffImplSystem {
public:
    // Periodic tick logic (per-frame or timer-triggered)
    static bool OnIntervalThink(const entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable) {
        if (!buffTable) return false;

        switch (buffTable->bufftype()) {
            case kBuffTypeNoDamageOrSkillHitInLastSeconds:
                return HandleIntervalNoDamageOrSkillHit(parent, buffComp, buffTable);
            default:
                return false;
        }
    }

    // On buff start
    static bool OnBuffStart(const entt::entity parent, const BuffEntry& buffComp, const BuffTable* buffTable) {
        if (!buffTable) return false;

        switch (buffTable->bufftype()) {
            case kBuffTypeSilence:
                return HandleBuffStartSilence(parent, buffComp);
            default:
                return false;
        }
    }

    // On buff destroy
    static bool OnBuffDestroy(const entt::entity parent, uint64_t buffId, const BuffTable* buffTable) {
        if (!buffTable) return false;

        switch (buffTable->bufftype()) {
            case kBuffTypeSilence:
                return HandleBuffDestroySilence(parent, buffId);
            default:
                return false;
        }
    }

    // Before damage is applied
    static void OnBeforeGiveDamage(
        const entt::entity casterEntity,
        const entt::entity targetEntity,
        DamageEventComp& damageEvent) {
        HandleBuffEffectsOnDamage(casterEntity, targetEntity, damageEvent);
    }

    // On skill hit
    static void OnSkillHit(const entt::entity casterEntity, const entt::entity targetEntity) {
        UpdateLastDamageOrSkillHitTime(casterEntity, targetEntity);
    }

    // Update last damage or skill hit timestamp
    static void UpdateLastDamageOrSkillHitTime(const entt::entity casterEntity, const entt::entity targetEntity) {
        UInt64Set buffsToRemoveTarget;

        defer(BuffSystem::RemoveBuff(targetEntity, buffsToRemoveTarget));
        
        for (auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(targetEntity);
            auto& buffComp : buffList | std::views::values) {
            FetchBuffTableOrContinue(buffComp.buffPb.buff_table_id());

            if (buffTable->bufftype() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
                if (const auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsComp>(buffComp.dataPbPtr)) {
                    dataPtr->set_last_time(TimeSystem::NowMilliseconds());
                }

                BuffSystem::RemoveSubBuff(buffComp, buffsToRemoveTarget);
            }
        }
    }
private:

    // Silence buff start handler
    static bool HandleBuffStartSilence(const entt::entity parent, const BuffEntry& buffComp) {
        CombatStateAddedPbEvent event;
        event.set_actor_entity(entt::to_integral(parent));
        event.set_source_buff_id(buffComp.buffPb.buff_id());
        event.set_state_type(kActorCombatStateSilence);

        dispatcher.trigger(event);
        return true;
    }

    // Silence buff destroy handler
    static bool HandleBuffDestroySilence(const entt::entity parent, const uint64_t buffId) {
        CombatStateRemovedPbEvent event;
        event.set_actor_entity(entt::to_integral(parent));
        event.set_source_buff_id(buffId);
        event.set_state_type(kActorCombatStateSilence);

        dispatcher.trigger(event);
        return true;
    }

    // No-damage-or-skill-hit duration check
    static bool HandleIntervalNoDamageOrSkillHit(
        const entt::entity parent,
        BuffEntry& buffComp,
        const BuffTable* buffTable
    ) {
        if (!buffTable || buffTable->nodamageorskillhitinlastseconds() <= 0) return false;

        auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsComp>(buffComp.dataPbPtr);
        if (!dataPtr) return false;

        auto elapsedTime = TimeSystem::NowMilliseconds() - dataPtr->last_time();
        if (static_cast<double>(elapsedTime) > buffTable->nodamageorskillhitinlastseconds()) {
            return true; // condition met
        }

        BuffSystem::AddSubBuffs(parent, buffTable, buffComp);
        return false;
    }

    // Apply buff effects to damage
    static void HandleBuffEffectsOnDamage(
        const entt::entity casterEntity,
        const entt::entity targetEntity,
        DamageEventComp& damageEvent
    ) {
        UInt64Set buffsToRemoveCaster;
        
        defer(BuffSystem::RemoveBuff(casterEntity, buffsToRemoveCaster));

        for (auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(casterEntity);
            auto& buffComp : buffList | std::views::values) {
           FetchBuffTableOrContinue(buffComp.buffPb.buff_table_id());

            switch (buffTable->bufftype()) {
                case kBuffTypeNextBasicAttack:
                    ApplyNextBasicAttackBuff(buffComp, buffTable, damageEvent, buffsToRemoveCaster, casterEntity, targetEntity);
                    break;
                default:
                    break;
            }
        }

    }

    // Next basic attack buff handler
    static void ApplyNextBasicAttackBuff(
        BuffEntry& buffComp,
		const BuffTable* buffTable,
        DamageEventComp& damageEvent,
        UInt64Set& buffsToRemoveCaster,
        const entt::entity casterEntity,
        const entt::entity targetEntity
    ) {
        const auto bonusDamage = BuffTableManager::Instance().GetBonusdamage(buffTable->id());
        damageEvent.set_damage(damageEvent.damage() + bonusDamage);

        buffsToRemoveCaster.emplace(buffComp.buffPb.buff_id());

        BuffSystem::AddSubBuffs(casterEntity, buffTable, buffComp);
        BuffSystem::AddTargetSubBuffs(targetEntity, buffTable, buffComp.skillContext);
    }



};

