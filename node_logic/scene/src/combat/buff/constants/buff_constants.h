#pragma once

enum eBuffType {
    // 控制类 Buff
    kBuffTypeMovementSpeedReduction = 0,           // 移动速度减速
    kBuffTypeAttackSpeedSlow = 1,                  // 攻击速度减速
    kBuffTypeCastSpeedSlow = 2,                    // 技能施放速度减速
    kBuffTypeGlobalSlow = 3,                       // 全局减速（移动、攻击、施法等都减速）

    // 属性增强类 Buff
    kBuffTypeIncreaseAttack = 10,                  // 增加攻击力
    kBuffTypeIncreaseDefense = 11,                 // 增加防御力
    kBuffTypeIncreaseHealth = 12,                  // 增加最大生命值
    kBuffTypeMovementSpeedBoost = 13,              // 提高移动速度
    kBuffTypeIncreaseCriticalChance = 14,          // 提高暴击率

    // 防御类 Buff
    kBuffTypeDamageReduction = 20,                 // 减少受到的伤害
    kBuffTypeShield = 21,                          // 获得护盾（吸收伤害）

    // 特殊类 Buff
    kBuffTypeStun = 30,                            // 晕眩，无法行动
    kBuffTypeSilence = 31,                         // 沉默，无法施放技能
    kBuffTypeInvincibility = 32,                   // 无敌，免疫所有伤害
    kBuffTypeStealth = 33,                         // 隐身，无法被敌人发现
    kBuffTypeImmunity = 34,                        // 免疫buff
    kBuffTypeDispel = 35,                          // 驱散，移除buff或debuff
    kBuffTypeNextBasicAttack = 36,                 // 下一次普攻，造成额外效果

    // 持续恢复类 Buff
    kBuffTypeHealthRegeneration = 40,              // 持续恢复生命值
    kBuffTypeManaRegeneration = 41,                // 持续恢复法力值
    kBuffTypeHealthRegenerationBasedOnLostHealth = 42, // 根据已损失生命值的每秒回复
    kBuffTypeNoDamageOrSkillHitInLastSeconds = 43, // 若在过去s秒内，没有受到伤害或被技能命中

    // Debuff 类
    kBuffTypePoison = 50,                          // 中毒，持续扣除生命值
    kBuffTypeBurn = 51,                            // 燃烧，持续受到火焰伤害
    kBuffTypeFreeze = 52,                          // 冰冻，无法行动
};
