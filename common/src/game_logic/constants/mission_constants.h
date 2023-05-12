#pragma once

#include <cstdint>

enum eCondtionType : uint32_t
{
	kConditionKillMonster = 1,//杀怪
	kConditionTalkWithNpc = 2,//对话
	kConditionCompleteCondition = 3,//完成相应条件
	kConditionUseItem = 4,//使用物品
	kConditionInteraction = 5,//交互
	kConditionLevelUp = 6,//升级
	kConditionCustom = 7,//自定义条件
	kConditionCompleteMission = 8,//完成任务
	kConditionTypeMax,
};
