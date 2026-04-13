# AOI Priority, Capacity & Visibility Design

**Reference**: [如何实现一个强大的MMO技能系统——AOI (kasan)](https://zhuanlan.zhihu.com/p/148077453)

## Core Principle

> The grid is just a means; the interest list is the result.

AOI spatial queries (hex grid neighbors) populate the **interest list**, but the
interest list can also be manipulated directly by skills, buffs, and other
game systems. The client displays whatever is in the interest list — not
the raw grid contents.

## Priority Tags & Policy-Based Weighting

Priority is split into two layers:
1. **Semantic tags** (`AoiPriority` enum) — what category an entity belongs to.
2. **Priority policy** (`AoiPriorityPolicy`) — scene-specific weight table
   that assigns an eviction weight to each tag.

### Tags

| Tag          | Value | Source |
|-------------|-------|--------|
| `kNormal`   | 0 | Default: entities discovered via spatial grid query |
| `kTeammate` | 1 | Same `TeamId` component |
| `kAttacker` | 2 | Combat/threat system (TODO: integrate) |
| `kQuestNpc` | 3 | Quest/mission system marks NPC (TODO: integrate) |
| `kBoss`     | 4 | Boss / elite monster |
| `kPinned`   | 5 | Manually pinned by buff/skill (e.g. eagle-eye, assassination target) |

### Built-in Policies

| Policy | Weight order (low → high) |
|--------|--------------------------|
| `kPolicyOpenWorld` | normal(0) < teammate(1) < attacker(2) < questNpc(3) < boss(4) < pinned(255) |
| `kPolicyDungeon`   | normal(0) < questNpc(1) < teammate(2) < attacker(3) < boss(4) < pinned(255) |
| `kPolicyPvpArena`  | normal(0) < questNpc(1) < teammate(2) < boss(3) < attacker(4) < pinned(255) |

Scenes attach `ScenePriorityPolicyComp` at creation to pick a policy.
`kPinned` always gets weight 255 across all policies — it can **never** be evicted
by spatial logic.

### Eviction Rule

When the list is full, `InterestSystem::AddAoiEntity` compares the **policy
weight** of the new entity against the lowest-weight existing entry:
- New entity's weight > lowest entry's weight → evict lowest, insert new.
- Otherwise → reject the new entity.

## Dynamic AOI Capacity

Capacity is no longer a compile-time constant. It is computed per-entity
per-frame:

```
effectiveCapacity = min(clientReportedCapacity, serverPressureCapacity)
```

### Components

| Component | Scope | Purpose |
|-----------|-------|---------|
| `AoiClientCapacityComp` | Per-entity (actor) | Client-reported desired display count |
| `ScenePressureComp` | Per-scene | Server pressure factor → derived capacity ceiling |

### Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `kAoiListCapacityDefault` | 100 | Fallback when no capacity components exist |
| `kAoiListCapacityMin` | 20 | Floor — never go below this even under max pressure |
| `kAoiListCapacityMax` | 200 | Ceiling — client can never request more |

### Server Pressure

`ScenePressureComp.pressureFactor` ∈ [0, 1]:
- 0 = idle → server capacity = `kAoiListCapacityMax` (200)
- 1 = max pressure → server capacity = `kAoiListCapacityMin` (20)
- Linear interpolation between the two.

The scene's tick/load monitor sets `pressureFactor` each frame (integration
TODO).

### Client Reported Count

The client sends a desired display count via a settings RPC. The server
stores it in `AoiClientCapacityComp.clientDesiredCount` and clamps it to
[`kAoiListCapacityMin`, `kAoiListCapacityMax`].

### Resolution

`InterestSystem::GetEffectiveCapacity(watcher)` returns
`min(clamped_client, server_pressure_cap)`.

## Pin / Unpin (Buff → AOI Bridge)

```
InterestSystem::PinAoiEntity(watcher, target)   // Adds at kPinned
InterestSystem::UnpinAoiEntity(watcher, target)  // Downgrades to kNormal
```

