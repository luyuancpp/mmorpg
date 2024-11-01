#include "motion_modifier_util.h"

#include "motion_modifier_impl_util.h"

bool MotionModifierBuffUtil::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    return true;
}

void MotionModifierBuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp)
{
}

bool MotionModifierBuffUtil::OnBuffRemove(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return  false;
}

bool MotionModifierBuffUtil::OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    if (buffTable == nullptr) {
        return false;
    }
    
    return  false;
}

bool MotionModifierBuffUtil::OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
{
    return MotionModifierBuffImpUtil::OnIntervalThink(parent, buffComp, buffTable);
}


void MotionModifierBuffUtil::OnSkillHit(entt::entity caster, entt::entity target)
{
    MotionModifierBuffImpUtil::OnSkillHit(caster, target);
}
