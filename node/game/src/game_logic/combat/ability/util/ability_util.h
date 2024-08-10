#pragma once

#include <cstdint>
#include "logic/client_player/player_ability.pb.h"
//https://zhuanlan.zhihu.com/p/149704315

class AbilityUtil
{
public:
	static uint32_t CheckSkillActivationPrerequisites(const ::UseAbilityRequest* request);

	void HandleAbilityInitialize();
	void HandleAbilityStart();
	void HandleAbilitySpell();
	void HandleAbilityFinish();

	void HandleChannelStart();
	void HandleChannelThink();
	void HandleChannelFinish();

	void HandleAbilityToggleOn();
	void HandleAbilityToggleOff();

	void HandleAbilityActivate();
	void HandleAbilityDeactivate();
};