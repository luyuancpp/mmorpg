#pragma once

#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

#include "logic/client_player/player_ability.pb.h"
//https://zhuanlan.zhihu.com/p/149704315

class SkillTable;

class SkillUtil
{
public:
	static uint32_t UseSkill(entt::entity caster, const ::UseSkillRequest* request);
	static uint32_t CheckSkillPrerequisites(entt::entity caster, const ::UseSkillRequest* request);

	static bool IsSkillOfType(uint32_t abilityId, uint32_t abilityType);

	static void HandleSkillInitialize();
	static void HandleGeneralSkillSpell(const entt::entity caster, uint32_t abilityId);
	static void HandleSkillRecovery(const entt::entity caster, uint32_t abilityId);
	static void HandleSkillFinish(const entt::entity caster, uint32_t abilityId);

	static void HandleChannelSkillSpell(entt::entity caster, uint32_t abilityId);
	static void HandleChannelThink(entt::entity caster, uint32_t abilityId);
	static void HandleChannelFinish(entt::entity caster, uint32_t abilityId);

	static void HandleSkillToggleOn(entt::entity caster, uint32_t abilityId);
	static void HandleSkillToggleOff(entt::entity caster, uint32_t abilityId);

	static void HandleSkillActivate(entt::entity caster, uint32_t abilityId);
	static void HandleSkillDeactivate(entt::entity caster, uint32_t abilityId);

    // 验证目标
    static uint32_t ValidateTarget(const ::UseSkillRequest* request);

    // 检查冷却时间
    static uint32_t CheckCooldown(entt::entity caster, const SkillTable* tableSkill);

    // 处理施法计时器
    static uint32_t CheckCasting(entt::entity caster, const SkillTable* tableSkill);

	static uint32_t CheckRecovery(const entt::entity caster, const SkillTable* tableSkill);

	static uint32_t CheckChannel(const entt::entity caster, const SkillTable* tableSkill);

    // 广播技能使用消息
    static void BroadcastSkillUsedMessage(entt::entity caster, const ::UseSkillRequest* request);
	
    // 设置施法计时器
    static void SetupCastingTimer(entt::entity caster, const SkillTable* tableSkill, uint32_t abilityId);

    // 发送技能中断消息
    static void SendSkillInterruptedMessage(entt::entity caster, uint32_t abilityId) ;

	static void TriggerSkillEffect(entt::entity caster, uint32_t abilityId);

	static void RemoveEffect(entt::entity caster, uint32_t abilityId);

	static void HandleSkillSpell(const entt::entity caster, uint32_t abilityId);
};