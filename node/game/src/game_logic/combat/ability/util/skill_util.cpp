#include "skill_util.h"

#include <muduo/base/Logging.h>
#include "ability_config.h"
#include "game_logic/combat/ability/comp/skill_comp.h"
#include "game_logic/combat/ability/constants/ability_constants.h"
#include "game_logic/scene/util/view_util.h"
#include "macros/return_define.h"
#include "pbc/ability_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "service_info/player_ability_service_info.h"
#include "thread_local/storage.h"
#include "time/comp/timer_task_comp.h"
#include "time/util/cooldown_time_util.h"

uint32_t SkillUtil::UseAbility(entt::entity caster, const UseAbilityRequest* request) {
	auto [abilityTable, result] = GetAbilityTable(request->ability_table_id());
	if (result != kOK) {
		return result;
	}

	CHECK_RETURN_IF_NOT_OK(CheckSkillPrerequisites(caster, request));

	BroadcastAbilityUsedMessage(caster, request);
	SetupCastingTimer(caster, abilityTable, request->ability_table_id());

	return kOK;
}

uint32_t SkillUtil::CheckSkillPrerequisites(const entt::entity caster, const ::UseAbilityRequest* request) {
	auto [abilityTable, result] = GetAbilityTable(request->ability_table_id());
	if (result != kOK) {
		return result;
	}

	CHECK_RETURN_IF_NOT_OK(ValidateTarget(request));
	CHECK_RETURN_IF_NOT_OK(CheckCooldown(caster, abilityTable));
	CHECK_RETURN_IF_NOT_OK(CheckCasting(caster, abilityTable));
	CHECK_RETURN_IF_NOT_OK(CheckRecovery(caster, abilityTable));
	CHECK_RETURN_IF_NOT_OK(CheckChannel(caster, abilityTable));

	return kOK;
}

bool SkillUtil::IsAbilityOfType(const uint32_t abilityId, const uint32_t abilityType) {
	auto [abilityTable, result] = GetAbilityTable(abilityId);
	if (abilityTable == nullptr) {
		return false;
	}

	for (auto& tabAbilityType : abilityTable->ability_type()) {
		if ((1 << tabAbilityType) == abilityType) {
			return true;
		}
	}

	return false;
}

void SkillUtil::HandleAbilityInitialize() {
	// Implementation here
}

void SkillUtil::HandleGeneralAbilitySpell(const entt::entity caster, const uint32_t abilityId) {
	HandleAbilitySpell(caster, abilityId);

	LOG_INFO << "Handling general ability spell. Caster: " << entt::to_integral(caster)
		<< ", Ability ID: " << abilityId;

	// Trigger ability effects
	TriggerSkillEffect(caster, abilityId);

	// Manage recovery
	HandleAbilityRecovery(caster, abilityId);
}

void SkillUtil::HandleAbilityRecovery(const entt::entity caster, uint32_t abilityId) {
	auto [abilityTable, result] = GetAbilityTable(abilityId);
	if (abilityTable == nullptr) {
		return;
	}

	auto& recoveryTimer = tls.registry.emplace_or_replace<RecoveryTimerComp>(caster).timer;
	recoveryTimer.RunAfter(abilityTable->recoverytime(), [caster, abilityId] {
		return HandleAbilityFinish(caster, abilityId);
		});
}

void SkillUtil::HandleAbilityFinish(const entt::entity caster, uint32_t abilityId) {
	// Implementation here
}

void SkillUtil::HandleChannelAbilitySpell(entt::entity caster, uint32_t abilityId) {
	auto [abilityTable, result] = GetAbilityTable(abilityId);
	if (abilityTable == nullptr) {
		return;
	}

	LOG_INFO << "Handling channel ability spell. Caster: " << entt::to_integral(caster)
		<< ", Ability ID: " << abilityId;

	HandleAbilitySpell(caster, abilityId);

	auto& channelFinishTimer = tls.registry.emplace_or_replace<ChannelFinishTimerComp>(caster).timer;
	channelFinishTimer.RunAfter(abilityTable->channelfinish(), [caster, abilityId] {
		return HandleChannelFinish(caster, abilityId);
		});

	auto& channelIntervalTimer = tls.registry.emplace_or_replace<ChannelIntervalTimerComp>(caster).timer;
	channelIntervalTimer.RunEvery(abilityTable->channelthink(), [caster, abilityId] {
		return HandleChannelThink(caster, abilityId);
		});
}

