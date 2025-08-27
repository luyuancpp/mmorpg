#include "skill_system.h"

#include <muduo/base/Logging.h>

#include "proto/table/entity_error_tip.pb.h"
#include "table/skillpermission_config.h"
#include "table/skill_config.h"
#include "actor/action_state/constants/actor_state_constants.h"
#include "actor/action_state/system/actor_action_state_system.h"
#include "scene/combat_state/system/combat_state_system.h"
#include "scene/combat/buff/system/buff_system.h"
#include "scene/combat/skill/comp/skill_comp.h"
#include "scene/combat/skill/constants/skill_constants.h"
#include "scene/system/view_system.h"
#include "proto/logic/event/combat_event.pb.h"
#include "proto/logic/event/skill_event.pb.h"
#include "macros/return_define.h"
#include "proto/table/common_error_tip.pb.h"
#include "proto/table/skill_error_tip.pb.h"
#include "proto/logic/component/buff_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"
#include "service_info/player_skill_service_info.h"
#include "proto/logic/component/actor_combat_state_comp.pb.h"

#include "time/comp/timer_task_comp.h"
#include "time/system/cooldown_time_system.h"
#include "time/system/time_system.h"
#include <core/system/id_generator_manager.h>
#include <threading/dispatcher_manager.h>

uint64_t GenerateUniqueSkillId(const SkillContextCompMap& casterBuffList, const SkillContextCompMap& targetBuffList) {
	uint64_t newSkillId;
	do {
		newSkillId = tlsIdGeneratorManager.skillIdGenerator.Generate();
	} while (casterBuffList.contains(newSkillId) || targetBuffList.contains(newSkillId));
	return newSkillId;
}

void SkillSystem::InitializeActorComponents(entt::entity entity) {
	tlsRegistryManager.actorRegistry.emplace<SkillContextCompMap>(entity);
	tlsRegistryManager.actorRegistry.emplace<CooldownTimeListComp>(entity);
}

void SkillSystem::StartCooldown(entt::entity caster, const SkillTable* skillTable) {
	if (auto* coolDownComp = tlsRegistryManager.actorRegistry.try_get<CooldownTimeListComp>(caster)) {
		CooldownTimeComp comp;
		comp.set_start(TimeUtil::NowMilliseconds());
		comp.set_cooldown_table_id(skillTable->cooldown_id());

		const auto coolDownList = coolDownComp->mutable_cooldown_list();
		(*coolDownList)[skillTable->cooldown_id()] = comp;
	}
}

void LookAtTargetPosition(entt::entity caster, const ReleaseSkillSkillRequest* request) {
	if (request->has_position()) {
		ViewSystem::LookAtPosition(caster, request->position());
	} else if (request->target_id() > 0) {
		const entt::entity target{ request->target_id() };
		if (const auto transform = tlsRegistryManager.actorRegistry.try_get<Transform>(target)) {
			ViewSystem::LookAtPosition(caster, transform->location());
		}
	}
}

std::shared_ptr<SkillContextPBComponent> CreateSkillContext(entt::entity caster, const ReleaseSkillSkillRequest* request) {
	auto context = std::make_shared<SkillContextPBComponent>();
	context->set_caster(entt::to_integral(caster));
	context->set_skilltableid(request->skill_table_id());
	context->set_target(request->target_id());
	context->set_casttime(TimeUtil::NowMilliseconds());
	context->set_skillid(GenerateUniqueSkillId(tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(caster), {}));
	return context;
}

