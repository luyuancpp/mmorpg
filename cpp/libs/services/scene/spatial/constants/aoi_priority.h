#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// AOI interest priority — semantic tags, not hard ordinals.
// The actual ordering is determined by AoiPriorityPolicy per scene type.
// Reference: https://zhuanlan.zhihu.com/p/148077453
// ---------------------------------------------------------------------------
enum class AoiPriority : uint8_t
{
    kNormal      = 0,   // Default: entities discovered via spatial grid query
    kTeammate    = 1,   // Party/team members
    kAttacker    = 2,   // Entities currently attacking the observer
    kQuestNpc    = 3,   // NPCs related to active quests/missions
    kBoss        = 4,   // Boss / elite monsters
    kPinned      = 5,   // Manually pinned by buff/skill (e.g. eagle-eye, assassination target)

    kCount       = 6,   // Sentinel — always last
};

// ---------------------------------------------------------------------------
// Priority policy — maps each AoiPriority tag to a numeric weight.
// Higher weight = harder to evict. Scenes pick a policy at creation time.
// kPinned always gets max weight so it can never be evicted by spatial logic.
// ---------------------------------------------------------------------------
struct AoiPriorityPolicy
{
    // weight[static_cast<uint8_t>(tag)] → eviction weight
    std::array<uint8_t, static_cast<std::size_t>(AoiPriority::kCount)> weight{};

    [[nodiscard]] uint8_t GetWeight(AoiPriority p) const
    {
        return weight[static_cast<uint8_t>(p)];
    }
};

// ---- Built-in policies ----

// Open-world default: quest NPC > attacker > teammate > normal
inline constexpr AoiPriorityPolicy kPolicyOpenWorld {{
    /* kNormal   */ 0,
    /* kTeammate */ 1,
    /* kAttacker */ 2,
    /* kQuestNpc */ 3,
    /* kBoss     */ 4,
    /* kPinned   */ 255,
}};

// Dungeon / instance: boss > attacker > teammate > quest NPC > normal
inline constexpr AoiPriorityPolicy kPolicyDungeon {{
    /* kNormal   */ 0,
    /* kTeammate */ 2,
    /* kAttacker */ 3,
    /* kQuestNpc */ 1,
    /* kBoss     */ 4,
    /* kPinned   */ 255,
}};

// PvP arena: attacker > teammate > boss > quest NPC > normal
inline constexpr AoiPriorityPolicy kPolicyPvpArena {{
    /* kNormal   */ 0,
    /* kTeammate */ 2,
    /* kAttacker */ 4,
    /* kQuestNpc */ 1,
    /* kBoss     */ 3,
    /* kPinned   */ 255,
}};

// ---------------------------------------------------------------------------
// Dynamic AOI capacity — resolved per-entity per-frame.
// effectiveCapacity = min(clientReportedCapacity, serverPressureCapacity)
// ---------------------------------------------------------------------------
constexpr std::size_t kAoiListCapacityDefault = 100;
constexpr std::size_t kAoiListCapacityMin     = 20;
constexpr std::size_t kAoiListCapacityMax     = 200;
