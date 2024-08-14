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
	static void HandleAbilityStart(entt::entity caster);
	static void HandleAbilitySpell();
	static void HandleAbilityFinish();

	static void HandleChannelStart(entt::entity caster);
	static void HandleChannelThink();
	static void HandleChannelFinish();

	static void HandleAbilityToggleOn();
	static void HandleAbilityToggleOff();

	static void HandleAbilityActivate();
	static void HandleAbilityDeactivate();

	// 验证技能表
    static std::pair<const ability_row*, uint32_t> ValidateAbilityTable(uint32_t abilityId);

    // 验证目标
    static uint32_t ValidateTarget(const ::UseAbilityRequest* request);

    // 检查冷却时间
    static uint32_t CheckCooldown(entt::entity caster, const ability_row* tableAbility);

    // 处理施法计时器
    static uint32_t HandleCastingTimer(entt::entity caster, const ability_row* tableAbility);

    // 广播技能使用消息
    static void BroadcastAbilityUsedMessage(entt::entity caster, const ::UseAbilityRequest* request);
	
    // 设置施法计时器
    static void SetupCastingTimer(entt::entity caster, const ability_row* tableAbility, uint32_t abilityId);

    // 发送技能中断消息
    static void SendAbilityInterruptedMessage(entt::entity caster) ;
};