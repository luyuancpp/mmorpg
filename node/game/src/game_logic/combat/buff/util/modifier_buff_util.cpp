#include "modifier_buff_util.h"

#include "buff_config.h"
#include "buff_config.pb.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"

bool ModifierBuffUtil::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    if (nullptr == buffTable){
        return  false;
    }
    
    if (buffTable->bufftype() == kBuffTypeMovementSpeedReduction ||
        buffTable->bufftype() == kBuffTypeMovementSpeedBoost){
        return  true;
    }
    
    return  false;
}

void ModifierBuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp)
{
}

bool ModifierBuffUtil::OnBuffRemove(entt::entity parent, uint64_t buffId)
{
    auto& buffList = tls.registry.get<BuffListComp>(parent);
    const auto buffIt = buffList.find(buffId);

    if (buffIt == buffList.end()) {
        LOG_ERROR << "Cannot find buff " << buffId;
        return false;
    }

    auto [buffTable, result] = GetBuffTable(buffIt->second.buffPb.buff_table_id());
    if (nullptr == buffTable){
        return  false;
    }
    
     if (buffTable->bufftype() == kBuffTypeMovementSpeedReduction ||
        buffTable->bufftype() == kBuffTypeMovementSpeedBoost){
        return  true;
    }

    return false;
}

bool ModifierBuffUtil::OnBuffDestroy(entt::entity parent, uint32_t buffTableId)
{
    return  false;
}

void ModifierBuffUtil::ModifyState(entt::entity parent, uint64_t buffId){
}

void ModifierBuffUtil::ModifyAttributes(entt::entity parent, uint64_t buffId){
}