#pragma once

#include <entt/src/entt/entity/entity.hpp>
#include "spatial/constants/aoi_priority.h"

class InterestSystem {
public:
    // Add target to watcher's interest list with the given priority.
    // Returns true if the target was inserted (or upgraded); false if capacity is
    // full and the target's priority is too low to evict anyone.
    static bool AddAoiEntity(entt::entity watcher, entt::entity target,
                             AoiPriority priority = AoiPriority::kNormal);

    // Remove target from watcher's interest list.
    static void RemoveAoiEntity(entt::entity watcher, entt::entity target);

    // Pin a remote entity into the watcher's interest list (buff/skill use).
    // Pinned entries survive spatial grid changes; they can only be removed by
    // an explicit UnpinAoiEntity or RemoveAoiEntity call.
    static bool PinAoiEntity(entt::entity watcher, entt::entity target);

    // Unpin a previously pinned entity (reverts priority to kNormal if still in
    // range, or removes it if out of range).
    static void UnpinAoiEntity(entt::entity watcher, entt::entity target);

    // Upgrade the priority of an existing entry (no-op if the target is absent
    // or already at a higher weight under the active policy).
    static void UpgradePriority(entt::entity watcher, entt::entity target, AoiPriority priority);

    // Effective AOI list capacity for |watcher|:
    //   min(client-reported desired count, server pressure-adjusted ceiling).
    // Falls back to kAoiListCapacityDefault when components are absent.
    static std::size_t GetEffectiveCapacity(entt::entity watcher);

    // Resolve the priority policy active for |watcher|'s scene.
    // Returns kPolicyOpenWorld when the scene has no explicit policy.
    static const AoiPriorityPolicy& GetPriorityPolicy(entt::entity watcher);
};

