﻿#include "skill_util.h"

#include <muduo/base/Logging.h>

#include "entity_error_tip.pb.h"
#include "skill_config.h"
#include "proto/logic/component/buff_comp.pb.h"
#include "game_logic/combat/buff/util/buff_util.h"
#include "game_logic/combat/skill/comp/skill_comp.h"
#include "game_logic/combat/skill/constants/skill_constants.h"
#include "game_logic/scene/util/view_util.h"
#include "logic/event/combat_event.pb.h"
#include "macros/return_define.h"
#include "pbc/common_error_tip.pb.h"
#include "pbc/skill_error_tip.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"
#include "service_info/player_skill_service_info.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "time/comp/timer_task_comp.h"
#include "time/util/cooldown_time_util.h"
#include "time/util/time_util.h"

// Generate a unique skill ID that is not present in the given buff lists
uint64_t GenerateUniqueSkillId(const SkillContextCompMap& casterBuffList, const SkillContextCompMap& targetBuffList) {
    uint64_t newSkillId;
    do {
        newSkillId = tlsGame.skillIdGenerator.Generate();
    } while (casterBuffList.contains(newSkillId) || targetBuffList.contains(newSkillId));
    return newSkillId;
}

// Initialize an entity with necessary skill components
void SkillUtil::InitializePlayerComponents(entt::entity entity) {
    tls.registry.emplace<SkillContextCompMap>(entity);
    tls.registry.emplace<CooldownTimeListComp>(entity);
}

void SkillUtil::StartCooldown(entt::entity caster, const SkillTable* skillTable) {
	if (auto* coolDownComp = tls.registry.try_get<CooldownTimeListComp>(caster)) {
		CooldownTimeComp comp;
		comp.set_start(TimeUtil::NowMilliseconds());
		comp.set_cooldown_table_id(skillTable->cooldown_id());

		auto coolDownList = coolDownComp->mutable_cooldown_list();
		(*coolDownList)[skillTable->cooldown_id()] = comp;
	}
}

// Look at the target position
void LookAtTargetPosition(entt::entity caster, const ReleaseSkillSkillRequest* request) {
	if (request->has_position()) {
		ViewUtil::LookAtPosition(caster, request->position());
	} else if (request->target_id() > 0) {
		entt::entity target{ request->target_id() };
		if (const auto transform = tls.registry.try_get<Transform>(target)) {
			ViewUtil::LookAtPosition(caster, transform->location());
		}
	}
}

// Create skill context for the caster
std::shared_ptr<SkillContextPBComponent> CreateSkillContext(entt::entity caster, const ReleaseSkillSkillRequest* request) {
	auto context = std::make_shared<SkillContextPBComponent>();
	context->set_caster(entt::to_integral(caster));
	context->set_skilltableid(request->skill_table_id());
	context->set_target(request->target_id());
	context->set_casttime(TimeUtil::NowMilliseconds());
	context->set_skillid(GenerateUniqueSkillId(tls.registry.get<SkillContextCompMap>(caster), {}));
	return context;
}

// Add skill context to the caster and target maps
void AddSkillContext(entt::entity caster, const ReleaseSkillSkillRequest* request, std::shared_ptr<SkillContextPBComponent> context) {
	auto& casterSkillContextMap = tls.registry.get<SkillContextCompMap>(caster);
	casterSkillContextMap.emplace(context->skillid(), context);

	entt::entity target{ request->target_id() };
	if (tls.registry.valid(target)) {
		auto& targetSkillContextMap = tls.registry.get<SkillContextCompMap>(target);
		targetSkillContextMap.emplace(context->skillid(), context);
	}
}

void ConsumeItems(entt::entity caster, const SkillTable* skillTable) {
	for (const auto& item : skillTable->requireditem()) {
		// TODO: Implement item consumption logic
	}
}

// Consume required resources
void ConsumeResources(entt::entity caster, const SkillTable* skillTable) {
	for (const auto& resource : skillTable->requestresource()) {
		// TODO: Implement resource consumption logic
	}
}

// Release a skill and perform necessary actions
uint32_t SkillUtil::ReleaseSkill(entt::entity caster, const ReleaseSkillSkillRequest* request) {
	auto [skillTable, result] = GetSkillTable(request->skill_table_id());
	if (result != kOK || !skillTable) return result;

	CHECK_RETURN_IF_NOT_OK(CheckSkillPrerequisites(caster, request));
	LookAtTargetPosition(caster, request);
	BroadcastSkillUsedMessage(caster, request);
    
	const auto context = CreateSkillContext(caster, request);
	AddSkillContext(caster, request, context);
    
	ConsumeItems(caster, skillTable);
	ConsumeResources(caster, skillTable);
	StartCooldown(caster, skillTable);
	SetupCastingTimer(caster, skillTable, context->skillid());

	return kOK;
}

