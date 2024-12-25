#include "motion_modifier_system.h"

#include "motion_modifier_impl_util.h"

bool MotionModifierBuffSystem::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    return true;
}

void MotionModifierBuffSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp)
{
}

bool MotionModifierBuffSystem::OnBuffRemove(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return  false;
}

bool MotionModifierBuffSystem::OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return  false;
}

bool MotionModifierBuffSystem::OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    return MotionModifierBuffImpUtil::OnIntervalThink(parent, buffComp, buffTable);
}


void MotionModifierBuffSystem::OnSkillHit(entt::entity caster, entt::entity target)
{
    MotionModifierBuffImpUtil::OnSkillHit(caster, target);
}
