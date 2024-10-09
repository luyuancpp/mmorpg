#pragma once

#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

#include "logic/client_player/player_skill.pb.h"
//https://zhuanlan.zhihu.com/p/149704315

class SkillTable;

class SkillUtil
{
public:
	static void InitializePlayerComponents(entt::entity entity);

	static void StartCooldown(const entt::entity caster, const SkillTable* skillTable);

	static uint32_t ReleaseSkill(entt::entity caster, const ::ReleaseSkillSkillRequest* request);
	static uint32_t CheckSkillPrerequisites(entt::entity caster, const ::ReleaseSkillSkillRequest* request);

	static bool IsSkillOfType(const uint32_t skillTableID, const uint32_t skillType);

	static void HandleSkillInitialize();
	static void HandleGeneralSkillSpell(const entt::entity caster, uint64_t skillId);
	static void HandleSkillRecovery(const entt::entity caster, uint64_t skillId);
	static void HandleSkillFinish(const entt::entity caster, uint64_t skillId);

	static void HandleChannelSkillSpell(entt::entity caster, uint64_t skillId);
	static void HandleChannelThink(entt::entity caster, uint64_t skillId);
	static void HandleChannelFinish(entt::entity caster, uint64_t skillId);

	static void HandleSkillToggleOn(entt::entity caster, uint64_t skillId);
	static void HandleSkillToggleOff(entt::entity caster, uint64_t skillId);

	static void HandleSkillActivate(entt::entity caster, uint64_t skillId);
	static void HandleSkillDeactivate(entt::entity caster, uint64_t skillId);

    // 验证目标
    static uint32_t ValidateTarget(const ::ReleaseSkillSkillRequest* request);

    // 检查冷却时间
    static uint32_t CheckCooldown(entt::entity caster, const SkillTable* tableSkill);

    // 处理施法计时器
    static uint32_t CheckCasting(entt::entity caster, const SkillTable* tableSkill);

	static uint32_t CheckRecovery(const entt::entity caster, const SkillTable* tableSkill);

	static uint32_t CheckChannel(const entt::entity caster, const SkillTable* tableSkill);

    // 广播技能使用消息
    static void BroadcastSkillUsedMessage(entt::entity caster, const ::ReleaseSkillSkillRequest* request);
	
    // 设置施法计时器
    static void SetupCastingTimer(entt::entity caster, const SkillTable* tableSkill, uint64_t skillId);

    // 发送技能中断消息
    static void SendSkillInterruptedMessage(entt::entity caster, const uint32_t skillTableId) ;

	static void TriggerSkillEffect(entt::entity caster, uint64_t skillId);

	static void RemoveEffect(entt::entity caster, uint64_t skillId);

	static void HandleSkillSpell(const entt::entity caster, const uint64_t skillId);
};