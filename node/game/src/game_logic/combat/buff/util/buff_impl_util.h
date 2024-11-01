#pragma once
#include "buff_config.h"
#include "buff_util.h"
#include "common_error_tip.pb.h"
#include "proto/logic/component/actor_status_comp.pb.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"
#include "time/util/time_util.h"

class BuffImplUtil
{
public:
    static bool OnIntervalThink(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
    {
        switch (buffTable->bufftype())
        {
        case kBuffTypeNoDamageOrSkillHitInLastSeconds:
            {
                OnIntervalThinkLastDamageOrSkillHitTime(parent, buffComp, buffTable);
                return true;
            }
            break;
        default:
            return false;
            break;
        }

        return  false;
    }

    static void OnSkillHit(entt::entity casterEntity, entt::entity targetEntity){
        UpdateLastDamageOrSkillHitTime(casterEntity, targetEntity);
    }


    static void UpdateLastDamageOrSkillHitTime(const entt::entity casterEntity, const entt::entity targetEntity){
        UInt64Set removeBuffIdList;
        
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

                    removeBuffIdList.emplace(buffComp.buffPb.buff_id());
                    
                    dataPtr->set_last_time(TimeUtil::NowMilliseconds());
            }
                break;
            default:
                break;
            }
        }

        for (auto& removeBuffId : removeBuffIdList)
        {
            BuffUtil::RemoveBuff(targetEntity, removeBuffId);
        }
    }

    static bool OnIntervalThinkLastDamageOrSkillHitTime(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
    {
        if (buffTable == nullptr ||
            buffTable->nodamageorskillhitinlastseconds() <= 0 ) {
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
            auto [result, newBuffId] = BuffUtil::AddOrUpdateBuff(parent, subBuff, buffComp.skillContext);
            if (result != kOK || newBuffId == UINT64_MAX)
            {
                continue;
            }
            buffComp.buffPb.mutable_sub_buff_list_id()->emplace(newBuffId, false);
        }
        
        return true;
    }

    
};
