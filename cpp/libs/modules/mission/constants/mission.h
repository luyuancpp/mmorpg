#pragma once

#include <cstdint>

enum class eConditionType : uint32_t
{
	kConditionKillMonster = 1,//kill monster	
	kConditionTalkWithNpc = 2, //talk with NPC
	kConditionCompleteCondition = 3,//complete specified condition
	kConditionUseItem = 4,//use item
	kConditionInteraction = 5,//interact
	kConditionLevelUp = 6,//level up
	kConditionCustom = 7,//custom condition
	kConditionCompleteMission = 8,
	kConditionTypeMax,
};
