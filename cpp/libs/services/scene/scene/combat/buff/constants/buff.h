#pragma once

enum eBuffType {
    // Control buffs
    kBuffTypeMovementSpeedReduction = 0,           // Movement speed reduction
    kBuffTypeAttackSpeedSlow = 1,                  // Attack speed slow
    kBuffTypeCastSpeedSlow = 2,                    // Cast speed slow
    kBuffTypeGlobalSlow = 3,                       // Global slow (movement, attack, cast)

    // Stat boost buffs
    kBuffTypeIncreaseAttack = 10,                  // Increase attack power
    kBuffTypeIncreaseDefense = 11,                 // Increase defense
    kBuffTypeIncreaseHealth = 12,                  // Increase max health
    kBuffTypeMovementSpeedBoost = 13,              // Increase movement speed
    kBuffTypeIncreaseCriticalChance = 14,          // Increase critical chance

    // Defensive buffs
    kBuffTypeDamageReduction = 20,                 // Reduce damage taken
    kBuffTypeShield = 21,                          // Shield (absorbs damage)

    // Special buffs
    kBuffTypeStun = 30,                            // Stun, unable to act
    kBuffTypeSilence = 31,                         // Silence, unable to cast skills
    kBuffTypeInvincibility = 32,                   // Invincible, immune to all damage
    kBuffTypeStealth = 33,                         // Stealth, invisible to enemies
    kBuffTypeImmunity = 34,                        // Buff immunity
    kBuffTypeDispel = 35,                          // Dispel buffs/debuffs
    kBuffTypeNextBasicAttack = 36,                 // Next basic attack has extra effect

    // Heal-over-time buffs
    kBuffTypeHealthRegeneration = 40,              // Health regeneration over time
    kBuffTypeManaRegeneration = 41,                // Mana regeneration over time
    kBuffTypeHealthRegenerationBasedOnLostHealth = 42, // Health regen based on lost HP
    kBuffTypeNoDamageOrSkillHitInLastSeconds = 43, // No damage or skill hit in last N seconds

    // Debuffs
    kBuffTypePoison = 50,                          // Poison, DoT health loss
    kBuffTypeBurn = 51,                            // Burn, fire DoT
    kBuffTypeFreeze = 52,                          // Freeze, unable to act
};
