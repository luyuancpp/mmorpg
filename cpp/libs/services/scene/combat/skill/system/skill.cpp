#include "skill.h"

#include <muduo/base/Logging.h>

#include "proto/scene/player_skill.pb.h"
#include "table/proto/tip/entity_error_tip.pb.h"
#include "table/code/skillpermission_table.h"
#include "table/code/skill_table.h"
#include "actor/action_state/constants/actor_state.h"
#include "actor/action_state/system/actor_action_state.h"
#include "combat_state/system/combat_state.h"
#include "combat/buff/system/buff.h"
#include "combat/skill/comp/skill_comp.h"
#include "combat/skill/constants/skill.h"
#include "spatial/system/view.h"
#include "proto/common/event/combat_event.pb.h"
#include "proto/common/event/skill_event.pb.h"
#include "macros/return_define.h"
#include "macros/error_return.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/skill_error_tip.pb.h"
#include "proto/common/component/buff_comp.pb.h"
#include "proto/common/component/npc_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"
#include "rpc/service_metadata/player_skill_service_metadata.h"
#include "proto/common/component/actor_combat_state_comp.pb.h"

#include "time/comp/timer_task_comp.h"
#include "time/system/time_cooldown.h"
#include "time/system/time.h"
#include <core/system/id_generator.h>
#include <thread_context/dispatcher_manager.h>

uint64_t GenerateUniqueSkillId(const SkillContextCompMap& casterSkillContexts, const SkillContextCompMap& targetSkillContexts) {
	uint64_t newSkillId;
	do {
		newSkillId = tlsIdGeneratorManager.skillIdGenerator.Generate();
	} while (casterSkillContexts.contains(newSkillId) || targetSkillContexts.contains(newSkillId));
	return newSkillId;
}

void SkillSystem::StartCooldown(entt::entity caster, const SkillTable* skillTable) {
	auto& coolDownComp = tlsEcs.actorRegistry.get_or_emplace<CooldownTimeListComp>(caster);
	CooldownTimeComp comp;
	comp.set_start(TimeSystem::NowMilliseconds());
	comp.set_cooldown_table_id(skillTable->cooldown_id());

	const auto coolDownList = coolDownComp.mutable_cooldown_list();
	(*coolDownList)[skillTable->cooldown_id()] = comp;
}

void LookAtTargetPosition(entt::entity caster, const ReleaseSkillRequest* request) {
	if (request->has_position()) {
		ViewSystem::LookAtPosition(caster, request->position());
	} else if (request->target_id() > 0) {
		const entt::entity target{ request->target_id() };
		auto& transform = tlsEcs.actorRegistry.get_or_emplace<Transform>(target);
		ViewSystem::LookAtPosition(caster, transform.location());
	}
}

std::shared_ptr<SkillContextComp> CreateSkillContext(entt::entity caster, const ReleaseSkillRequest* request) {
	auto context = std::make_shared<SkillContextComp>();
	context->set_caster(entt::to_integral(caster));
	context->set_skilltableid(request->skill_table_id());
	context->set_target(request->target_id());
	context->set_casttime(TimeSystem::NowMilliseconds());
	context->set_skillid(GenerateUniqueSkillId(tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(caster), {}));
	return context;
}

void AddSkillContext(entt::entity caster, const ReleaseSkillRequest* request, std::shared_ptr<SkillContextComp> context) {
	auto& casterSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(caster);
	casterSkillContextMap.emplace(context->skillid(), context);

	entt::entity target{ request->target_id() };
	if (tlsEcs.actorRegistry.valid(target)) {
		auto& targetSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(target);
		targetSkillContextMap.emplace(context->skillid(), context);
	}
}

void ConsumeItems(entt::entity caster, const SkillTable* skillTable) {
	// TODO: Implement item consumption logic
}

void ConsumeResources(entt::entity caster, const SkillTable* skillTable) {
	// TODO: Implement resource consumption logic
}

void ApplySkillHitEffectIfValid(const entt::entity casterEntity, const uint64_t targetId) {
	const entt::entity targetEntity{targetId};
	if (!tlsEcs.actorRegistry.valid(targetEntity)) {
		return;
	}
	BuffSystem::OnSkillHit(casterEntity, targetEntity);
}

