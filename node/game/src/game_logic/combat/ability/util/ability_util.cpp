#include "ability_util.h"

#include <entt/src/entt/entity/entity.hpp>

#include "ability_config.h"
#include "game_logic/combat/ability/comp/ability_comp.h"
#include "game_logic/scene/util/view_util.h"
#include "pbc/ability_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "service/player_ability_service.h"
#include "thread_local/storage.h"
#include "time/comp/timer_task_comp.h"

uint32_t AbilityUtil::CheckSkillActivationPrerequisites(entt::entity caster, const ::UseAbilityRequest* request)
{
    // 从请求中获取技能表
    auto tableAbility = GetAbilityTable(request->ability_id());
    if (tableAbility == nullptr) {
        return kInvalidTableId; // 技能表无效
    }

    // 检查技能是否有有效的目标类型
    const bool hasValidTargetType = !tableAbility->target_type().empty();
    const auto targetId = request->target_id();

    if (hasValidTargetType) {
        if (targetId <= 0) {
            return kAbilityInvalidTargetId; // 无效的目标ID
        } else {
            // 创建目标实体并检查其有效性
            entt::entity target{targetId};
            if (tls.registry.valid(target)) {
                return kAbilityInvalidTarget; // 目标无效
            }
        }
    }

    // 获取施法者的施法计时器
    const auto castTimer = tls.registry.try_get<CastingTimer>(caster);
    if (castTimer != nullptr) {
        // 检查技能是否可以打断
        if (tableAbility->immediately() && castTimer->timer.IsActive()) {
            // 立即技能且施法中，广播技能中断消息
            AbilityInterruptedS2C abilityInterruptedS2C;
            ViewUtil::BroadcastMessageToVisiblePlayers(
                caster,
                PlayerAbilityServiceNotifyAbilityInterruptedMsgId,
                abilityInterruptedS2C
            );
        } else if (!tableAbility->immediately() && castTimer->timer.IsActive()) {
            // 非立即技能且施法中，技能不可打断
            return kAbilityUnInterruptible;
        }

        // 移除施法计时器
        tls.registry.remove<CastingTimer>(caster);
    }

    // 广播技能使用消息
    AbilityUsedS2C abilityUsedS2C;
    ViewUtil::BroadcastMessageToVisiblePlayers(
        caster,
        PlayerAbilityServiceNotifyAbilityUsedMsgId,
        abilityUsedS2C
    );

    // 添加新的施法计时器
    tls.registry.emplace<CastingTimer>(caster).timer.RunAfter(
        tableAbility->castpoint(),
        [caster] { return HandleAbilityStart(caster); }
    );

    return kOK; // 成功
}


void AbilityUtil::HandleAbilityInitialize()
{

}

void AbilityUtil::HandleAbilityStart(entt::entity caster)
{

}

void AbilityUtil::HandleAbilitySpell()
{

}

void AbilityUtil::HandleAbilityFinish()
{

}

void AbilityUtil::HandleChannelStart(entt::entity caster)
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
