#pragma once

#include <cstdint>

//https://zhuanlan.zhihu.com/p/149704315

class AbilityUtil
{
public:
	static uint32_t CheckSkillActivationPrerequisites();

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