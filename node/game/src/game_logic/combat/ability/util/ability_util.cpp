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
#include "time/util/cooldown_time_util.h"

uint32_t AbilityUtil::CheckSkillActivationPrerequisites(entt::entity caster, const ::UseAbilityRequest* request) {
    auto [tableAbility, result] = ValidateAbilityTable(request->ability_id());
    if (result != kOK) {
        return result;
    }

    result = ValidateTarget(request);
    if (result != kOK) {
        return result;
    }

    result = CheckCooldown(caster, tableAbility);
    if (result != kOK) {
        return result;
    }

    result = HandleCastingTimer(caster, tableAbility);
    if (result != kOK) {
        return result;
    }

    BroadcastAbilityUsedMessage(caster, request);

    SetupCastingTimer(caster, tableAbility, request->ability_id());

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

// 验证技能表
std::pair<const ability_row*, uint32_t> AbilityUtil::ValidateAbilityTable(uint32_t abilityId) {
    const auto* tableAbility = GetAbilityTable(abilityId);
    if (tableAbility == nullptr) {
        return {nullptr, kInvalidTableId};
    }
    return {tableAbility, kOK};
}

uint32_t AbilityUtil::ValidateTarget(const ::UseAbilityRequest* request) {
    const auto* tableAbility = GetAbilityTable(request->ability_id());
    if (!tableAbility->target_type().empty() && request->target_id() <= 0) {
        return kAbilityInvalidTargetId;
    }

    if (!tableAbility->target_type().empty()) {
        entt::entity target{request->target_id()};
        if (!tls.registry.valid(target)) {
            return kAbilityInvalidTargetId;
        }
    }
    return kOK;
}

uint32_t AbilityUtil::CheckCooldown(entt::entity caster, const ability_row* tableAbility) {
    const auto* coolDownTimeListComp = tls.registry.try_get<CooldownTimeListComp>(caster);
    if (coolDownTimeListComp) {
        auto it = coolDownTimeListComp->cooldown_list().find(tableAbility->cooldown_id());
        if (it != coolDownTimeListComp->cooldown_list().end() &&
            CoolDownTimeMillisecondUtil::IsInCooldown(it->second)) {
            return kAbilityCooldownNotReady;
            }
    }
    return kOK;
}

uint32_t AbilityUtil::HandleCastingTimer(entt::entity caster, const ability_row* tableAbility) {
    const auto* castTimerComp = tls.registry.try_get<CastingTimer>(caster);
    if (castTimerComp) {
        if (tableAbility->immediately() && castTimerComp->timer.IsActive()) {
            SendAbilityInterruptedMessage(caster);
            tls.registry.remove<CastingTimer>(caster);
            return kOK;
        }
        if (!tableAbility->immediately() && castTimerComp->timer.IsActive()) {
            return kAbilityUnInterruptible;
        }
        tls.registry.remove<CastingTimer>(caster);
    }
    return kOK;
}

void AbilityUtil::BroadcastAbilityUsedMessage(entt::entity caster, const ::UseAbilityRequest* request) {
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
}

void AbilityUtil::SetupCastingTimer(entt::entity caster, const ability_row* tableAbility, uint32_t abilityId) {
    auto& castingTimer = tls.registry.emplace<CastingTimer>(caster).timer;
    if (IsAbilityOfType(abilityId, kGeneralAbility)) {
        castingTimer.RunAfter(tableAbility->castpoint(), [caster] { return HandleAbilityStart(caster); });
    } else if (IsAbilityOfType(abilityId, kChannelAbility)) {
        castingTimer.RunAfter(tableAbility->castpoint(), [caster] { return HandleChannelStart(caster); });
    }
}

// 发送技能中断消息
void AbilityUtil::SendAbilityInterruptedMessage(entt::entity caster) {
    AbilityInterruptedS2C abilityInterruptedS2C;
    abilityInterruptedS2C.set_entity(entt::to_integral(caster));
    abilityInterruptedS2C.set_ability_id(0); // todo
    ViewUtil::BroadcastMessageToVisiblePlayers(
        caster,
        PlayerAbilityServiceNotifyAbilityInterruptedMessageId,
        abilityInterruptedS2C
    );
}