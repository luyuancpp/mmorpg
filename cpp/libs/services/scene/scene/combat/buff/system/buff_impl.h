#pragma once

#include "table/code/buff_table.h"
#include "scene/combat/buff/system/buff.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "scene//combat_state/constants/combat_state.h"
#include "scene/combat/buff/comp/buff.h"
#include "scene/combat/buff/constants/buff.h"
#include "proto/logic/event/actor_combat_state_event.pb.h"

#include "time/system/time.h"
#include "core/utils/defer/defer.h"
#include <threading/registry_manager.h>
#include "threading/dispatcher_manager.h"

// BuffImplSystem: Buff逻辑工具类，用于处理各种Buff生命周期相关的逻辑
class BuffImplSystem {
public:
    // 定期调用逻辑 (每帧或定时触发)
    static bool OnIntervalThink(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable) {
        if (!buffTable) return false;

        switch (buffTable->bufftype()) {
            case kBuffTypeNoDamageOrSkillHitInLastSeconds:
                return HandleIntervalNoDamageOrSkillHit(parent, buffComp, buffTable);
            default:
                return false;
        }
    }

    // Buff开始时的逻辑
    static bool OnBuffStart(const entt::entity parent, const BuffComp& buffComp, const BuffTable* buffTable) {
        if (!buffTable) return false;

        switch (buffTable->bufftype()) {
            case kBuffTypeSilence:
                return HandleBuffStartSilence(parent, buffComp);
            default:
                return false;
        }
    }

    // Buff销毁时的逻辑
    static bool OnBuffDestroy(const entt::entity parent, uint64_t buffId, const BuffTable* buffTable) {
        if (!buffTable) return false;

        switch (buffTable->bufftype()) {
            case kBuffTypeSilence:
                return HandleBuffDestroySilence(parent, buffId);
            default:
                return false;
        }
    }

    // 伤害结算前的逻辑
    static void OnBeforeGiveDamage(
        const entt::entity casterEntity,
        const entt::entity targetEntity,
        DamageEventPbComponent& damageEvent) {
        HandleBuffEffectsOnDamage(casterEntity, targetEntity, damageEvent);
    }

    // 技能命中时的逻辑
    static void OnSkillHit(const entt::entity casterEntity, const entt::entity targetEntity) {
        UpdateLastDamageOrSkillHitTime(casterEntity, targetEntity);
    }

    // 更新最后一次伤害或技能命中时间
    static void UpdateLastDamageOrSkillHitTime(const entt::entity casterEntity, const entt::entity targetEntity) {
        UInt64Set buffsToRemoveTarget;

        defer(BuffSystem::RemoveBuff(targetEntity, buffsToRemoveTarget));
        
        for (auto& buffList = tlsRegistryManager.actorRegistry.get_or_emplace<BuffListComp>(targetEntity);
            auto& buffComp : buffList | std::views::values) {
            FetchBuffTableOrContinue(buffComp.buffPb.buff_table_id());

            if (buffTable->bufftype() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
                if (const auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr)) {
                    dataPtr->set_last_time(TimeSystem::NowMilliseconds());
                }

                BuffSystem::RemoveSubBuff(buffComp, buffsToRemoveTarget);
            }
        }
    }
private:
    // **具体实现部分**

    // Silence Buff 开始逻辑
    static bool HandleBuffStartSilence(const entt::entity parent, const BuffComp& buffComp) {
        CombatStateAddedPbEvent event;
        event.set_actor_entity(entt::to_integral(parent));
        event.set_source_buff_id(buffComp.buffPb.buff_id());
        event.set_state_type(kActorCombatStateSilence);

        dispatcher.trigger(event);
        return true;
    }

    // Silence Buff 销毁逻辑
    static bool HandleBuffDestroySilence(const entt::entity parent, const uint64_t buffId) {
        CombatStateRemovedPbEvent event;
        event.set_actor_entity(entt::to_integral(parent));
        event.set_source_buff_id(buffId);
        event.set_state_type(kActorCombatStateSilence);

        dispatcher.trigger(event);
        return true;
    }

    // 无伤害或技能命中时间检查逻辑
    static bool HandleIntervalNoDamageOrSkillHit(
        const entt::entity parent,
        BuffComp& buffComp,
        const BuffTable* buffTable
    ) {
        if (!buffTable || buffTable->nodamageorskillhitinlastseconds() <= 0) return false;

        auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
        if (!dataPtr) return false;

        auto elapsedTime = TimeSystem::NowMilliseconds() - dataPtr->last_time();
        if (static_cast<double>(elapsedTime) > buffTable->nodamageorskillhitinlastseconds()) {
            return true; // 条件满足
        }

        BuffSystem::AddSubBuffs(parent, buffTable, buffComp);
        return false;
    }

    // Buff对伤害的影响
    static void HandleBuffEffectsOnDamage(
        const entt::entity casterEntity,
        const entt::entity targetEntity,
        DamageEventPbComponent& damageEvent
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

    // 处理下一次基础攻击的 Buff 逻辑
    static void ApplyNextBasicAttackBuff(
        BuffComp& buffComp,
		const BuffTable* buffTable,
        DamageEventPbComponent& damageEvent,
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
