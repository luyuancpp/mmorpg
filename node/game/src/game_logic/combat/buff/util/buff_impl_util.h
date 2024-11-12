#pragma once

#include "buff_config.h"
#include "buff_util.h"
#include "common_error_tip.pb.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"
#include "time/util/time_util.h"

class BuffImplUtil
{
public:

    static bool OnIntervalThink(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable) {
        if (buffTable->bufftype() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
            OnIntervalThinkLastDamageOrSkillHitTime(parent, buffComp, buffTable);
            return true;
        }
        return false;
    }

    static void OnBeforeGiveDamage(entt::entity parent, DamageEventPbComponent& damageEvent) {
        UInt64Set removeBuffIdList;

        for (auto& buffComp : tls.registry.get<BuffListComp>(parent) | std::views::values) {
            auto [buffTable, result] = GetBuffTable(buffComp.buffPb.buff_table_id());
            if (buffTable == nullptr) {
                continue;
            }

            switch (buffTable->bufftype()) {
            case kBuffTypeNextBasicAttack: {
                    damageEvent.set_damage(damageEvent.damage() + buffTable->bonusdamage());
                    removeBuffIdList.emplace(buffComp.buffPb.buff_id());
                    BuffUtil::AddSubBuffs(parent, buffTable, buffComp);
            }
                break;
            default:
                break;
            }
        }

        BuffUtil::RemoveBuff(parent, removeBuffIdList);
    }

    static void OnSkillHit(entt::entity casterEntity, entt::entity targetEntity) {
        UpdateLastDamageOrSkillHitTime(casterEntity, targetEntity);
    }

    static void UpdateLastDamageOrSkillHitTime(const entt::entity casterEntity, const entt::entity targetEntity) {
        UInt64Set removeBuffIdList;
    
        for (auto& buffComp : tls.registry.get<BuffListComp>(targetEntity) | std::views::values) {
            auto [buffTable, result] = GetBuffTable(buffComp.buffPb.buff_table_id());
            if (buffTable == nullptr) {
                continue;
            }

            switch (buffTable->bufftype()) {
            case kBuffTypeNoDamageOrSkillHitInLastSeconds: {
                    auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
                    if (dataPtr == nullptr) {
                        continue;
                    }
                    removeBuffIdList.emplace(buffComp.buffPb.buff_id());
                    dataPtr->set_last_time(TimeUtil::NowMilliseconds());
            }
                break;
            default:
                break;
            }
        }

        BuffUtil::RemoveBuff(casterEntity, removeBuffIdList);
    }

    static bool OnIntervalThinkLastDamageOrSkillHitTime(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable) {
        if (buffTable == nullptr || buffTable->nodamageorskillhitinlastseconds() <= 0) {
            return false;
        }

        const auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
        if (dataPtr == nullptr) {
            return false;
        }

        if (static_cast<double>(TimeUtil::NowMilliseconds() - dataPtr->last_time()) > buffTable->nodamageorskillhitinlastseconds()) {
            return true;
        }

        BuffUtil::AddSubBuffs(parent, buffTable, buffComp);
        return true;
    }

};
