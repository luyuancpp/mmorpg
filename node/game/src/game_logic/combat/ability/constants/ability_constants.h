#pragma once
#include <cstdint>

//技能释放时不需要目标即可释放（如群疗，踩地板技能） -> 1 << 1
//技能释放时需要选定目标（单体指向性技能） -> 1 << 2
//技能释放时需要以指定地点为目标（常用于AOE技能） -> 1 << 3
// Enum for target requirements
enum eTargetRequirement : uint32_t {
	kNoTargetRequired = 1 << 0,   // 不需要目标
	kTargetedAbility = 1 << 1,    // 选定目标
	kAreaOfEffect = 1 << 2        // 以指定地点为目标
};


// Enum for ability types
enum eAbilityType : uint32_t {
	kPassiveAbility = 1 << 0,    // 被动技能
	kGeneralAbility = 1 << 1,    // 普通施法技能
	kChannelAbility = 1 << 2,    // 持续施法技能
	kToggleAbility = 1 << 3,     // 开关类技能
	kActivateAbility = 1 << 4    // 激活类技能
};