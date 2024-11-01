#pragma once
#include <entt/src/entt/entity/entity.hpp>

#include "buff_impl_util.h"

class MotionModifierBuffImpUtil
{
public:
    static bool OnIntervalThink(const entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable)
    {
        return false;
    }

    static void OnSkillHit(entt::entity casterEntity, entt::entity targetEntity)
    {
    }
};
