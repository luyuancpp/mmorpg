#pragma once

#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

#include "logic/client_player/player_ability.pb.h"
//https://zhuanlan.zhihu.com/p/149704315

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
};