void SkillUtil::HandleChannelThink(entt::entity caster, uint32_t abilityId) {
	// Implementation here
}

void SkillUtil::HandleChannelFinish(const entt::entity caster, const uint32_t abilityId) {
	tls.registry.remove<ChannelIntervalTimerComp>(caster);
	HandleAbilityRecovery(caster, abilityId);
}

void SkillUtil::HandleAbilityToggleOn(const entt::entity caster, const uint32_t abilityId) {
	TriggerSkillEffect(caster, abilityId);
}

void SkillUtil::HandleAbilityToggleOff(const entt::entity caster, const uint32_t abilityId) {
	RemoveEffect(caster, abilityId);
}

void SkillUtil::HandleAbilityActivate(const entt::entity caster, const uint32_t abilityId) {
	TriggerSkillEffect(caster, abilityId);
}

void SkillUtil::HandleAbilityDeactivate(const entt::entity caster, const uint32_t abilityId) {
	RemoveEffect(caster, abilityId);
}

uint32_t SkillUtil::ValidateTarget(const ::UseAbilityRequest* request) {
	auto [abilityTable, result] = GetAbilityTable(request->ability_table_id());
	if (!abilityTable->target_type().empty() && request->target_id() <= 0) {
		LOG_ERROR << "Invalid target ID: " << request->target_id()
			<< " for ability ID: " << request->ability_table_id();
		return kAbilityInvalidTargetId;
	}

	if (!abilityTable->target_type().empty()) {
		entt::entity target{ request->target_id() };
		if (!tls.registry.valid(target)) {
			LOG_ERROR << "Invalid target entity: " << request->target_id()
				<< " for ability ID: " << request->ability_table_id();
			return kAbilityInvalidTargetId;
		}
	}

	return kOK;
}

uint32_t SkillUtil::CheckCooldown(const entt::entity caster, const AbilityTable* abilityTable) {
	if (const auto* coolDownTimeListComp = tls.registry.try_get<CooldownTimeListComp>(caster)) {
		if (const auto it = coolDownTimeListComp->cooldown_list().find(abilityTable->cooldown_id());
			it != coolDownTimeListComp->cooldown_list().end() &&
			CoolDownTimeMillisecondUtil::IsInCooldown(it->second)) {
			LOG_ERROR << "Ability ID: " << abilityTable->id()
				<< " is in cooldown for player: " << entt::to_integral(caster)
				<< ". Cooldown ID: " << abilityTable->cooldown_id()
				<< ". Time remaining: " << CoolDownTimeMillisecondUtil::Remaining(it->second) << "ms";
			return kAbilityCooldownNotReady;
		}
	}

	return kOK;
}

uint32_t SkillUtil::CheckCasting(const entt::entity caster, const AbilityTable* abilityTable) {
	if (const auto* castTimerComp = tls.registry.try_get<CastingTimerComp>(caster)) {
		if (abilityTable->immediately() && castTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate ability: " << abilityTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendAbilityInterruptedMessage(caster, abilityTable->id());
			tls.registry.remove<CastingTimerComp>(caster);
			return kOK;
		}
		if (!abilityTable->immediately() && castTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate ability: " << abilityTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kAbilityUnInterruptible;
		}
		tls.registry.remove<CastingTimerComp>(caster);
	}

	return kOK;
}

