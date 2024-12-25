#include "modifier_buff_system.h"
#include "buff_config.pb.h"
#include "modifier_buff_impl_system.h"
#include "game_logic/actor/attribute/constants/actor_state_attribute_calculator_constants.h"
#include "game_logic/actor/attribute/system/actor_attribute_calculator_system.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "game_logic/combat/buff/constants/buff_constants.h"
#include "thread_local/storage.h"

bool ModifierBuffSystem::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorSystem::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

void ModifierBuffSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp) {
    // 当前未实现，可以添加逻辑来刷新 buff
}

bool ModifierBuffSystem::OnBuffRemove(const entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    if (buffTable == nullptr) {
        return false;
    }

    if(IsMovementSpeedBuff(buffTable))
    {
        ActorAttributeCalculatorSystem::MarkAttributeForUpdate(parent, kVelocity);
        return  true;
    }

    return  false;
}

bool ModifierBuffSystem::OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTable* buffTable) {
    // 当前未实现，可以添加销毁 buff 的逻辑

    if (buffTable == nullptr) {
        return false;
    }

    
    return false;
}

bool ModifierBuffSystem::OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    return ModifierBuffUtilImplSystem::OnIntervalThink(parent, buffComp, buffTable);
}

void ModifierBuffSystem::OnSkillHit(entt::entity caster, entt::entity target)
{
    ModifierBuffUtilImplSystem::OnSkillHit(caster, target);
}

bool ModifierBuffSystem::IsMovementSpeedBuff(const BuffTable* buffTable) {
    return buffTable->bufftype() == kBuffTypeMovementSpeedReduction || 
           buffTable->bufftype() == kBuffTypeMovementSpeedBoost;
}