uint32_t SkillSystem::ReleaseSkill(const entt::entity casterEntity, const ReleaseSkillRequest* request) {
	LookupSkill(request->skill_table_id());

	RETURN_ON_ERROR(CheckSkillPrerequisites(casterEntity, request));
	LookAtTargetPosition(casterEntity, request);
	BroadcastSkillUsedMessage(casterEntity, request);
    
	const auto context = CreateSkillContext(casterEntity, request);
	AddSkillContext(casterEntity, request, context);
    
	ConsumeItems(casterEntity, skillRow);
	ConsumeResources(casterEntity, skillRow);
	StartCooldown(casterEntity, skillRow);
	SetupCastingTimer(casterEntity, skillRow, context->skillid());

	ApplySkillHitEffectIfValid(casterEntity, request->target_id());

	return kSuccess;
}

uint32_t CheckPlayerLevel(const entt::entity casterEntity, const SkillTable* skillTable) {
	// TODO: Implement level requirement validation for non-NPC casters
	return kSuccess;
}

uint32_t canUseSkillInCurrentState(const uint32_t state, const uint32_t skill) {
	LookupSkillPermission(state);

	const auto skillTypeIndex = (1 << skill);
	if (skillTypeIndex >= skillPermissionRow->skill_type_size())
	{
		return MAKE_ERROR_MSG(kInvalidTableData,
			"state=" << state << " skill=" << skill
			<< " skillTypeIndex=" << skillTypeIndex
			<< " size=" << skillPermissionRow->skill_type_size());
	}
	
	return skillPermissionRow->skill_type(skillTypeIndex);
}


uint32_t CheckBuff(const entt::entity casterEntity, const SkillTable* skillTable) {

	auto& combatStateCollection = tlsEcs.actorRegistry.get_or_emplace<CombatStateCollectionComp>(casterEntity);

	for (auto& [currentState, buffList] : combatStateCollection.states())
	{
        for (const auto& skillType : skillTable->skill_type()) {
            const auto skill = static_cast<eSkillType>(skillType);
            const auto result = canUseSkillInCurrentState(currentState, skill);
            if (result != kSuccess) {
                return result;  // Return error code if any skill can't be used
            }
        }
	}

	return kSuccess;  // All skills can be used in the current state
}


uint32_t CheckState(const entt::entity casterEntity, const SkillTable* skillTable) {
	RETURN_ON_ERROR(ActorActionStateSystem::TryPerformAction(casterEntity, kActorActionUseSkill, kActorStateCombat));
	RETURN_ON_ERROR(CombatStateSystem::ValidateSkillUsage(casterEntity, kActorActionUseSkill));
	return kSuccess;
}

uint32_t CheckItemUse(const entt::entity casterEntity, const SkillTable* skillTable) {
	// TODO: Validate item requirements before skill use
	return kSuccess;
}

uint32_t SkillSystem::CheckSkillPrerequisites(const entt::entity casterEntity, const ::ReleaseSkillRequest* request) {
	LookupSkill(request->skill_table_id());

	RETURN_ON_ERROR(ValidateTarget(request));
	RETURN_ON_ERROR(CheckCooldown(casterEntity, skillRow));
	RETURN_ON_ERROR(CheckCasting(casterEntity, skillRow));
	RETURN_ON_ERROR(CheckRecovery(casterEntity, skillRow));
	RETURN_ON_ERROR(CheckChannel(casterEntity, skillRow));
	RETURN_ON_ERROR(CheckPlayerLevel(casterEntity, skillRow));
	RETURN_ON_ERROR(CheckBuff(casterEntity, skillRow));
	RETURN_ON_ERROR(CheckState(casterEntity, skillRow));
	RETURN_ON_ERROR(CheckItemUse(casterEntity, skillRow));
	return kSuccess;
}

bool SkillSystem::IsSkillOfType(const uint32_t skillTableId, const uint32_t skillType) {
	LookupSkillOrFalse(skillTableId);

	for (auto& tabSkillType : skillRow->skill_type()) {
		if ((1 << tabSkillType) == skillType) {
			return true;
		}
	}

	return false;
}

