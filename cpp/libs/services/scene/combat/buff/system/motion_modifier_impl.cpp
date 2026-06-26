#include "motion_modifier_impl.h"

void MotionModifierBuffImplSystem::OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
}

void MotionModifierBuffImplSystem::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffEntry& buffComp)
{
}

void MotionModifierBuffImplSystem::OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
}

void MotionModifierBuffImplSystem::OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable)
{
}

void MotionModifierBuffImplSystem::OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable)
{
}


void MotionModifierBuffImplSystem::OnSkillHit(entt::entity caster, entt::entity target)
{
}
