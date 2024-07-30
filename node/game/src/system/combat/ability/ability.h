#pragma once

//https://zhuanlan.zhihu.com/p/149704315

class AbilitySystem
{
public:
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