void SkillSystem::HandleGeneralSkillSpell(const entt::entity casterEntity, const uint64_t skillId) {
    if (!tlsEcs.actorRegistry.valid(casterEntity))
    {
        return;
    }

	HandleSkillSpell(casterEntity, skillId);

	LOG_INFO << "Handling general skill spell. Caster: " << entt::to_integral(casterEntity)
		<< ", Skill ID: " << skillId;

	TriggerSkillEffect(casterEntity, skillId);
	HandleSkillRecovery(casterEntity, skillId);
}

// Set up a timer for skill recovery after casting
void SkillSystem::HandleSkillRecovery(const entt::entity casterEntity, uint64_t skillId) {
	auto& casterSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(casterEntity);
	auto skillContentIt = casterSkillContextMap.find(skillId);

	if (skillContentIt == casterSkillContextMap.end()) {
		return;
	}

	LookupSkillOrVoid(skillContentIt->second->skilltableid());

	auto& recoveryTimer = tlsEcs.actorRegistry.get_or_emplace<RecoveryTimerComp>(casterEntity).timer;
	recoveryTimer.RunAfter(skillRow->recovery_time(), [casterEntity, skillId] {
		return HandleSkillFinish(casterEntity, skillId);
		});
}

void SkillSystem::HandleSkillFinish(const entt::entity casterEntity, uint64_t skillId) {
    if (!tlsEcs.actorRegistry.valid(casterEntity))
    {
        return;
    }

	// TODO: Handle offline player
	auto& casterSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(casterEntity);
	auto skillContentIt = casterSkillContextMap.find(skillId);
	if (skillContentIt != casterSkillContextMap.end()) {
		entt::entity target = entt::to_entity(skillContentIt->second->target());
		if (tlsEcs.actorRegistry.valid(target)) {
			auto& targetSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(target);
			targetSkillContextMap.erase(skillId);
		}
		casterSkillContextMap.erase(skillContentIt);
	}
}

void SkillSystem::HandleChannelSkillSpell(entt::entity casterEntity, uint64_t skillId) {
    if (!tlsEcs.actorRegistry.valid(casterEntity))
    {
        return;
    }

	LookupSkillOrVoid(skillId);

	LOG_INFO << "Handling channel skill spell. Caster: " << entt::to_integral(casterEntity)
		<< ", Skill ID: " << skillId;

	HandleSkillSpell(casterEntity, skillId);

	auto& channelFinishTimer = tlsEcs.actorRegistry.get_or_emplace<ChannelFinishTimerComp>(casterEntity).timer;
	channelFinishTimer.RunAfter(skillRow->channel_finish(), [casterEntity, skillId] {
		return HandleChannelFinish(casterEntity, skillId);
		});

	auto& channelIntervalTimer = tlsEcs.actorRegistry.get_or_emplace<ChannelIntervalTimerComp>(casterEntity).timer;
	channelIntervalTimer.RunEvery(skillRow->channel_think(), [casterEntity, skillId] {
		return HandleChannelThink(casterEntity, skillId);
		});
}

void SkillSystem::HandleChannelThink(entt::entity casterEntity, uint64_t skillId) {
	// TODO: Implement channel think logic
}

void SkillSystem::HandleChannelFinish(const entt::entity casterEntity, const uint64_t skillId) {
    if (!tlsEcs.actorRegistry.valid(casterEntity))
    {
        return;
    }

	tlsEcs.actorRegistry.remove<ChannelIntervalTimerComp>(casterEntity);
	HandleSkillRecovery(casterEntity, skillId);
}

