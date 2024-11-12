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
        if (buffTable && buffTable->bufftype() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
            return OnIntervalThinkLastDamageOrSkillHitTime(parent, buffComp, buffTable);
        }
        return false;
    }

    static void OnBeforeGiveDamage(entt::entity parent, DamageEventPbComponent& damageEvent) {
        UInt64Set removeBuffIdList;

        for (auto& buffComp : tls.registry.get<BuffListComp>(parent) | std::views::values) {
            const BuffTable* buffTable = GetValidBuffTable(buffComp);
            if (!buffTable) continue;

            switch (buffTable->bufftype()) {
            case kBuffTypeNextBasicAttack:
                damageEvent.set_damage(damageEvent.damage() + buffTable->bonusdamage());
                removeBuffIdList.emplace(buffComp.buffPb.buff_id());
                BuffUtil::AddSubBuffs(parent, buffTable, buffComp);
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
            const BuffTable* buffTable = GetValidBuffTable(buffComp);
            if (!buffTable) continue;

            if (buffTable->bufftype() == kBuffTypeNoDamageOrSkillHitInLastSeconds) {
                auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
                if (dataPtr) {
                    removeBuffIdList.emplace(buffComp.buffPb.buff_id());
                    dataPtr->set_last_time(TimeUtil::NowMilliseconds());
                }
            }
        }

        BuffUtil::RemoveBuff(casterEntity, removeBuffIdList);
    }

private:

    static const BuffTable* GetValidBuffTable(const BuffComp& buffComp) {
        auto [buffTable, result] = GetBuffTable(buffComp.buffPb.buff_table_id());
        return buffTable;
    }

    static bool OnIntervalThinkLastDamageOrSkillHitTime(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable) {
        if (buffTable->nodamageorskillhitinlastseconds() <= 0) {
            return false;
        }

        auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
        if (!dataPtr) {
            return false;
        }

        auto currentTime = TimeUtil::NowMilliseconds();
        if (static_cast<double>(currentTime - dataPtr->last_time()) > buffTable->nodamageorskillhitinlastseconds()) {
            return true;
        }

        BuffUtil::AddSubBuffs(parent, buffTable, buffComp);
        return true;
    }
};
