#pragma once
#include <cstdint>

// No target: cast without a target (e.g. AoE heals, ground skills) -> 1 << 0
// Targeted: requires selecting a target (single-target skills) -> 1 << 1
// Area of effect: requires targeting a location -> 1 << 2
enum eTargetRequirement : uint32_t {
	kNoTargetRequired = 1 << 0,   // No target needed
	kTargetedSkill = 1 << 1,    // Single target required
	kAreaOfEffect = 1 << 2        // Location target
};

enum eSkillType : uint32_t {
	kPassiveSkill = 1 << 0,    // Passive skill
	kGeneralSkill = 1 << 1,    // General cast skill
	kChannelSkill = 1 << 2,    // Channeled skill
	kToggleSkill = 1 << 3,     // Toggle skill
	kActivateSkill = 1 << 4,    // Activation skill
	kBasicAttack   = 1 << 5  // Basic attack
};