uint32_t SkillSystem::ValidateTarget(const ::ReleaseSkillRequest* request) {
	LookupSkill(request->skill_table_id());

	// Validate target ID
	if (!skillRow->targeting_mode().empty() && request->target_id() <= 0) {
		return MAKE_ERROR_MSG(kSkillInvalidTargetId,
			"target_id=" << request->target_id()
			<< " skill_table_id=" << request->skill_table_id());
	}

	uint32_t err = kSuccess;

	for (auto& tabSkillType : skillRow->targeting_mode()) {
		if ((1 << tabSkillType) == kNoTargetRequired) {
			return kSuccess;
		}

		if ((1 << tabSkillType) == kTargetedSkill) {
			entt::entity target{ request->target_id() };

			// Validate target entity
			if (!tlsEcs.actorRegistry.valid(target)) {
				return MAKE_ERROR_MSG(kSkillInvalidTargetId,
					"target_id=" << request->target_id()
					<< " skill_table_id=" << request->skill_table_id()
					<< " reason=entity_invalid");
			}

			// Check target entity type
			bool isValidTargetType = tlsEcs.actorRegistry.any_of<Player>(target) || tlsEcs.actorRegistry.any_of<Npc>(target);
			if (!isValidTargetType) {
				return MAKE_ERROR_MSG(kSkillInvalidTargetId,
					"target_id=" << request->target_id()
					<< " skill_table_id=" << request->skill_table_id()
					<< " reason=invalid_entity_type");
			}

			return kSuccess;
		}

		if ((1 << tabSkillType) == kAreaOfEffect) {
			return kSuccess;
		}
	}

	return err;
}

// Common interrupt-or-reject check for casting/recovery/channel timers
template <typename TimerComp>
uint32_t CheckTimerPhase(const entt::entity casterEntity, const SkillTable* skillTable) {
	auto& timerComp = tlsEcs.actorRegistry.get_or_emplace<TimerComp>(casterEntity);
	if (timerComp.timer.IsActive()) {
		if (skillTable->immediate()) {
			LOG_INFO << "Immediate skill: " << skillTable->id()
				<< " is currently in phase. Sending interrupt message.";
			SkillSystem::SendSkillInterruptedMessage(casterEntity, skillTable->id());
			tlsEcs.actorRegistry.remove<TimerComp>(casterEntity);
			return kSuccess;
		}
		return MAKE_ERROR_MSG(kSkillUnInterruptible,
			"skill_id=" << skillTable->id()
			<< " caster=" << entt::to_integral(casterEntity));
	}
	tlsEcs.actorRegistry.remove<TimerComp>(casterEntity);
	return kSuccess;
}

uint32_t SkillSystem::CheckCooldown(const entt::entity casterEntity, const SkillTable* skillTable) {
	auto& coolDownTimeListComp = tlsEcs.actorRegistry.get_or_emplace<CooldownTimeListComp>(casterEntity);
	if (const auto it = coolDownTimeListComp.cooldown_list().find(skillTable->cooldown_id());
		it != coolDownTimeListComp.cooldown_list().end() &&
		CoolDownTimeMillisecondSystem::IsInCooldown(it->second)) {
		return MAKE_ERROR_MSG(kSkillCooldownNotReady,
			"skill_id=" << skillTable->id()
			<< " caster=" << entt::to_integral(casterEntity)
			<< " cooldown_id=" << skillTable->cooldown_id()
			<< " remaining_ms=" << CoolDownTimeMillisecondSystem::Remaining(it->second));
	}

	return kSuccess;
}

uint32_t SkillSystem::CheckCasting(const entt::entity casterEntity, const SkillTable* skillTable) {
	return CheckTimerPhase<CastingTimerComp>(casterEntity, skillTable);
}

uint32_t SkillSystem::CheckRecovery(const entt::entity casterEntity, const SkillTable* skillTable) {
	return CheckTimerPhase<RecoveryTimerComp>(casterEntity, skillTable);
}

uint32_t SkillSystem::CheckChannel(const entt::entity casterEntity, const SkillTable* skillTable) {
	return CheckTimerPhase<ChannelFinishTimerComp>(casterEntity, skillTable);
}

void SkillSystem::BroadcastSkillUsedMessage(const entt::entity casterEntity, const ::ReleaseSkillRequest* request) {
	SkillUsedS2C skillUsedS2C;
	skillUsedS2C.set_entity(entt::to_integral(casterEntity));
	skillUsedS2C.add_target_entity(request->target_id());
	skillUsedS2C.set_skill_table_id(request->skill_table_id());
	skillUsedS2C.mutable_position()->CopyFrom(request->position());

	ViewSystem::BroadcastMessageToVisiblePlayers(
		casterEntity,
		SceneSkillClientPlayerNotifySkillUsedMessageId,
		skillUsedS2C
	);
}

