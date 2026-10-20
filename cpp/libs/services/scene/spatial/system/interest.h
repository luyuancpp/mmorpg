#pragma once

#include <entt/src/entt/entity/entity.hpp>
#include "spatial/constants/aoi_priority.h"

class InterestSystem {
public:
    // Add target to watcher's interest list with the given priority.
    // Returns true if the target was inserted (or upgraded); false if capacity is
    // full and the target's priority is too low to evict anyone.
    // 前置条件:watcher 与 target 都是活着的 actor;无效句柄被拒绝且不会创建组件。
    // AoiListComp 是按需状态,本函数是唯一创建入口,第一次实际插入时创建。
    // 不要求观察者预先入格;只读查询拿不到表表示没有观察对象。
    static bool AddAoiEntity(entt::entity watcher, entt::entity target,
                             AoiPriority priority = AoiPriority::kNormal);

    // Remove target from watcher's interest list.
    static void RemoveAoiEntity(entt::entity watcher, entt::entity target);

    // Upgrade the priority of an existing entry (no-op if the target is absent
    // or already at a higher weight under the active policy).
    static void UpgradePriority(entt::entity watcher, entt::entity target, AoiPriority priority);

    // 条目当前优先级恰好等于 |from| 时才改成 |to|;条目不存在或不是 |from| 时静默返回。
    // 专供"语义标签失效"的回退使用(例:离队后 kTeammate -> kNormal,team-system.md §F.4)。
    // 按标签精确匹配而不是按权重比较,任何 from != kPinned 的调用都不会碰预留的锁定标签。
    static void DowngradePriority(entt::entity watcher, entt::entity target, AoiPriority from, AoiPriority to);

    // Effective AOI list capacity for |watcher|:
    //   min(client-reported desired count, server pressure-adjusted ceiling).
    // Falls back to kAoiListCapacityDefault when components are absent.
    static std::size_t GetEffectiveCapacity(entt::entity watcher);

    // Resolve the priority policy active for |watcher|'s scene.
    // Returns kPolicyOpenWorld when the scene has no explicit policy.
    static const AoiPriorityPolicy& GetPriorityPolicy(entt::entity watcher);
};

