# EnterScene Zone Routing Design (2026-04-17)

## Problem
`EnterSceneRequest` had `zone_id` and `gate_zone_id` fields but:
1. Login never passed `zone_id` → `resolveScene` looked up `world_channels:zone:0:xxx` → miss
2. `PlayerLocation` in Redis had no `zone_id` → no way to know what zone a player was last in
3. Scene creation didn't store `scene:{id}:zone` → no way to reverse-lookup a scene's zone

## Solution: Zone Resolution Chain
SceneManager determines `targetZoneId` via a priority-ordered fallback chain:
1. `in.ZoneId` — caller explicitly says "go to this zone" (cross-zone teleport, login)
2. `scene:{id}:zone` Redis lookup — when `sceneId != 0` (reconnect, follow-leader)
3. `PlayerLocation.zone_id` — player's last known zone (offline fallback)
4. `in.GateZoneId` — gate's zone (first-ever login, everything else is 0)

Cross-zone check: `GateZoneId != 0 && targetZoneId != 0 && GateZoneId != targetZoneId` → redirect.

## Changes Made
| Component | Change |
|-----------|--------|
| `storage.proto` / `PlayerLocation` | Added `zone_id` field 4 |
| `changesceneutil.go` | `UpdatePlayerLocation` takes `zoneId`; added `GetSceneZone()` |
| `world_init.go` | Writes `scene:{id}:zone` on world scene creation |
| `createscenelogic.go` | `allocateScene` takes `zoneId`, writes `scene:{id}:zone` |
| `enterscenelogic.go` | Zone resolution chain; `handleCrossZoneRedirect` takes `targetZoneId` |
| Login `entergamelogic.go` | Passes `ZoneId = config.Node.ZoneId` |
| C++ `player_scene.cpp` | Passes `zone_id = GetZoneId()` for follow-leader |

## Redis Keys
- `scene:{id}:zone` — uint32 zone_id for each scene instance
- `player:{id}:location` — protobuf `PlayerLocation` (scene_id, node_id, update_time, zone_id)
- `world_channels:zone:{zoneId}:{confId}` — SET of sceneId strings per zone/conf

## Cross-Zone Flow: Player in zone B → wants zone C scene
1. Client on zone B Gate → `EnterScene(ZoneId=C, GateZoneId=B)`
2. SceneManager: `targetZoneId=C`, `B != C` → cross-zone redirect
3. Returns `RedirectToGateInfo` with zone C gate address
4. Client disconnects B, connects C gate, re-logins
5. Login(C): `EnterScene(ZoneId=C, GateZoneId=C)` → same zone → normal entry

## Offline-Return Behavior
Player belongs to zone A, was in zone C, goes offline long time:
- Client reconnects → picks zone A gate → Login(A)
- Login passes `ZoneId=A` (from config) → SceneManager uses it directly
- Player enters zone A scene → **returns to home zone**
- This is intentional: cross-zone content (instances) may have been destroyed

## Merge-Server (合服)
`zone_id` is a **runtime routing identifier**, not a permanent identity:
- **Home zone (authoritative for data routing)**: run `tools/merge_zone` (or `DataService/RemapHomeZoneForMerge`) to rewrite `player:zone:*` in mapping Redis; see `docs/design/guild_ranking_architecture_zh.md` §合服工具.
- **Guild / 本服榜**: same tool updates MySQL `guild.zone_id` and merges `guild_rank:zone:{id}`.
- **SceneManager hot state** (`scene:{id}:zone`, `player:{id}:location`, world channel sets): normally cleared or left to refresh on next login after a maintenance window; coordinate with ops (stale keys can cause wrong routing until overwritten).