void SkillSystem::SetupCastingTimer(entt::entity casterEntity, const SkillTable* skillTable, uint64_t skillId) {
	auto& castingTimer = tlsEcs.actorRegistry.get_or_emplace<CastingTimerComp>(casterEntity).timer;
	if (IsSkillOfType(skillTable->id(), kGeneralSkill)) {
		castingTimer.RunAfter(skillTable->cast_point(), [casterEntity, skillId] {
			return HandleGeneralSkillSpell(casterEntity, skillId);
			});
	}
	else if (IsSkillOfType(skillTable->id(), kChannelSkill)) {
		castingTimer.RunAfter(skillTable->cast_point(), [casterEntity, skillId] {
			return HandleChannelSkillSpell(casterEntity, skillId);
			});
	}
}

void SkillSystem::SendSkillInterruptedMessage(const entt::entity casterEntity, const uint32_t skillTableId) {
	SkillInterruptedS2C skillInterruptedS2C;
	skillInterruptedS2C.set_entity(entt::to_integral(casterEntity));
	skillInterruptedS2C.set_skill_table_id(skillTableId);

	ViewSystem::BroadcastMessageToVisiblePlayers(
		casterEntity,
		SceneSkillClientPlayerNotifySkillInterruptedMessageId,
		skillInterruptedS2C
	);
}

void SkillSystem::TriggerSkillEffect(const entt::entity casterEntity, const uint64_t skillId) {
	auto& casterSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(casterEntity);
	const auto skillContextIt = casterSkillContextMap.find(skillId);

	if (skillContextIt == casterSkillContextMap.end()) {
		return;
	}

	const auto& skillContext = skillContextIt->second;
	
	LookupSkillOrVoid(skillContext->skilltableid());

	LOG_INFO << "Triggering skill effect. Caster: " << entt::to_integral(casterEntity) << ", Skill ID: " << skillId;

	for (const auto& effect : skillRow->effect()) {
		BuffSystem::AddOrUpdateBuff(entt::to_entity(skillContext->target()), effect, skillContext);
	}
}

bool IsTargetDead(entt::entity targetEntity) {
    auto& targetBaseAttributes = tlsEcs.actorRegistry.get_or_emplace<BaseAttributesComp>(targetEntity);
    return targetBaseAttributes.health() <= 0;
}


double CalculateFinalDamage(const entt::entity casterEntity, const entt::entity target, double baseDamage) {
    auto& casterAttributes = tlsEcs.actorRegistry.get_or_emplace<BaseAttributesComp>(casterEntity);
    double critChance = casterAttributes.critchance();
    double strength = casterAttributes.strength();

    auto& targetAttributes = tlsEcs.actorRegistry.get_or_emplace<BaseAttributesComp>(target);
    double armor = targetAttributes.armor();
    double resistance = targetAttributes.resistance();

    // Apply crit and penetration modifiers
    double finalDamage = baseDamage * (1 + strength * 0.1);
    finalDamage = finalDamage - armor;
    finalDamage *= (1 - resistance * 0.01);

    if (rand() / static_cast<double>(RAND_MAX) < critChance) {
        finalDamage *= 2;
    }

    return std::max(finalDamage, 0.0);
}


void CalculateSkillDamage(const entt::entity casterEntity, DamageEventComp& damageEvent) {
    auto& casterSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(casterEntity);
    auto skillContentIt = casterSkillContextMap.find(damageEvent.skill_id());

    if (skillContentIt == casterSkillContextMap.end()) {
        LOG_ERROR << "Skill context not found for skill ID: " << damageEvent.skill_id();
        return;
    }

	LookupSkillOrVoid(skillContentIt->second->skilltableid());

    auto targetEntity = entt::to_entity(damageEvent.target());

	if (!tlsEcs.actorRegistry.valid(targetEntity))
	{
		return;
	}

    if (IsTargetDead(targetEntity)) {
        LOG_INFO << "Target is already dead, skipping damage calculation.";
        return;
    }

    auto& levelComponent = tlsEcs.actorRegistry.get_or_emplace<LevelComp>(casterEntity);
    SkillTableManager::Instance().SetDamageParam({ static_cast<double>(levelComponent.level()) });

    damageEvent.set_attacker_id(entt::to_integral(casterEntity));

    double baseDamage = SkillTableManager::Instance().GetDamage(skillContentIt->second->skilltableid());
    double finalDamage = CalculateFinalDamage(casterEntity, targetEntity, baseDamage);
    damageEvent.set_damage(finalDamage);
}


