#pragma once

#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

#include "logic/client_player/player_ability.pb.h"
//https://zhuanlan.zhihu.com/p/149704315

class ability_row;

class AbilityUtil
{
public:
	static uint32_t CheckSkillActivationPrerequisites(entt::entity caster, const ::UseAbilityRequest* request);

	static bool IsAbilityOfType(uint32_t abilityId, uint32_t abilityType);

	static void HandleAbilityInitialize();
	static void HandleAbilitySpell(const entt::entity caster, uint32_t abilityId);
	static void HandleAbilityRecovery(const entt::entity caster, uint32_t abilityId);
	static void HandleAbilityFinish(const entt::entity caster, uint32_t abilityId);

	static void HandleChannelStart(entt::entity caster, uint32_t abilityId);
	static void HandleChannelThink(entt::entity caster, uint32_t abilityId);
	static void HandleChannelFinish(entt::entity caster, uint32_t abilityId);

	static void HandleAbilityToggleOn(entt::entity caster, uint32_t abilityId);
	static void HandleAbilityToggleOff(entt::entity caster, uint32_t abilityId);

	static void HandleAbilityActivate(entt::entity caster, uint32_t abilityId);
	static void HandleAbilityDeactivate(entt::entity caster, uint32_t abilityId);

	// 验证技能表
    static std::pair<const ability_row*, uint32_t> ValidateAbilityTable(uint32_t abilityId);

    // 验证目标
    static uint32_t ValidateTarget(const ::UseAbilityRequest* request);

    // 检查冷却时间
    static uint32_t CheckCooldown(entt::entity caster, const ability_row* tableAbility);

    // 处理施法计时器
    static uint32_t HandleCastingTimer(entt::entity caster, const ability_row* tableAbility);

	static uint32_t HandleRecoveryTimeTimer(const entt::entity caster, const ability_row* tableAbility);

	static uint32_t HandleChannelTimeTimer(const entt::entity caster, const ability_row* tableAbility);

    // 广播技能使用消息
    static void BroadcastAbilityUsedMessage(entt::entity caster, const ::UseAbilityRequest* request);
	
    // 设置施法计时器
    static void SetupCastingTimer(entt::entity caster, const ability_row* tableAbility, uint32_t abilityId);

    // 发送技能中断消息
    static void SendAbilityInterruptedMessage(entt::entity caster) ;

	static void TriggerSkillEffect(entt::entity caster, uint32_t abilityId);

	static void RemoveEffect(entt::entity caster, uint32_t abilityId);
};