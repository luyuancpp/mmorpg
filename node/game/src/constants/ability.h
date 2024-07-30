#pragma once
#include <cstdint>

//技能释放时不需要目标即可释放（如群疗，踩地板技能） -> 1 << 1
//技能释放时需要选定目标（单体指向性技能） -> 1 << 2
//技能释放时需要以指定地点为目标（常用于AOE技能） -> 1 << 3
constexpr uint32_t kAbilityNoTarget = 1 << 1;       // Skills that can be cast without targeting (e.g., group healing, ground-targeted skills)
constexpr uint32_t kAbilitySingleTarget = 1 << 2;   // Skills that require selecting a single target (e.g., single target healing, single target attack)
constexpr uint32_t kAbilityLocationTarget = 1 << 3;  // Skills that require selecting a location as the target (e.g., AOE skills)