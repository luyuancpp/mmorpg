# ECS Component Access Rules

## Overview

entt uses a **sparse-set** storage model. Each component type has its own packed array. Entities that do not have a component use **zero memory** in that pool and are never visited by `view<T>` iteration. This is a core design advantage for instances/dungeons where entities intentionally omit components (AI, bag, buff, skills) to save memory and CPU.

## Three Access Patterns

| API | Behavior | Use When |
|-----|----------|----------|
| `get<T>(entity)` | Asserts existence; **crashes** if absent | Inside `view<T,...>` iteration, or after `any_of<T>` / `all_of<T>` guard |
| `try_get<T>(entity)` | Returns `nullptr` when absent | Cross-entity lookups, optional components, any entity whose archetype is not guaranteed |
| `get_or_emplace<T>(entity)` | Silently **creates default** if absent | **Only** during entity initialization/setup (e.g. `InitPlayerFromAllData`, event handlers) |

## `get_or_emplace` Anti-Pattern

`get_or_emplace<T>` in per-tick, per-pair, combat, spatial, or attribute sync code paths is an **anti-pattern**:

1. It silently creates a **zero-valued default component** on any entity that doesn't already have one.
2. This **masks missing initialization** bugs — the entity appears to have the component, but with garbage/zero data.
3. It **corrupts ECS state** — entities that should not participate in a system now have the component and will be iterated.
4. It **defeats the sparse-set memory advantage** — components grow for entities that should not have them.

### Acceptable uses of `get_or_emplace`

- `player_database_loader.cpp` — marshaling player data for DB save (read-only fallback to default)
- `player_lifecycle.cpp` L333-334 — first-time player init (setting registration timestamp, level=1)
- `mission_event_handler.cpp` — `InitializePlayerCompsEvent` handler creating initial mission state
- `player_scene.cpp` L190 — `SceneEntityComp` during `EnterScene` (binding player to scene)
- `globalRegistry.get_or_emplace<TipInfoMessage>` — reusable per-frame message buffer on global entity
- `nodeGlobalRegistry.get_or_emplace<ServiceNodeList>` — singleton node registry

### Problematic uses (should be reviewed/refactored)

| File | Line | Component | Issue |
|------|------|-----------|-------|
| `skill.cpp` | 57 | `Transform` | Creates zero-position Transform on target if missing |
| `skill.cpp` | 43, 355 | `CooldownTimeListComp` | Per-skill-cast; should be initialized at actor creation |
| `skill.cpp` | 68, 73, 78, 214, 236, 241, 422, 469, 562 | `SkillContextCompMap` | Per-cast; should exist if entity can use skills |
| `skill.cpp` | 142, 441, 447, 451, 544 | `BaseAttributesComp` / `CombatStateCollectionComp` | Combat hot path; should be initialized at actor creation |
| `skill.cpp` | 223, 261, 266, 337, 396 | Timer comps | Per-cast; should be initialized at actor creation |
| `skill.cpp` | 491 | `LevelComp` | Combat formula; should exist on any combatant |
| `buff.cpp` | 59, 74, 113, 167, 188, 209, 228, 317 | `BuffListComp` | Should exist on any entity that can receive buffs |
| `modifier_buff_impl.cpp` | 56-58 | `BaseAttributesComp` / `DerivedAttributesComp` / `LevelComp` | Modifier calculation; should exist on combatants |
| `actor_attribute_calculator.cpp` | 17, 20, 45, 46, 64 | `Velocity` / `BuffListComp` / various | Per-tick attribute recalc |
| `actor_action_state.cpp` | 54, 74, 83, 92, 108 | `ActorStateComp` | State queries; should exist if entity has action states |
| `combat_state.cpp` | 17, 40, 62 | `CombatStateCollectionComp` | Combat state management |
| `aoi.cpp` | 192 | `SceneEntityComp` | Should exist on any entity in a scene |
| `interest.cpp` | 87 | `AoiListComp` | AOI watcher registration |
| `grid.cpp` / `aoi.cpp` | multiple | `SceneGridListComp` | Scene-level grid; lazy init may be acceptable |
| `scene_handler.cpp` | 274 | `LastActiveFrameComp` | Should be emplaced at player creation |
| `scene_handler.cpp` | 541 | `PlayerSessionSnapshotComp` | Session binding; could be init-time |
| `generated/attribute/*.cpp` | multiple | `*DirtyMaskComp` | Generated code; dirty mask lazy init (may be acceptable) |

## `std::optional` Return Pattern

Functions that depend on optional components should return `std::optional<T>`:

```cpp
// GOOD: caller decides how to handle missing Transform
std::optional<double> GetDistanceBetweenEntities(entt::entity e1, entt::entity e2)
{
    const auto *t1 = registry.try_get<Transform>(e1);
    const auto *t2 = registry.try_get<Transform>(e2);
    if (!t1 || !t2) return std::nullopt;
    return ComputeDistance(*t1, *t2);
}

// BAD: crashes if either entity lacks Transform
double GetDistanceBetweenEntities(entt::entity e1, entt::entity e2)
{
    const auto &t1 = registry.get<Transform>(e1); // assert crash
    const auto &t2 = registry.get<Transform>(e2); // assert crash
    return ComputeDistance(t1, t2);
}
```

## Dungeon/Instance Memory Savings

Entities that intentionally omit components:
- Use **zero memory** in those component pools (sparse-set design)
- Are **never iterated** by `view<T>` — zero CPU cost
- Keep **packed arrays dense** — better cache line utilization for entities that do have the component

Example: 200 NPC桩体 in a dungeon with only `Transform` + `Npc` tag, no AI/bag/buff/skills → each saves ~10 components × dozens to hundreds of bytes each.