void AddSkillContext(entt::entity caster, const ReleaseSkillSkillRequest* request, std::shared_ptr<SkillContextPBComponent> context) {
	auto& casterSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(caster);
	casterSkillContextMap.emplace(context->skillid(), context);

	entt::entity target{ request->target_id() };
	if (tlsRegistryManager.actorRegistry.valid(target)) {
		auto& targetSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(target);
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

void ApplySkillHitEffectIfValid(const entt::entity casterEntity, const uint64_t targetId) {
	const entt::entity targetEntity{targetId};
	if (!tlsRegistryManager.actorRegistry.valid(targetEntity)) {
		return;
	}
	BuffSystem::OnSkillHit(casterEntity, targetEntity);
}

uint32_t SkillSystem::ReleaseSkill(const entt::entity casterEntity, const ReleaseSkillSkillRequest* request) {
	FetchAndValidateSkillTable(request->skill_table_id());

	RETURN_ON_ERROR(CheckSkillPrerequisites(casterEntity, request));
	LookAtTargetPosition(casterEntity, request);
	BroadcastSkillUsedMessage(casterEntity, request);
    
	const auto context = CreateSkillContext(casterEntity, request);
	AddSkillContext(casterEntity, request, context);
    
	ConsumeItems(casterEntity, skillTable);
	ConsumeResources(casterEntity, skillTable);
	StartCooldown(casterEntity, skillTable);
	SetupCastingTimer(casterEntity, skillTable, context->skillid());

	ApplySkillHitEffectIfValid(casterEntity, request->target_id());

	return kSuccess;
}

uint32_t CheckPlayerLevel(const entt::entity casterEntity, const SkillTable* skillTable) {
	if (!tlsRegistryManager.actorRegistry.any_of<Player>(casterEntity))
	{
		return  kSuccess;
	}
	return kSuccess;
}

uint32_t canUseSkillInCurrentState(const uint32_t state, const uint32_t skill) {
	FetchAndValidateSkillPermissionTable(state);

	const auto skillTypeIndex = (1 << skill);
	if (skillTypeIndex >= skillPermissionTable->skilltype_size())
	{
		return kInvalidTableData;
	}
	
	return  skillPermissionTable->skilltype(skillTypeIndex);
}


uint32_t CheckBuff(const entt::entity casterEntity, const SkillTable* skillTable) {

	auto& combatStateCollection = tlsRegistryManager.actorRegistry.get<CombatStateCollectionPbComponent>(casterEntity);

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
	for (auto& item : skillTable->requireditem()){
		
	}
	return kSuccess;
}

uint32_t SkillSystem::CheckSkillPrerequisites(const entt::entity casterEntity, const ::ReleaseSkillSkillRequest* request) {
	FetchAndValidateSkillTable(request->skill_table_id());

	RETURN_ON_ERROR(ValidateTarget(request));
	RETURN_ON_ERROR(CheckCooldown(casterEntity, skillTable));
	RETURN_ON_ERROR(CheckCasting(casterEntity, skillTable));
	RETURN_ON_ERROR(CheckRecovery(casterEntity, skillTable));
	RETURN_ON_ERROR(CheckChannel(casterEntity, skillTable));
	RETURN_ON_ERROR(CheckPlayerLevel(casterEntity, skillTable));
	RETURN_ON_ERROR(CheckBuff(casterEntity, skillTable));
	RETURN_ON_ERROR(CheckState(casterEntity, skillTable));
	RETURN_ON_ERROR(CheckItemUse(casterEntity, skillTable));
	return kSuccess;
}

bool SkillSystem::IsSkillOfType(const uint32_t skillTableId, const uint32_t skillType) {
	FetchSkillTableOrReturnFalse(skillTableId);

	for (auto& tabSkillType : skillTable->skill_type()) {
		if ((1 << tabSkillType) == skillType) {
			return true;
		}
	}

	return false;
}

void SkillSystem::HandleSkillInitialize() {
	// Implementation here
}

void SkillSystem::HandleGeneralSkillSpell(const entt::entity casterEntity, const uint64_t skillId) {
    if (!tlsRegistryManager.actorRegistry.valid(casterEntity))
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
	auto& casterSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(casterEntity);
	auto skillContentIt = casterSkillContextMap.find(skillId);

	if (skillContentIt == casterSkillContextMap.end()) {
		return;
	}

	FetchSkillTableOrReturnVoid(skillContentIt->second->skilltableid());

	auto& recoveryTimer = tlsRegistryManager.actorRegistry.get_or_emplace<RecoveryTimerComp>(casterEntity).timer;
	recoveryTimer.RunAfter(skillTable->recoverytime(), [casterEntity, skillId] {
		return HandleSkillFinish(casterEntity, skillId);
		});
}

void SkillSystem::HandleSkillFinish(const entt::entity casterEntity, uint64_t skillId) {
    if (!tlsRegistryManager.actorRegistry.valid(casterEntity))
    {
        return;
    }

	// todo player off line 
	auto& casterSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(casterEntity);
	auto skillContentIt = casterSkillContextMap.find(skillId);
	if (skillContentIt != casterSkillContextMap.end()) {
		entt::entity target = entt::to_entity(skillContentIt->second->target());
		if (tlsRegistryManager.actorRegistry.valid(target)) {
			auto& targetSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(target);
			targetSkillContextMap.erase(skillId);
		}
		casterSkillContextMap.erase(skillContentIt);
	}
}

void SkillSystem::HandleChannelSkillSpell(entt::entity casterEntity, uint64_t skillId) {
    if (!tlsRegistryManager.actorRegistry.valid(casterEntity))
    {
        return;
    }

	FetchSkillTableOrReturnVoid(skillId);

	LOG_INFO << "Handling channel skill spell. Caster: " << entt::to_integral(casterEntity)
		<< ", Skill ID: " << skillId;

	HandleSkillSpell(casterEntity, skillId);

	auto& channelFinishTimer = tlsRegistryManager.actorRegistry.get_or_emplace<ChannelFinishTimerComp>(casterEntity).timer;
	channelFinishTimer.RunAfter(skillTable->channelfinish(), [casterEntity, skillId] {
		return HandleChannelFinish(casterEntity, skillId);
		});

	auto& channelIntervalTimer = tlsRegistryManager.actorRegistry.get_or_emplace<ChannelIntervalTimerComp>(casterEntity).timer;
	channelIntervalTimer.RunEvery(skillTable->channelthink(), [casterEntity, skillId] {
		return HandleChannelThink(casterEntity, skillId);
		});
}

// Placeholder for channeling think logic
void SkillSystem::HandleChannelThink(entt::entity casterEntity, uint64_t skillId) {
	// TODO: Implement channel think logic here
}

void SkillSystem::HandleChannelFinish(const entt::entity casterEntity, const uint64_t skillId) {
    if (!tlsRegistryManager.actorRegistry.valid(casterEntity))
    {
        return;
    }

	tlsRegistryManager.actorRegistry.remove<ChannelIntervalTimerComp>(casterEntity);
	HandleSkillRecovery(casterEntity, skillId);
}

void SkillSystem::HandleSkillToggleOn(const entt::entity casterEntity, const uint64_t skillId) {
	TriggerSkillEffect(casterEntity, skillId);
}

void SkillSystem::HandleSkillToggleOff(const entt::entity casterEntity, const uint64_t skillId) {
	RemoveEffect(casterEntity, skillId);
}

void SkillSystem::HandleSkillActivate(const entt::entity casterEntity, const uint64_t skillId) {
	TriggerSkillEffect(casterEntity, skillId);
}

void SkillSystem::HandleSkillDeactivate(const entt::entity casterEntity, const uint64_t skillId) {
	RemoveEffect(casterEntity, skillId);
}

uint32_t SkillSystem::ValidateTarget(const ::ReleaseSkillSkillRequest* request) {
	FetchAndValidateSkillTable(request->skill_table_id());

	// 检查目标ID的有效性
	if (!skillTable->targeting_mode().empty() && request->target_id() <= 0) {
		LOG_ERROR << "Invalid target ID: " << request->target_id()
			<< " provided for skill ID: " << request->skill_table_id()
			<< ". Target ID must be positive if target type is specified.";
		return kSkillInvalidTargetId;
	}

	// 默认错误状态
	uint32_t err = kSuccess;

	// 遍历技能目标类型
	for (auto& tabSkillType : skillTable->targeting_mode()) {
		// 检查不需要目标的情况
		if ((1 << tabSkillType) == kNoTargetRequired) {
			return kSuccess;  // 无需进一步检查
		}

		// 检查是否为目标技能
		if ((1 << tabSkillType) == kTargetedSkill) {
			entt::entity target{ request->target_id() };

			// 验证目标实体
			if (!tlsRegistryManager.actorRegistry.valid(target)) {
				LOG_ERROR << "Target entity with ID: " << request->target_id()
					<< " is invalid or does not exist for skill ID: " << request->skill_table_id();
				return kSkillInvalidTargetId;
			}

			// 检查目标实体类型
			bool isValidTargetType = tlsRegistryManager.actorRegistry.any_of<Player>(target) || tlsRegistryManager.actorRegistry.any_of<Npc>(target);
			if (!isValidTargetType) {
				LOG_ERROR << "Target entity with ID: " << request->target_id()
					<< " is of an invalid type for skill ID: " << request->skill_table_id()
					<< ". Expected Player or Npc.";
				return kSkillInvalidTargetId;
			}

			return kSuccess;  // 验证通过
		}

		// 检查范围技能
		if ((1 << tabSkillType) == kAreaOfEffect) {
			return kSuccess;  // 验证通过
		}
	}

	return err;  // 返回错误状态（如果有）
}

uint32_t SkillSystem::CheckCooldown(const entt::entity casterEntity, const SkillTable* skillTable) {
	if (const auto* coolDownTimeListComp = tlsRegistryManager.actorRegistry.try_get<CooldownTimeListComp>(casterEntity)) {
		if (const auto it = coolDownTimeListComp->cooldown_list().find(skillTable->cooldown_id());
			it != coolDownTimeListComp->cooldown_list().end() &&
			CoolDownTimeMillisecondSystem::IsInCooldown(it->second)) {
			LOG_ERROR << "Skill ID: " << skillTable->id()
				<< " is in cooldown for player: " << entt::to_integral(casterEntity)
				<< ". Cooldown ID: " << skillTable->cooldown_id()
				<< ". Time remaining: " << CoolDownTimeMillisecondSystem::Remaining(it->second) << "ms";
			return kSkillCooldownNotReady;
		}
	}

	return kSuccess;
}

uint32_t SkillSystem::CheckCasting(const entt::entity casterEntity, const SkillTable* skillTable) {
	if (auto* castTimerComp = tlsRegistryManager.actorRegistry.try_get<CastingTimerComp>(casterEntity)) {
		if (skillTable->immediately() && castTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate skill: " << skillTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendSkillInterruptedMessage(casterEntity, skillTable->id());
			tlsRegistryManager.actorRegistry.remove<CastingTimerComp>(casterEntity);
			return kSuccess;
		}
		
		if (!skillTable->immediately() && castTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate skill: " << skillTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kSkillUnInterruptible;
		}
		tlsRegistryManager.actorRegistry.remove<CastingTimerComp>(casterEntity);
	}

	return kSuccess;
}

uint32_t SkillSystem::CheckRecovery(const entt::entity casterEntity, const SkillTable* skillTable) {
	if (auto* recoveryTimeTimerComp = tlsRegistryManager.actorRegistry.try_get<RecoveryTimerComp>(casterEntity)) {
		if (skillTable->immediately() && recoveryTimeTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate skill: " << skillTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendSkillInterruptedMessage(casterEntity, skillTable->id());
			tlsRegistryManager.actorRegistry.remove<RecoveryTimerComp>(casterEntity);
			return kSuccess;
		}
		
		if (!skillTable->immediately() && recoveryTimeTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate skill: " << skillTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kSkillUnInterruptible;
		}
		tlsRegistryManager.actorRegistry.remove<RecoveryTimerComp>(casterEntity);
	}

	return kSuccess;
}

uint32_t SkillSystem::CheckChannel(const entt::entity casterEntity, const SkillTable* skillTable) {
	if (auto* channelFinishTimerComp = tlsRegistryManager.actorRegistry.try_get<ChannelFinishTimerComp>(casterEntity)) {
		if (skillTable->immediately() && channelFinishTimerComp->timer.IsActive()) {
			LOG_INFO << "Immediate skill: " << skillTable->id()
				<< " is currently casting. Sending interrupt message.";
			SendSkillInterruptedMessage(casterEntity, skillTable->id());
			// TODO: Implement logic for handling the skill interruption
			tlsRegistryManager.actorRegistry.remove<ChannelFinishTimerComp>(casterEntity);
			return kSuccess;
		}

		if (!skillTable->immediately() && channelFinishTimerComp->timer.IsActive()) {
			LOG_ERROR << "Non-immediate skill: " << skillTable->id()
				<< " is currently casting and cannot be interrupted.";
			return kSkillUnInterruptible;
		}

		// TODO: Implement logic for handling the skill interruption
		tlsRegistryManager.actorRegistry.remove<ChannelFinishTimerComp>(casterEntity);
	}

	return kSuccess;
}

void SkillSystem::BroadcastSkillUsedMessage(const entt::entity casterEntity, const ::ReleaseSkillSkillRequest* request) {
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
	auto& castingTimer = tlsRegistryManager.actorRegistry.get_or_emplace<CastingTimerComp>(casterEntity).timer;
	if (IsSkillOfType(skillTable->id(), kGeneralSkill)) {
		castingTimer.RunAfter(skillTable->castpoint(), [casterEntity, skillId] {
			return HandleGeneralSkillSpell(casterEntity, skillId);
			});
	}
	else if (IsSkillOfType(skillTable->id(), kChannelSkill)) {
		castingTimer.RunAfter(skillTable->castpoint(), [casterEntity, skillId] {
			return HandleChannelSkillSpell(casterEntity, skillId);
			});
	}
}

void SkillSystem::SendSkillInterruptedMessage(const entt::entity casterEntity, const uint32_t skillTableId) {
	SkillInterruptedS2C skillInterruptedS2C;
	skillInterruptedS2C.set_entity(entt::to_integral(casterEntity));
	skillInterruptedS2C.set_skill_table_id(skillTableId);
	//skillInterruptedS2C.set_skill_id(skillTableID);

	ViewSystem::BroadcastMessageToVisiblePlayers(
		casterEntity,
		SceneSkillClientPlayerNotifySkillInterruptedMessageId,
		skillInterruptedS2C
	);
}

void SkillSystem::TriggerSkillEffect(const entt::entity casterEntity, const uint64_t skillId) {
	auto& casterSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(casterEntity);
	const auto skillContextIt = casterSkillContextMap.find(skillId);

	if (skillContextIt == casterSkillContextMap.end()) {
		return;
	}

	const auto& skillContext = skillContextIt->second;
	
	FetchSkillTableOrReturnVoid(skillContext->skilltableid());

	LOG_INFO << "Triggering skill effect. Caster: " << entt::to_integral(casterEntity) << ", Skill ID: " << skillId;

	for (const auto& effect : skillTable->effect()) {
		BuffSystem::AddOrUpdateBuff(entt::to_entity(skillContext->target()), effect, skillContext);
	}
}

void SkillSystem::RemoveEffect(entt::entity casterEntity, const uint64_t skillId) {
	auto& casterSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(casterEntity);
	auto skillContentIt = casterSkillContextMap.find(skillId);

	if (skillContentIt == casterSkillContextMap.end()) {
		return;
	}
	
	FetchSkillTableOrReturnVoid(skillContentIt->second->skilltableid());

	for (const auto& effect : skillTable->effect()) {
		// TODO: Implement effect removal logic here
	}
}


// 判断目标是否已死亡
bool IsTargetDead(entt::entity targetEntity) {
    auto& targetBaseAttributes = tlsRegistryManager.actorRegistry.get<BaseAttributesPbComponent>(targetEntity);
    return targetBaseAttributes.health() <= 0;
}


double CalculateFinalDamage(const entt::entity casterEntity, const entt::entity target, double baseDamage) {
    // 获取施法者的属性，例如力量和暴击率
    auto& casterAttributes = tlsRegistryManager.actorRegistry.get<BaseAttributesPbComponent>(casterEntity);
    double critChance = casterAttributes.critchance();
    double strength = casterAttributes.strength();

    // 获取目标的属性，例如护甲和抗性
    auto& targetAttributes = tlsRegistryManager.actorRegistry.get<BaseAttributesPbComponent>(target);
    double armor = targetAttributes.armor();
    double resistance = targetAttributes.resistance();

    // 计算暴击和穿透的影响
    double finalDamage = baseDamage * (1 + strength * 0.1);  // 力量影响伤害
    finalDamage = finalDamage - armor;  // 减去护甲值
    finalDamage *= (1 - resistance * 0.01);  // 计算抗性的减伤效果

    // 暴击处理
    if (rand() / static_cast<double>(RAND_MAX) < critChance) {
        finalDamage *= 2;  // 暴击造成双倍伤害
    }

    return std::max(finalDamage, 0.0);  // 确保伤害不为负数
}


void CalculateSkillDamage(const entt::entity casterEntity, DamageEventPbComponent& damageEvent) {
    // 获取施法者的技能上下文
    auto& casterSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(casterEntity);
    auto skillContentIt = casterSkillContextMap.find(damageEvent.skill_id());

    if (skillContentIt == casterSkillContextMap.end()) {
        LOG_ERROR << "Skill context not found for skill ID: " << damageEvent.skill_id();
        return;
    }

    // 获取技能表
	FetchSkillTableOrReturnVoid(skillContentIt->second->skilltableid());

    // 获取目标的 BaseAttributesPbComponent 用于判断是否死亡
    auto targetEntity = entt::to_entity(damageEvent.target());

	if (!tlsRegistryManager.actorRegistry.valid(targetEntity))
	{
		return;
	}

    // 如果目标已经死亡，停止进一步处理
    if (IsTargetDead(targetEntity)) {
        LOG_INFO << "Target is already dead, skipping damage calculation.";
        return;
    }

    // 获取施法者的等级组件并设置伤害参数
    auto& levelComponent = tlsRegistryManager.actorRegistry.get<LevelPbComponent>(casterEntity);
    SkillConfigurationTable::Instance().SetDamageParam({ static_cast<double>(levelComponent.level()) });

    // 设置攻击者 ID
    damageEvent.set_attacker_id(entt::to_integral(casterEntity));

    // 计算技能的基础伤害
    double baseDamage = SkillConfigurationTable::Instance().GetDamage(skillContentIt->second->skilltableid());

    // 计算最终伤害
    double finalDamage = CalculateFinalDamage(casterEntity, targetEntity, baseDamage);

    // 设置最终的伤害值
    damageEvent.set_damage(finalDamage);
}


// 触发伤害前的事件
void TriggerBeforeDamageEvents(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent) {
    BuffSystem::OnBeforeGiveDamage(casterEntity, targetEntity, damageEvent);
    BuffSystem::OnBeforeTakeDamage(casterEntity, targetEntity, damageEvent);
}

// 处理目标生命值的减少
void ApplyDamage(BaseAttributesPbComponent& baseAttributesPBComponent, const DamageEventPbComponent& damageEvent) {
    const auto damage = static_cast<uint64_t>(std::ceil(damageEvent.damage()));

    if (baseAttributesPBComponent.health() > damage) {
        baseAttributesPBComponent.set_health(baseAttributesPBComponent.health() - damage);
    }
    else {
        baseAttributesPBComponent.set_health(0);
    }
}

// 触发被击杀事件
void TriggerBeKillEvent(const entt::entity casterEntity, const entt::entity target) {
	BeKillEvent beKillEvent;
	beKillEvent.set_caster(entt::to_integral(casterEntity));
	beKillEvent.set_target(entt::to_integral(target));

	dispatcher.trigger(beKillEvent);
}

// 触发伤害后的事件
void TriggerAfterDamageEvents(const entt::entity casterEntity, const entt::entity targetEntity, DamageEventPbComponent& damageEvent) {
	BuffSystem::OnAfterGiveDamage(casterEntity, targetEntity, damageEvent);
	BuffSystem::OnAfterTakeDamage(casterEntity, targetEntity, damageEvent);
}

// 处理目标死亡逻辑
void HandleTargetDeath(const entt::entity casterEntity, const entt::entity target, const DamageEventPbComponent& damageEvent) {
    // 触发死亡前的事件
    BuffSystem::OnBeforeDead(target); 

    // 触发死亡后的事件
    BuffSystem::OnAfterDead(target);

    // 如果不是自杀，触发击杀事件
    if (casterEntity != target) {
        BuffSystem::OnKill(casterEntity);
    }

    // 生成并触发被击杀事件
    TriggerBeKillEvent(casterEntity, target);
}

// 处理具体的伤害逻辑
void DealDamage(DamageEventPbComponent& damageEvent, const entt::entity caster, const entt::entity target) {
	auto& baseAttributesPBComponent = tlsRegistryManager.actorRegistry.get<BaseAttributesPbComponent>(target);

	// 如果目标已死亡，直接返回
	if (IsTargetDead(target)) {
		return;
	}

	// 设置伤害事件的目标
	damageEvent.set_target(entt::to_integral(target)); 

	// 触发伤害前事件
	TriggerBeforeDamageEvents(caster, target, damageEvent);

	// 处理目标生命值的减少
	ApplyDamage(baseAttributesPBComponent, damageEvent);

	// 检查目标是否死亡
	if (IsTargetDead(target)) {
		HandleTargetDeath(caster, target, damageEvent);
	}

	// 触发伤害后事件
	TriggerAfterDamageEvents(caster, target, damageEvent);
}

void SkillSystem::HandleSkillSpell(const entt::entity casterEntity, const uint64_t skillId) {
	auto& casterSkillContextMap = tlsRegistryManager.actorRegistry.get<SkillContextCompMap>(casterEntity);
	const auto skillContextIt = casterSkillContextMap.find(skillId);

	if (skillContextIt == casterSkillContextMap.end()) {
		return;
	}

	const auto& skillContext = skillContextIt->second;

	const entt::entity targetEntity = entt::to_entity(skillContext->target());

	if (!tlsRegistryManager.actorRegistry.valid(targetEntity))
	{
		return;
	}
    
	DamageEventPbComponent damageEvent;
	damageEvent.set_skill_id(skillId);
	damageEvent.set_target(skillContext->target());
	CalculateSkillDamage(casterEntity, damageEvent); // 计算伤害
	DealDamage(damageEvent, casterEntity, targetEntity); // 处理伤害

	// 触发技能执行事件
	SkillExecutedEvent skillExecutedEvent;
	skillExecutedEvent.set_caster(entt::to_integral(casterEntity));
	skillExecutedEvent.set_target(skillContext->target());
	BuffSystem::OnSkillExecuted(skillExecutedEvent);
}

