#pragma once

#include <cstdint>

// 属性计算器枚举，用于标识不同的属性计算器类型
enum eAttributeCalculator : uint32_t {
    kVelocity,                // 速度计算器
    kHealth,                  // 生命值计算器
    kEnergy,                  // 能量计算器
    kDamage,                  // 伤害计算器
    kStatusEffect,            // 状态效果计算器
    kCombatState,             //战斗状态
    kAttributeCalculatorMax   // 最大计算器数量，方便遍历
};