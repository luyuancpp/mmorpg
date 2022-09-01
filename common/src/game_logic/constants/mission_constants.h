#pragma once

#include <cstdint>

enum eCondtionType : uint32_t
{
	kConditionKillMonster = 1,//杀怪
	kConditionTalkWithNpc,//对话
	kConditionCompleteCondition,//完成相应条件
	kConditionUseItem,//使用物品
	kConditionInteration,//交互
	kConditionLevelUp,//升级
	kConditionCustom,//自定义条件
	kConditionCompleteMission,//完成任务
	kConditionTypeMax,
};