uint32_t SkillUtil::CheckRecovery(const entt::entity caster, const AbilityTable* abilityTable) {
	if (const auto* recoveryTimeTimerComp = tls.registry.try_get<RecoveryTimerComp>(caster)) {
		if (abilityTable->immediately() && recoveryTimeTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate ability: " << abilityTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendAbilityInterruptedMessage(caster, abilityTable->id());
			tls.registry.remove<RecoveryTimerComp>(caster);
			return kOK;
		}
		if (!abilityTable->immediately() && recoveryTimeTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate ability: " << abilityTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kAbilityUnInterruptible;
		}
		tls.registry.remove<RecoveryTimerComp>(caster);
	}

	return kOK;
}
uint32_t SkillUtil::CheckChannel(const entt::entity caster, const AbilityTable* abilityTable) {
	if (const auto* channelFinishTimerComp = tls.registry.try_get<ChannelFinishTimerComp>(caster)) {
		if (abilityTable->immediately() && channelFinishTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate ability: " << abilityTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendAbilityInterruptedMessage(caster, abilityTable->id());
			// TODO: Implement logic for handling the skill interruption
			tls.registry.remove<ChannelFinishTimerComp>(caster);
			return kOK;
		}

		if (!abilityTable->immediately() && channelFinishTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate ability: " << abilityTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kAbilityUnInterruptible;
		}

		// TODO: Implement logic for handling the skill interruption
		tls.registry.remove<ChannelFinishTimerComp>(caster);
	}

	return kOK;
}

void SkillUtil::BroadcastAbilityUsedMessage(const entt::entity caster, const ::UseAbilityRequest* request) {
	AbilityUsedS2C abilityUsedS2C;
	abilityUsedS2C.set_entity(entt::to_integral(caster));
	abilityUsedS2C.add_target_entity(request->target_id());
	abilityUsedS2C.set_ability_table_id(request->ability_table_id());
	abilityUsedS2C.mutable_position()->CopyFrom(request->position());

	ViewUtil::BroadcastMessageToVisiblePlayers(
		caster,
		PlayerAbilityServiceNotifyAbilityUsedMessageId,
		abilityUsedS2C
	);
}

void SkillUtil::SetupCastingTimer(entt::entity caster, const AbilityTable* abilityTable, uint32_t abilityId) {
	auto& castingTimer = tls.registry.emplace_or_replace<CastingTimerComp>(caster).timer;
	if (IsAbilityOfType(abilityId, kGeneralAbility)) {
		castingTimer.RunAfter(abilityTable->castpoint(), [caster, abilityId] {
			return HandleGeneralAbilitySpell(caster, abilityId);
			});
	}
	else if (IsAbilityOfType(abilityId, kChannelAbility)) {
		castingTimer.RunAfter(abilityTable->castpoint(), [caster, abilityId] {
			return HandleChannelAbilitySpell(caster, abilityId);
			});
	}
}

void SkillUtil::SendAbilityInterruptedMessage(const entt::entity caster, uint32_t abilityId) {
	AbilityInterruptedS2C abilityInterruptedS2C;
	abilityInterruptedS2C.set_entity(entt::to_integral(caster));
	abilityInterruptedS2C.set_ability_table_id(abilityId);

	ViewUtil::BroadcastMessageToVisiblePlayers(
		caster,
		PlayerAbilityServiceNotifyAbilityInterruptedMessageId,
		abilityInterruptedS2C
	);
}

void SkillUtil::TriggerSkillEffect(entt::entity caster, const uint32_t abilityId) {
	auto [abilityTable, result] = GetAbilityTable(abilityId);
	if (abilityTable == nullptr) {
		LOG_ERROR << "Failed to get ability table for Ability ID: " << abilityId;
		return;
	}

	LOG_INFO << "Triggering skill effect. Caster: " << entt::to_integral(caster) << ", Ability ID: " << abilityId;

	for (const auto& effect : abilityTable->effect()) {
		// TODO: Implement effect application logic here
	}
}

void SkillUtil::RemoveEffect(entt::entity caster, const uint32_t abilityId) {
	auto [abilityTable, result] = GetAbilityTable(abilityId);
	if (abilityTable == nullptr) {
		LOG_ERROR << "Failed to get ability table for Ability ID: " << abilityId;
		return;
	}

	for (const auto& effect : abilityTable->effect()) {
		// TODO: Implement effect removal logic here
	}
}

void SkillUtil::HandleAbilitySpell(const entt::entity caster, uint32_t abilityId) {
	// TODO: Implement ability spell handling logic here
}