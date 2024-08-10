#include "ability_util.h"

#include <entt/src/entt/entity/entity.hpp>

#include "ability_config.h"
#include "pbc/ability_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "thread_local/storage.h"

uint32_t AbilityUtil::CheckSkillActivationPrerequisites(const ::UseAbilityRequest* request)
{
    auto tableAbility = GetAbilityTable(request->ability_id());

    if (tableAbility == nullptr) {
        return kInvalidTableId;
    }

    const bool hasValidTargetType = !tableAbility->target_type().empty();
    const auto targetId = request->target_id();

    if (hasValidTargetType) {
        if (targetId <= 0) {
            return kAbilityInvalidTargetId;
        } else {
            // 创建目标实体并检查其有效性
            entt::entity target{targetId};
            if (tls.registry.valid(target)) {
                return kAbilityInvalidTarget;
            }
        }
    }

    return kOK;
}

void AbilityUtil::HandleAbilityInitialize()
{

}

void AbilityUtil::HandleAbilityStart()
{

}

void AbilityUtil::HandleAbilitySpell()
{

}

void AbilityUtil::HandleAbilityFinish()
{

}

void AbilityUtil::HandleChannelStart()
{

}

void AbilityUtil::HandleChannelThink()
{

}

void AbilityUtil::HandleChannelFinish()
{

}

void AbilityUtil::HandleAbilityToggleOn()
{

}

void AbilityUtil::HandleAbilityToggleOff()
{

}

void AbilityUtil::HandleAbilityActivate()
{

}

void AbilityUtil::HandleAbilityDeactivate()
{

}