uint32_t CheckPlayerLevel(const entt::entity caster, const SkillTable* skillTable) {
	if (!tls.registry.any_of<Player>(caster))
	{
		return  kOK;
	}
	return kOK;
}

uint32_t CheckBuff(const entt::entity caster, const SkillTable* skillTable) {
	return kOK;
}

uint32_t CheckState(const entt::entity caster, const SkillTable* skillTable) {
	for (auto& resource : skillTable->requestresource()){
		
	}
	
	return kOK;
}

uint32_t CheckItemUse(const entt::entity caster, const SkillTable* skillTable) {
	for (auto& item : skillTable->requireditem()){
		
	}
	return kOK;
}

uint32_t SkillUtil::CheckSkillPrerequisites(const entt::entity caster, const ::ReleaseSkillSkillRequest* request) {
	auto [skillTable, result] = GetSkillTable(request->skill_table_id());
	if (result != kOK) {
		return result;
	}

	CHECK_RETURN_IF_NOT_OK(ValidateTarget(request));
	CHECK_RETURN_IF_NOT_OK(CheckCooldown(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckCasting(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckRecovery(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckChannel(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckPlayerLevel(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckBuff(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckState(caster, skillTable));
	CHECK_RETURN_IF_NOT_OK(CheckItemUse(caster, skillTable));
	return kOK;
}

bool SkillUtil::IsSkillOfType(const uint32_t skillTableID, const uint32_t skillType) {
	auto [skillTable, result] = GetSkillTable(skillTableID);
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
	auto& casterSkillContextMap = tls.registry.get<SkillContextCompMap>(caster);
	auto skillContentIt = casterSkillContextMap.find(skillId);

	if (skillContentIt == casterSkillContextMap.end()) {
		return;
	}

	auto [skillTable, result] = GetSkillTable(skillContentIt->second->skilltableid());
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

	// todo player off line 
	auto& casterSkillContextMap = tls.registry.get<SkillContextCompMap>(caster);
	auto skillContentIt = casterSkillContextMap.find(skillId);
	if (skillContentIt != casterSkillContextMap.end()) {
		entt::entity target = entt::to_entity(skillContentIt->second->target());
		if (tls.registry.valid(target)) {
			auto& targetSkillContextMap = tls.registry.get<SkillContextCompMap>(target);
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

uint32_t SkillUtil::ValidateTarget(const ::ReleaseSkillSkillRequest* request) {
	// 获取技能表
	auto [skillTable, result] = GetSkillTable(request->skill_table_id());
	if (result != kOK || skillTable == nullptr) {
		return result;
	}

	// 检查目标ID的有效性
	if (!skillTable->target_type().empty() && request->target_id() <= 0) {
		LOG_ERROR << "Invalid target ID: " << request->target_id()
			<< " provided for skill ID: " << request->skill_table_id()
			<< ". Target ID must be positive if target type is specified.";
		return kSkillInvalidTargetId;
	}

	// 默认错误状态
	uint32_t err = kOK;

	// 遍历技能目标类型
	for (auto& tabSkillType : skillTable->target_type()) {
		// 检查不需要目标的情况
		if ((1 << tabSkillType) == kNoTargetRequired) {
			return kOK;  // 无需进一步检查
		}

		// 检查是否为目标技能
		if ((1 << tabSkillType) == kTargetedSkill) {
			entt::entity target{ request->target_id() };

			// 验证目标实体
			if (!tls.registry.valid(target)) {
				LOG_ERROR << "Target entity with ID: " << request->target_id()
					<< " is invalid or does not exist for skill ID: " << request->skill_table_id();
				return kSkillInvalidTargetId;
			}

			// 检查目标实体类型
			bool isValidTargetType = tls.registry.any_of<Player>(target) || tls.registry.any_of<Npc>(target);
			if (!isValidTargetType) {
				LOG_ERROR << "Target entity with ID: " << request->target_id()
					<< " is of an invalid type for skill ID: " << request->skill_table_id()
					<< ". Expected Player or Npc.";
				return kSkillInvalidTargetId;
			}

			return kOK;  // 验证通过
		}

		// 检查范围技能
		if ((1 << tabSkillType) == kAreaOfEffect) {
			return kOK;  // 验证通过
		}
	}

	return err;  // 返回错误状态（如果有）
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

void SkillUtil::BroadcastSkillUsedMessage(const entt::entity caster, const ::ReleaseSkillSkillRequest* request) {
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
	if (IsSkillOfType(skillTable->id(), kGeneralSkill)) {
		castingTimer.RunAfter(skillTable->castpoint(), [caster, skillId] {
			return HandleGeneralSkillSpell(caster, skillId);
			});
	}
	else if (IsSkillOfType(skillTable->id(), kChannelSkill)) {
		castingTimer.RunAfter(skillTable->castpoint(), [caster, skillId] {
			return HandleChannelSkillSpell(caster, skillId);
			});
	}
}

void SkillUtil::SendSkillInterruptedMessage(const entt::entity caster, const uint64_t skillId) {
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
	auto& casterSkillContextMap = tls.registry.get<SkillContextCompMap>(caster);
	auto skillContextIt = casterSkillContextMap.find(skillId);

	if (skillContextIt == casterSkillContextMap.end()) {
		return;
	}

	const auto& skillContext = skillContextIt->second;
	
	auto [skillTable, result] = GetSkillTable(skillContext->skilltableid());
	if (skillTable == nullptr) {
		LOG_ERROR << "Failed to get skill table for Skill ID: " << skillId;
		return;
	}

	LOG_INFO << "Triggering skill effect. Caster: " << entt::to_integral(caster) << ", Skill ID: " << skillId;

	for (const auto& effect : skillTable->effect()) {
		BuffUtil::AddOrUpdateBuff(entt::to_entity(skillContext->target()), effect, skillContext);
	}
}

void SkillUtil::RemoveEffect(entt::entity caster, const uint64_t skillId) {
	auto& casterSkillContextMap = tls.registry.get<SkillContextCompMap>(caster);
	auto skillContentIt = casterSkillContextMap.find(skillId);

	if (skillContentIt == casterSkillContextMap.end()) {
		return;
	}
	
	auto [skillTable, result] = GetSkillTable(skillContentIt->second->skilltableid());
	if (skillTable == nullptr) {
		LOG_ERROR << "Failed to get skill table for Skill ID: " << skillId;
		return;
	}

	for (const auto& effect : skillTable->effect()) {
		// TODO: Implement effect removal logic here
	}
}
// 计算技能伤害
void CalculateSkillDamage(const entt::entity caster, DamageEventComponent& damageEvent) {
	damageEvent.set_attacker_id(entt::to_integral(caster));
	damageEvent.set_damage(100); // 设置固定伤害值
}

// 具体的伤害处理
void DealDamage(DamageEventComponent& damageEvent, const entt::entity caster, const entt::entity target) {
	damageEvent.set_target(entt::to_integral(target)); 

	// 触发伤害前事件
	BuffUtil::OnBeforeGiveDamage(caster, damageEvent);
	BuffUtil::OnBeforeTakeDamage(target, damageEvent);
    
	auto& baseAttributesPBComponent = tls.registry.get<BaseAttributesPBComponent>(target);
    
	// 减少目标生命值
	baseAttributesPBComponent.set_health(baseAttributesPBComponent.health() - static_cast<uint64_t>(std::ceil(damageEvent.damage())));

	// 检查目标是否死亡
	if (baseAttributesPBComponent.health() <= 0) {
		BuffUtil::OnBeforeDead(target); // 触发死亡前事件
	}

	// 触发伤害后事件
	BuffUtil::OnAfterGiveDamage(caster, damageEvent);
	BuffUtil::OnAfterTakeDamage(target, damageEvent);

	// 处理死亡后逻辑
	if (baseAttributesPBComponent.health() <= 0) {
		BuffUtil::OnAfterDead(target); // 触发死亡后事件
		if (caster != target) {
			BuffUtil::OnKill(caster); // 触发击杀事件
		}

		BeKillEvent beKillEvent;
		beKillEvent.set_caster(entt::to_integral(caster));
		beKillEvent.set_target(entt::to_integral(target));

		tls.dispatcher.trigger(beKillEvent);
	}
}

void SkillUtil::HandleSkillSpell(const entt::entity caster, uint64_t skillId) {
	auto& casterSkillContextMap = tls.registry.get<SkillContextCompMap>(caster);
	auto skillContextIt = casterSkillContextMap.find(skillId);

	if (skillContextIt == casterSkillContextMap.end()) {
		return;
	}

	const auto& skillContext = skillContextIt->second;
	auto [skillTable, result] = GetSkillTable(skillContext->skilltableid());

	if (skillTable == nullptr) {
		LOG_ERROR << "Failed to get skill table for Skill ID: " << skillId;
		return;
	}
    
	DamageEventComponent damageEvent;
	CalculateSkillDamage(caster, damageEvent); // 计算伤害
	DealDamage(damageEvent, caster, entt::to_entity(skillContext->target())); // 处理伤害
}

