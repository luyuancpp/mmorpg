#pragma once

#include "buff_config.h"
#include "buff_util.h"
#include "common_error_tip.pb.h"
#include "game_logic/actor/combat_state/constants/combat_state_constants.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "proto/logic/event/actor_combat_state_event.pb.h"
#include "thread_local/storage.h"
#include "time/util/time_util.h"

class BuffImplUtil
{
public:

    static bool OnIntervalThink(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable) {
        if (buffTable && buffTable->bufftype() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
            return OnIntervalThinkLastDamageOrSkillHitTime(parent, buffComp, buffTable);
        }
        return false;
    }
    
    static bool OnBuffStart(const entt::entity parent, const BuffComp& buffComp, const BuffTable* buffTable){
        switch (buffTable->bufftype())
        {
        case kBuffTypeSilence:
            {
                CombatStateAddedPbEvent combatStateAddedPbEvent;
                combatStateAddedPbEvent.set_actor_entity(entt::to_integral(parent));
                combatStateAddedPbEvent.set_source_buff_id(buffComp.buffPb.buff_id());
                combatStateAddedPbEvent.set_state_type(kActorCombatStateSilence);
                
                tls.dispatcher.trigger(combatStateAddedPbEvent);
                return true;
            }
            break;

        default:
            break;
        }

        return false;
    }

    static bool OnBuffDestroy(const entt::entity parent, const uint64_t buffId, const BuffTable* buffTable)
    {
        switch (buffTable->bufftype())
        {
        case kBuffTypeSilence:
            {
                CombatStateRemovedPbEvent combatStateRemovedPbEvent;
                combatStateRemovedPbEvent.set_actor_entity(entt::to_integral(parent));
                combatStateRemovedPbEvent.set_source_buff_id(buffId);
                combatStateRemovedPbEvent.set_state_type(kActorCombatStateSilence);
                
                tls.dispatcher.trigger(combatStateRemovedPbEvent);
                return true;
            }
            break;

        default:
            break;
        }

        return false;
    }

    
    static void OnBeforeGiveDamage(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent) {
        UInt64Set removeBuffIdList;

        for (auto& buffComp : tls.registry.get<BuffListComp>(casterEntity) | std::views::values) {
           FetchBuffTableOrContinue(buffComp.buffPb.buff_table_id());

            switch (buffTable->bufftype()) {
            case kBuffTypeNextBasicAttack:
                {
                    const auto bonus_damage = BuffConfigurationTable::Instance().GetBonusdamage(buffTable->id());
                    damageEvent.set_damage(damageEvent.damage() + bonus_damage);
                    removeBuffIdList.emplace(buffComp.buffPb.buff_id());
                    
                    BuffUtil::AddSubBuffs(casterEntity, buffTable, buffComp);
                    BuffUtil::AddTargetSubBuffs(targetEntity, buffTable, buffComp.skillContext);
                }
                break;
            default:
                break;
            }
        }

        BuffUtil::RemoveBuff(casterEntity, removeBuffIdList);
    }

    static void OnSkillHit(const entt::entity casterEntity, const entt::entity targetEntity) {
        UpdateLastDamageOrSkillHitTime(casterEntity, targetEntity);
    }

    static void UpdateLastDamageOrSkillHitTime(const entt::entity casterEntity, const entt::entity targetEntity) {
        UInt64Set removeBuffIdList;

        for (auto& buffComp : tls.registry.get<BuffListComp>(targetEntity) | std::views::values) {
            FetchBuffTableOrContinue(buffComp.buffPb.buff_table_id());

            if (buffTable->bufftype() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
                if (const auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr)) {
                    dataPtr->set_last_time(TimeUtil::NowMilliseconds());
                }

                removeBuffIdList.emplace(buffComp.buffPb.buff_id());
            }
        }

        BuffUtil::RemoveBuff(casterEntity, removeBuffIdList);
    }


private:

    static bool OnIntervalThinkLastDamageOrSkillHitTime(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable) {
        if (buffTable->nodamageorskillhitinlastseconds() <= 0) {
            return false;
        }

        const auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
        if (!dataPtr) {
            return false;
        }

        if (static_cast<double>(TimeUtil::NowMilliseconds() - dataPtr->last_time()) > buffTable->nodamageorskillhitinlastseconds()) {
            return true;
        }

        BuffUtil::AddSubBuffs(parent, buffTable, buffComp);
        return true;
    }
};
