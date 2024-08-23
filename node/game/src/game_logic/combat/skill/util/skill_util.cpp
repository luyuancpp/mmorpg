#include "skill_util.h"

#include <muduo/base/Logging.h>
#include "skill_config.h"
#include "game_logic/combat/skill/comp/skill_comp.h"
#include "game_logic/combat/skill/constants/skill_constants.h"
#include "game_logic/scene/util/view_util.h"
#include "macros/return_define.h"
#include "pbc/skill_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "service_info/player_skill_service_info.h"
#include "thread_local/storage.h"
#include "time/comp/timer_task_comp.h"
#include "time/util/cooldown_time_util.h"
#include "time/util/time_util.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "thread_local/storage_game.h"

// Generate a unique skill ID that is not in the provided context maps
uint64_t GenerateUniqueSkillId(const SkillContextMap& casterBuffList, const SkillContextMap& targetBuffList) {
	uint64_t newSkillId;
	do {
		newSkillId = tlsGame.skillIdGenerator.Generate();
	} while (casterBuffList.contains(newSkillId) || targetBuffList.contains(newSkillId));
	return newSkillId;
}

// Initialize an entity with a SkillContextMap
void SkillUtil::InitEntity(entt::entity entity) {
	tls.registry.emplace<SkillContextMap>(entity);
}

// Handle the use of a skill by the caster
uint32_t SkillUtil::UseSkill(entt::entity caster, const UseSkillRequest* request) {
	auto [skillTable, result] = GetSkillTable(request->skill_table_id());
	if (result != kOK) {
		return result;
	}

	CHECK_RETURN_IF_NOT_OK(CheckSkillPrerequisites(caster, request));

	BroadcastSkillUsedMessage(caster, request);

	auto context = std::make_shared<SkillContextPtrComp::element_type>();
	context->caster = entt::to_integral(caster);
	context->skillTableId = request->skill_table_id();
	context->target = request->target_id();
	context->castTime = TimeUtil::NowMilliseconds();

	auto& casterSkillContextMap = tls.registry.get<SkillContextMap>(caster);
	entt::entity target = entt::to_entity(request->target_id());
	SkillContextMap emptySkillContextMap;
	auto& targetSkillContextMap = tls.registry.valid(target) ? tls.registry.get<SkillContextMap>(target) : emptySkillContextMap;

	context->SkillId = GenerateUniqueSkillId(casterSkillContextMap, targetSkillContextMap);
	casterSkillContextMap.emplace(context->SkillId, context);
	if (tls.registry.valid(target)) targetSkillContextMap.emplace(context->SkillId, context);

	SetupCastingTimer(caster, skillTable, context->SkillId);

	return kOK;
}