**Example (assassination quest)**:
1. Player uses eagle-eye skill → buff created.
2. `OnBuffStart` calls `InterestSystem::PinAoiEntity(player, questTarget)`.
3. Target appears on client even though it's far outside grid range.
4. Quest complete / buff expires → `OnBuffRemove` calls `InterestSystem::UnpinAoiEntity(player, questTarget)`.
5. Next `AoiSystem::Update` evicts the target if it's still out of range.

Pinned entries are protected from spatial grid leave/enter eviction.

## Stealth Visibility

`ViewSystem::CanSee(observer, target)` replaces raw `IsWithinViewRadius` in
AOI enter checks:

1. Distance check (as before).
2. If target has `kBuffTypeStealth` buff → invisible **unless** observer has
   the target pinned (`AoiPriority::kPinned`) in its interest list.

This means a "detect stealth" skill simply pins the stealthed target.

## Directionality

Interest is **unidirectional**: A seeing B does not require B to see A. Each
direction is checked independently during `HandleEntityVisibility`. This
enables asymmetric scenarios (different view radii, stealth, capacity limits).

## Key APIs

| Method | Description |
|--------|-------------|
| `InterestSystem::AddAoiEntity(watcher, target, priority)` | Uses policy weights for comparison, dynamic capacity for limit |
| `InterestSystem::RemoveAoiEntity(watcher, target)` | Remove target from watcher's interest list |
| `InterestSystem::PinAoiEntity(watcher, target)` | Add at kPinned priority |
| `InterestSystem::UnpinAoiEntity(watcher, target)` | Unpin, downgrade to kNormal |
| `InterestSystem::UpgradePriority(watcher, target, priority)` | Upgrade priority (compares policy weights, not raw enum values) |
| `InterestSystem::GetEffectiveCapacity(watcher)` | Dynamically resolve effective capacity |
| `InterestSystem::GetPriorityPolicy(watcher)` | Get scene's policy (default: open-world) |
| `ViewSystem::CanSee(observer, target)` | Full visibility check: distance + stealth state |
| `ViewSystem::IsStealthed(entity)` | Check for active stealth buff |

## Files Changed

| File | Change |
|------|--------|
| `spatial/constants/aoi_priority.h` | Priority tags, `AoiPriorityPolicy` struct, built-in policies, dynamic capacity constants |
| `spatial/comp/scene_node_scene_comp.h` | `AoiListComp` (entries map), `AoiClientCapacityComp`, `ScenePressureComp`, `ScenePriorityPolicyComp` |
| `spatial/system/interest.h/.cpp` | Policy-aware `AddAoiEntity`, `GetEffectiveCapacity`, `GetPriorityPolicy`, `UpgradePriority` |
| `spatial/system/view.h/.cpp` | `CanSee()`, `IsStealthed()` |
| `spatial/system/aoi.cpp` | Uses `CanSee`, `DetermineAoiPriority` (semantic tags), pinned check via `==` |
| `actor/attribute/system/actor_state_attribute_sync.cpp` | Updated for `entries` API |
| `tests/aoi_test/*` | Priority, capacity, pin/team, dynamic capacity, policy tests |

## Future Work

- **Attacker priority**: Hook combat/damage system to call
  `InterestSystem::UpgradePriority(target, attacker, kAttacker)` on hit.
- **Quest NPC priority**: Hook mission accept to call
  `InterestSystem::UpgradePriority(player, npc, kQuestNpc)`.
- **Boss tagging**: Scene creation marks boss entities; grid entry detects and
  assigns `kBoss` tag.
- **Pressure integration**: Scene tick monitors entity count / CPU load and
  writes `ScenePressureComp.pressureFactor`.
- **Client settings RPC**: Wire client-reported display count to
  `AoiClientCapacityComp`.
- **Hysteresis / grace period**: Add a grace period before removing entities
  that oscillate across grid boundaries.
- **Custom policies**: Data-driven policy loading from configuration tables.
