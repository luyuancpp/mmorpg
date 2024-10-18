#include "motion_modifier_util.h"

bool MotionModifierBuffUtil::OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable)
{
    return true;
}

void MotionModifierBuffUtil::OnBuffRefresh(entt::entity parent, uint32_t buffTableId,
    const SkillContextPtrComp& abilityContext, BuffComp& buffComp)
{
}

bool MotionModifierBuffUtil::OnBuffRemove(entt::entity parent, uint64_t buffId)
{
    return  false;
}

bool MotionModifierBuffUtil::OnBuffDestroy(entt::entity parent, uint32_t buffTableId)
{
    return  false;
}

void MotionModifierBuffUtil::ApplyMotion()
{

}

void MotionModifierBuffUtil::ModifyMotion()
{
}