uint32_t SkillUtil::CheckSkillPrerequisites(const entt::entity caster, const ::UseSkillRequest* request) {
	auto [skillTable, result] = GetSkillTable(request->skill_table_id());
	if (result != kOK) {
		return result;
	}

	CHECK_RETURN_IF_NOT_OK(ValidateTarget(request));
	CHECK_RETURN_IF_NOT_OK(CheckCooldown(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckCasting(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckRecovery(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckChannel(caster, skillTable));

	return kOK;
}

bool SkillUtil::IsSkillOfType(const uint64_t skillId, const uint32_t skillType) {
	auto [skillTable, result] = GetSkillTable(skillId);
	if (skillTable == nullptr) {
		return false;
	}

	for (auto& tabSkillType : skillTable->skill_type()) {
		if ((1 << tabSkillType) == skillType) {
			return true;
		}
	}

	return false;
}

void SkillUtil::HandleSkillInitialize() {
	// Implementation here
}

void SkillUtil::HandleGeneralSkillSpell(const entt::entity caster, const uint64_t skillId) {
	HandleSkillSpell(caster, skillId);

	LOG_INFO << "Handling general skill spell. Caster: " << entt::to_integral(caster)
		<< ", Skill ID: " << skillId;

	TriggerSkillEffect(caster, skillId);
	HandleSkillRecovery(caster, skillId);
}

// Set up a timer for skill recovery after casting
void SkillUtil::HandleSkillRecovery(const entt::entity caster, uint64_t skillId) {
	auto [skillTable, result] = GetSkillTable(skillId);
	if (skillTable == nullptr) {
		return;
	}

	auto& recoveryTimer = tls.registry.emplace_or_replace<RecoveryTimerComp>(caster).timer;
	recoveryTimer.RunAfter(skillTable->recoverytime(), [caster, skillId] {
		return HandleSkillFinish(caster, skillId);
		});
}

void SkillUtil::HandleSkillFinish(const entt::entity caster, uint64_t skillId) {
	// Implementation here

	//todo player off line 
	auto& casterSkillContextMap = tls.registry.get<SkillContextMap>(caster);
	auto skillContentIt = casterSkillContextMap.find(skillId);
	if (skillContentIt != casterSkillContextMap.end())
	{
		entt::entity target = entt::to_entity(skillContentIt->second->target);
		if (tls.registry.valid(target)) {
			auto& targetSkillContextMap = tls.registry.get<SkillContextMap>(target);
			targetSkillContextMap.erase(skillId);
		}
		casterSkillContextMap.erase(skillContentIt);
	}
	
}

void SkillUtil::HandleChannelSkillSpell(entt::entity caster, uint64_t skillId) {
	auto [skillTable, result] = GetSkillTable(skillId);
	if (skillTable == nullptr) {
		return;
	}

	LOG_INFO << "Handling channel skill spell. Caster: " << entt::to_integral(caster)
		<< ", Skill ID: " << skillId;

	HandleSkillSpell(caster, skillId);

	auto& channelFinishTimer = tls.registry.emplace_or_replace<ChannelFinishTimerComp>(caster).timer;
	channelFinishTimer.RunAfter(skillTable->channelfinish(), [caster, skillId] {
		return HandleChannelFinish(caster, skillId);
		});

	auto& channelIntervalTimer = tls.registry.emplace_or_replace<ChannelIntervalTimerComp>(caster).timer;
	channelIntervalTimer.RunEvery(skillTable->channelthink(), [caster, skillId] {
		return HandleChannelThink(caster, skillId);
		});
}

// Placeholder for channeling think logic
void SkillUtil::HandleChannelThink(entt::entity caster, uint64_t skillId) {
	// TODO: Implement channel think logic here
}

void SkillUtil::HandleChannelFinish(const entt::entity caster, const uint64_t skillId) {
	tls.registry.remove<ChannelIntervalTimerComp>(caster);
	HandleSkillRecovery(caster, skillId);
}

void SkillUtil::HandleSkillToggleOn(const entt::entity caster, const uint64_t skillId) {
	TriggerSkillEffect(caster, skillId);
}

void SkillUtil::HandleSkillToggleOff(const entt::entity caster, const uint64_t skillId) {
	RemoveEffect(caster, skillId);
}

void SkillUtil::HandleSkillActivate(const entt::entity caster, const uint64_t skillId) {
	TriggerSkillEffect(caster, skillId);
}

void SkillUtil::HandleSkillDeactivate(const entt::entity caster, const uint64_t skillId) {
	RemoveEffect(caster, skillId);
}

uint32_t SkillUtil::ValidateTarget(const ::UseSkillRequest* request) {
	auto [skillTable, result] = GetSkillTable(request->skill_table_id());
	if (result != kOK || skillTable == nullptr) {
		return result;
	}

	// Check if target ID is valid
	if (!skillTable->target_type().empty() && request->target_id() <= 0) {
		LOG_ERROR << "Invalid target ID: " << request->target_id()
			<< " provided for skill ID: " << request->skill_table_id()
			<< ". Target ID must be positive if target type is specified.";
		return kSkillInvalidTargetId;
	}

	// Validate target entity
	if (!skillTable->target_type().empty()) {
		entt::entity target{ request->target_id() };

		if (!tls.registry.valid(target)) {
			LOG_ERROR << "Target entity with ID: " << request->target_id()
				<< " is invalid or does not exist for skill ID: " << request->skill_table_id();
			return kSkillInvalidTargetId;
		}

		// Check target entity type
		bool isValidTargetType = tls.registry.all_of<Player>(target) || tls.registry.all_of<Npc>(target);
		if (!isValidTargetType) {
			LOG_ERROR << "Target entity with ID: " << request->target_id()
				<< " is of an invalid type for skill ID: " << request->skill_table_id()
				<< ". Expected Player or Npc.";
			return kSkillInvalidTargetId;
		}
	}

	return kOK;
}


uint32_t SkillUtil::CheckCooldown(const entt::entity caster, const SkillTable* skillTable) {
	if (const auto* coolDownTimeListComp = tls.registry.try_get<CooldownTimeListComp>(caster)) {
		if (const auto it = coolDownTimeListComp->cooldown_list().find(skillTable->cooldown_id());
			it != coolDownTimeListComp->cooldown_list().end() &&
			CoolDownTimeMillisecondUtil::IsInCooldown(it->second)) {
			LOG_ERROR << "Skill ID: " << skillTable->id()
				<< " is in cooldown for player: " << entt::to_integral(caster)
				<< ". Cooldown ID: " << skillTable->cooldown_id()
				<< ". Time remaining: " << CoolDownTimeMillisecondUtil::Remaining(it->second) << "ms";
			return kSkillCooldownNotReady;
		}
	}

	return kOK;
}

uint32_t SkillUtil::CheckCasting(const entt::entity caster, const SkillTable* skillTable) {
	if (const auto* castTimerComp = tls.registry.try_get<CastingTimerComp>(caster)) {
		if (skillTable->immediately() && castTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate skill: " << skillTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendSkillInterruptedMessage(caster, skillTable->id());
			tls.registry.remove<CastingTimerComp>(caster);
			return kOK;
		}
		if (!skillTable->immediately() && castTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate skill: " << skillTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kSkillUnInterruptible;
		}
		tls.registry.remove<CastingTimerComp>(caster);
	}

	return kOK;
}

uint32_t SkillUtil::CheckRecovery(const entt::entity caster, const SkillTable* skillTable) {
	if (const auto* recoveryTimeTimerComp = tls.registry.try_get<RecoveryTimerComp>(caster)) {
		if (skillTable->immediately() && recoveryTimeTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate skill: " << skillTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendSkillInterruptedMessage(caster, skillTable->id());
			tls.registry.remove<RecoveryTimerComp>(caster);
			return kOK;
		}
		if (!skillTable->immediately() && recoveryTimeTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate skill: " << skillTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kSkillUnInterruptible;
		}
		tls.registry.remove<RecoveryTimerComp>(caster);
	}

	return kOK;
}
uint32_t SkillUtil::CheckChannel(const entt::entity caster, const SkillTable* skillTable) {
	if (const auto* channelFinishTimerComp = tls.registry.try_get<ChannelFinishTimerComp>(caster)) {
		if (skillTable->immediately() && channelFinishTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate skill: " << skillTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendSkillInterruptedMessage(caster, skillTable->id());
			// TODO: Implement logic for handling the skill interruption
			tls.registry.remove<ChannelFinishTimerComp>(caster);
			return kOK;
		}

		if (!skillTable->immediately() && channelFinishTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate skill: " << skillTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kSkillUnInterruptible;
		}

		// TODO: Implement logic for handling the skill interruption
		tls.registry.remove<ChannelFinishTimerComp>(caster);
	}

	return kOK;
}

void SkillUtil::BroadcastSkillUsedMessage(const entt::entity caster, const ::UseSkillRequest* request) {
	SkillUsedS2C skillUsedS2C;
	skillUsedS2C.set_entity(entt::to_integral(caster));
	skillUsedS2C.add_target_entity(request->target_id());
	skillUsedS2C.set_skill_table_id(request->skill_table_id());
	skillUsedS2C.mutable_position()->CopyFrom(request->position());

	ViewUtil::BroadcastMessageToVisiblePlayers(
		caster,
		PlayerSkillServiceNotifySkillUsedMessageId,
		skillUsedS2C
	);
}

void SkillUtil::SetupCastingTimer(entt::entity caster, const SkillTable* skillTable, uint64_t skillId) {
	auto& castingTimer = tls.registry.emplace_or_replace<CastingTimerComp>(caster).timer;
	if (IsSkillOfType(skillId, kGeneralSkill)) {
		castingTimer.RunAfter(skillTable->castpoint(), [caster, skillId] {
			return HandleGeneralSkillSpell(caster, skillId);
			});
	}
	else if (IsSkillOfType(skillId, kChannelSkill)) {
		castingTimer.RunAfter(skillTable->castpoint(), [caster, skillId] {
			return HandleChannelSkillSpell(caster, skillId);
			});
	}
}

void SkillUtil::SendSkillInterruptedMessage(const entt::entity caster, uint64_t skillId) {
	SkillInterruptedS2C skillInterruptedS2C;
	skillInterruptedS2C.set_entity(entt::to_integral(caster));
	skillInterruptedS2C.set_skill_table_id(skillId);

	ViewUtil::BroadcastMessageToVisiblePlayers(
		caster,
		PlayerSkillServiceNotifySkillInterruptedMessageId,
		skillInterruptedS2C
	);
}

void SkillUtil::TriggerSkillEffect(entt::entity caster, const uint64_t skillId) {
	auto [skillTable, result] = GetSkillTable(skillId);
	if (skillTable == nullptr) {
		LOG_ERROR << "Failed to get skill table for Skill ID: " << skillId;
		return;
	}

	LOG_INFO << "Triggering skill effect. Caster: " << entt::to_integral(caster) << ", Skill ID: " << skillId;

	for (const auto& effect : skillTable->effect()) {
		// TODO: Implement effect application logic here
	}
}

void SkillUtil::RemoveEffect(entt::entity caster, const uint64_t skillId) {
	auto [skillTable, result] = GetSkillTable(skillId);
	if (skillTable == nullptr) {
		LOG_ERROR << "Failed to get skill table for Skill ID: " << skillId;
		return;
	}

	for (const auto& effect : skillTable->effect()) {
		// TODO: Implement effect removal logic here
	}
}

void SkillUtil::HandleSkillSpell(const entt::entity caster, uint64_t skillId) {
	// TODO: Implement skill spell handling logic here
}