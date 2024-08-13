#include "ability_util.h"

#include "ability_config.h"
#include "game_logic/combat/ability/comp/ability_comp.h"
#include "game_logic/combat/ability/constants/ability_constants.h"
#include "game_logic/scene/util/view_util.h"
#include "pbc/ability_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "service_info/player_ability_service_info.h"
#include "thread_local/storage.h"
#include "time/comp/timer_task_comp.h"

uint32_t AbilityUtil::CheckSkillActivationPrerequisites(entt::entity caster, const ::UseAbilityRequest* request) {
    // 获取技能表信息
    const auto* tableAbility = GetAbilityTable(request->ability_id());
    if (tableAbility == nullptr) {
        return kInvalidTableId; // 技能表无效
    }

    // 检查技能是否有目标，并验证目标ID
    if (!tableAbility->target_type().empty()) {
        if (request->target_id() <= 0) {
            return kAbilityInvalidTargetId; // 无效的目标ID
        }

        entt::entity target{request->target_id()};
        if (!tls.registry.valid(target)) {
            return kAbilityInvalidTarget; // 目标无效
        }
    }

    // 检查施法者是否有正在进行的施法计时器
    const auto* castTimer = tls.registry.try_get<CastingTimer>(caster);
    if (castTimer != nullptr) {
        if (tableAbility->immediately() && castTimer->timer.IsActive()) {
            // 处理立即技能的中断情况
            AbilityInterruptedS2C abilityInterruptedS2C;
            abilityInterruptedS2C.set_entity(to_integral(caster));
            abilityInterruptedS2C.set_ability_id(0);//todo
            
            ViewUtil::BroadcastMessageToVisiblePlayers(
                caster,
                PlayerAbilityServiceNotifyAbilityInterruptedMessageId,
                abilityInterruptedS2C
            );
        } else if (!tableAbility->immediately() && castTimer->timer.IsActive()) {
            // 处理非立即技能的不可中断情况
            return kAbilityUnInterruptible;
        }

        tls.registry.remove<CastingTimer>(caster);
    }

    // 广播技能使用消息
    AbilityUsedS2C abilityUsedS2C;
    abilityUsedS2C.set_entity(entt::to_integral(caster));
    abilityUsedS2C.add_target_entity(request->target_id());
    abilityUsedS2C.set_ability_id(request->ability_id());
    abilityUsedS2C.mutable_position()->CopyFrom(request->position());

    ViewUtil::BroadcastMessageToVisiblePlayers(
        caster,
        PlayerAbilityServiceNotifyAbilityUsedMessageId,
        abilityUsedS2C
    );

    // 根据技能类型设置新的施法计时器
    auto& castingTimer = tls.registry.emplace<CastingTimer>(caster).timer;
    if (IsAbilityOfType(request->ability_id(), kGeneralAbility)) {
        castingTimer.RunAfter(tableAbility->castpoint(), [caster] { return HandleAbilityStart(caster); });
    } else if (IsAbilityOfType(request->ability_id(), kChannelAbility)) {
        castingTimer.RunAfter(tableAbility->castpoint(), [caster] { return HandleChannelStart(caster); });
    }

    return kOK;
}

bool AbilityUtil::IsAbilityOfType(uint32_t abilityId, uint32_t abilityType) {
    const auto* tableAbility = GetAbilityTable(abilityId);
    if (tableAbility == nullptr) {
        return false;
    }

    return std::find(tableAbility->ability_type().begin(), tableAbility->ability_type().end(), abilityType) != tableAbility->ability_type().end();
}


void AbilityUtil::HandleAbilityInitialize()
{

}

void AbilityUtil::HandleAbilityStart(entt::entity caster)
{
    auto& castingTimer = tls.registry.emplace<CastingTimer>(caster).timer;
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
