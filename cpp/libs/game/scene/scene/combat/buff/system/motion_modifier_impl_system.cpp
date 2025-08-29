#include "motion_modifier_impl_system.h"


bool MotionModifierBuffImplSystem::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTableTempPtr& buffTable)
{
    return true;
}

void MotionModifierBuffImplSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp)
{
}

bool MotionModifierBuffImplSystem::OnBuffRemove(entt::entity parent, BuffComp& buff, const BuffTableTempPtr& buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return  false;
}

bool MotionModifierBuffImplSystem::OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTableTempPtr& buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return  false;
}

bool MotionModifierBuffImplSystem::OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTableTempPtr& buffTable)
{
    return false;
}


void MotionModifierBuffImplSystem::OnSkillHit(entt::entity caster, entt::entity target)
{
}
