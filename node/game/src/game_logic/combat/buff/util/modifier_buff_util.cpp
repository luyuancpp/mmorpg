#include "modifier_buff_util.h"
#include "buff_config.pb.h"
#include "game_logic/actor/constants/actor_state_attribute_calculator_constants.h"
#include "game_logic/actor/util/actor_attribute_calculator_util.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"

bool ModifierBuffUtil::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorUtil::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

void ModifierBuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp) {
    // 当前未实现，可以添加逻辑来刷新 buff
}

bool ModifierBuffUtil::OnBuffRemove(const entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorUtil::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

bool ModifierBuffUtil::OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    // 当前未实现，可以添加销毁 buff 的逻辑

    if (buffTable == nullptr) {
        return false;
    }

    
    return false;
}

bool ModifierBuffUtil::OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    return true;
}

bool ModifierBuffUtil::IsMovementSpeedBuff(const BuffTable* buffTable) {
    return buffTable->bufftype() == kBuffTypeMovementSpeedReduction || 
           buffTable->bufftype() == kBuffTypeMovementSpeedBoost;
}
