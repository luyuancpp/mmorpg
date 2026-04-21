#include "motion_modifier_impl.h"

bool MotionModifierBuffImplSystem::OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
    return false;
}

void MotionModifierBuffImplSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffEntry& buffComp)
{
}

bool MotionModifierBuffImplSystem::OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return false;
}

bool MotionModifierBuffImplSystem::OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return false;
}

bool MotionModifierBuffImplSystem::OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable)
{
    return false;
}


void MotionModifierBuffImplSystem::OnSkillHit(entt::entity caster, entt::entity target)
{
}
