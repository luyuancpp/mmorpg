#pragma once

#include <algorithm>
#include <unordered_map>

#include "type_alias/actor.h"
#include "spatial/constants/aoi_priority.h"

struct AoiEntry
{
    AoiPriority priority = AoiPriority::kNormal;
};

// Per-entity interest list.
// Maps observed entities to their AoiEntry (priority metadata).
// Capacity is enforced by InterestSystem; when full, lowest-priority entries
// are evicted to make room for higher-priority ones.
struct AoiListComp
{
    std::unordered_map<entt::entity, AoiEntry> entries;

    // Convenience: flat set of entity keys (for backward-compat iteration).
    [[nodiscard]] EntityUnorderedSet GetEntitySet() const
    {
        EntityUnorderedSet result;
        result.reserve(entries.size());
        for (const auto& [e, _] : entries) { result.insert(e); }
        return result;
    }

    [[nodiscard]] bool Contains(entt::entity target) const { return entries.contains(target); }
    [[nodiscard]] std::size_t Size() const { return entries.size(); }
};

// ---------------------------------------------------------------------------
// Per-entity: client-reported display count (how many entities the client wants
// to see).  Updated when the client sends a settings RPC.
// ---------------------------------------------------------------------------
struct AoiClientCapacityComp
{
    std::size_t clientDesiredCount = kAoiListCapacityDefault;
};

// ---------------------------------------------------------------------------
// Per-scene: server pressure metric.
// pressureFactor in [0, 1]: 0 = idle, 1 = maximum pressure.
// GetServerCapacity() linearly interpolates between max and min capacity.
// ---------------------------------------------------------------------------
struct ScenePressureComp
{
    double pressureFactor = 0.0;

    [[nodiscard]] std::size_t GetServerCapacity() const
    {
        const auto range = static_cast<double>(kAoiListCapacityMax - kAoiListCapacityMin);
        const auto cap = static_cast<std::size_t>(kAoiListCapacityMax - pressureFactor * range);
        return std::clamp(cap, kAoiListCapacityMin, kAoiListCapacityMax);
    }
};

// ---------------------------------------------------------------------------
// Per-scene: active priority policy.  Defaults to open-world ordering.
// Set once at scene creation (e.g. dungeon vs arena vs open-world).
// ---------------------------------------------------------------------------
struct ScenePriorityPolicyComp
{
    const AoiPriorityPolicy* policy = &kPolicyOpenWorld;
};