void TriggerBeforeDamageEvents(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent) {
    BuffSystem::OnBeforeGiveDamage(casterEntity, targetEntity, damageEvent);
    BuffSystem::OnBeforeTakeDamage(casterEntity, targetEntity, damageEvent);
}

void ApplyDamage(BaseAttributesComp& baseAttributesPBComponent, const DamageEventComp& damageEvent) {
    const auto damage = static_cast<uint64_t>(std::ceil(damageEvent.damage()));

    if (baseAttributesPBComponent.health() > damage) {
        baseAttributesPBComponent.set_health(baseAttributesPBComponent.health() - damage);
    }
    else {
        baseAttributesPBComponent.set_health(0);
    }
}

void TriggerBeKillEvent(const entt::entity casterEntity, const entt::entity target) {
	BeKillEvent beKillEvent;
	beKillEvent.set_caster(entt::to_integral(casterEntity));
	beKillEvent.set_target(entt::to_integral(target));

	tlsEcs.dispatcher.trigger(beKillEvent);
}

void TriggerAfterDamageEvents(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventComp& damageEvent) {
	BuffSystem::OnAfterGiveDamage(casterEntity, targetEntity, damageEvent);
	BuffSystem::OnAfterTakeDamage(casterEntity, targetEntity, damageEvent);
}

void HandleTargetDeath(const entt::entity casterEntity, const entt::entity target, const DamageEventComp& damageEvent) {
    BuffSystem::OnBeforeDead(target); 
    BuffSystem::OnAfterDead(target);

    // Trigger kill event if not self-damage
    if (casterEntity != target) {
        BuffSystem::OnKill(casterEntity);
    }

    TriggerBeKillEvent(casterEntity, target);
}

void DealDamage(DamageEventComp& damageEvent, const entt::entity caster, const entt::entity target) {
	auto& baseAttributesPBComponent = tlsEcs.actorRegistry.get_or_emplace<BaseAttributesComp>(target);

	if (IsTargetDead(target)) {
		return;
	}

	damageEvent.set_target(entt::to_integral(target)); 
	TriggerBeforeDamageEvents(caster, target, damageEvent);
	ApplyDamage(baseAttributesPBComponent, damageEvent);

	if (IsTargetDead(target)) {
		HandleTargetDeath(caster, target, damageEvent);
	}

	TriggerAfterDamageEvents(caster, target, damageEvent);
}

void SkillSystem::HandleSkillSpell(const entt::entity casterEntity, const uint64_t skillId) {
	auto& casterSkillContextMap = tlsEcs.actorRegistry.get_or_emplace<SkillContextCompMap>(casterEntity);
	const auto skillContextIt = casterSkillContextMap.find(skillId);

	if (skillContextIt == casterSkillContextMap.end()) {
		return;
	}

	const auto& skillContext = skillContextIt->second;

	const entt::entity targetEntity = entt::to_entity(skillContext->target());

	if (!tlsEcs.actorRegistry.valid(targetEntity))
	{
		return;
	}
    
	DamageEventComp damageEvent;
	damageEvent.set_skill_id(skillId);
	damageEvent.set_target(skillContext->target());
	CalculateSkillDamage(casterEntity, damageEvent);
	DealDamage(damageEvent, casterEntity, targetEntity);

	SkillExecutedEvent skillExecutedEvent;
	skillExecutedEvent.set_caster(entt::to_integral(casterEntity));
	skillExecutedEvent.set_target(skillContext->target());
	BuffSystem::OnSkillExecuted(skillExecutedEvent);
}


