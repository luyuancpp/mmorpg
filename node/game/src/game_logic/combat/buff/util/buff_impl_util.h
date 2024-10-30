#pragma once
#include "buff_config.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"
#include "time/util/time_util.h"

class BuffImplUtil
{
public:
    static void OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
    {
        if(OnIntervalThinkLastDamageOrSkillHitTime(parent, buffComp, buffTable)){
            return;
        }
    }

    static void UpdateLastDamageOrSkillHitTime(const entt::entity casterEntity, const entt::entity targetEntity){
        for (auto& buffComp : tls.registry.get<BuffListComp>(targetEntity) | std::views::values){
            auto [buffTable, result] = GetBuffTable(buffComp.buffPb.buff_table_id());
            if (buffTable == nullptr) {
                continue;
            }

            switch (buffTable->bufftype()) {
            case kBuffTypeNoDamageOrSkillHitInLastSeconds:{
                    auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
                    if (nullptr == dataPtr){
                        continue;
                    }
                
                    dataPtr->set_last_time(TimeUtil::NowMilliseconds());
            }
                break;
            default:
                break;
            }
        }
    }

    static bool OnIntervalThinkLastDamageOrSkillHitTime(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
    {
        if (buffTable == nullptr ||
            buffTable->nodamageorskillhitinlastseconds() <= 0 ||
            kBuffTypeNoDamageOrSkillHitInLastSeconds != buffTable->bufftype()) {
            return false;
        }

        const auto dataPtr = std::dynamic_pointer_cast<BuffNoDamageOrSkillHitInLastSecondsPbComp>(buffComp.dataPbPtr);
        if (nullptr == dataPtr){
            return false;
        }
                
        if ( static_cast<double>(TimeUtil::NowMilliseconds() - dataPtr->last_time()) > buffTable->nodamageorskillhitinlastseconds()){
            return  true;
        }

        for (auto& subBuff : buffTable->subbuff()){
            BuffUtil::AddOrUpdateBuff(parent, subBuff, buffComp.skillContext);
        }
        
        return true;
    }
};
