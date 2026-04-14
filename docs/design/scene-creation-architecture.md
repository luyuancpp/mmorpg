# Scene Creation Architecture

**Updated:** 2026-04-14

## Overview

The scene system supports two scene types managed by Go SceneManager and hosted on C++ SceneNodes:
- **Main World** scenes: persistent, one per config ID per zone, created at startup.
- **Instance** scenes: on-demand, lifecycle-managed, auto-destroyed when idle.

### Zone-Agnostic Design

SceneManager is **stateless and horizontally scalable**. It does not bind to any specific zone:
- Zone information flows through **RPC requests** (`zone_id` field), not config.
- `LoadReporter` discovers scene nodes from **all zones** via etcd (`SceneNodeService.rpc/` prefix without zone filter), populating per-zone Redis load sets.
- `syncSceneNodes` detects new zones and **node re-appearance** via `prevSeenByZone`, triggers `initMainScenesForZone` to create/ensure main scenes.
- `InstanceLifecycleManager` iterates over `GetActiveZones()` when cleaning up idle instances.
- Multiple SceneManager instances can run behind a load balancer, each serving any zone.

## Scene Types

| Type | Constant | Behavior |
|------|----------|----------|
| Main World | `SceneTypeMainWorld=1` | Persistent. Created at startup via `InitMainScenes`. Idempotent (returns existing if already created). Never auto-destroyed. |
| Instance | `SceneTypeInstance=2` | On-demand. Unique per request. Tracked in Redis sorted set. Auto-destroyed after `InstanceIdleTimeoutSeconds` with 0 players. |

Auto-detection: if `scene_type` is unset in the request, the system checks `MainSceneConfIds` config. If the `scene_conf_id` is in that list → main world; otherwise → instance.

## Create Flow

```
Client/System → Go SceneManager.CreateScene(scene_conf_id)
│
├─ resolveSceneType (auto-detect or explicit)
│
├─ Main World path:
│  ├─ Check Redis hash → return existing (idempotent)
│  ├─ allocateScene → INCR scene:id_counter, SET scene:{id}:node
│  ├─ HSET main_scenes:zone:{zoneId} confId → sceneId
│  └─ RequestNodeCreateScene → gRPC → C++ Scene.CreateScene(config_id)
│
└─ Instance path:
   ├─ allocateScene → INCR scene:id_counter, SET scene:{id}:node
   ├─ ZADD instances:zone:{zoneId}:active (score = timestamp)
   ├─ SET instance:{id}:player_count = 0
   └─ RequestNodeCreateScene → gRPC → C++ Scene.CreateScene(config_id)
```

### C++ Side (Scene.CreateScene handler)

1. **Idempotency check**: scans `sceneRegistry.view<SceneInfoComp>()` for existing entity with same `config_id`. If found, returns existing info without creating — safe to call repeatedly.
2. `tlsEcs.sceneRegistry.create()` — creates entt entity (only if no duplicate).
3. Emplaces `SceneInfoComp` (config_id, guid) and `ScenePlayers` (empty set).
4. Fires `OnSceneCreated` event for downstream handlers (AOI, etc.).

## Destroy Flow

```
Go SceneManager.DestroyScene(scene_id) or InstanceLifecycleManager
│
├─ GET scene:{id}:node → resolve nodeId
├─ RequestNodeDestroyScene → gRPC → C++ Scene.DestroyScene(scene_id)
├─ DEL scene:{id}:node
├─ ZREM instances:zone:{zoneId}:active
├─ DEL instance:{id}:player_count
└─ INCRBY node:{nodeId}:scene_count -1
```

### C++ Side (Scene.DestroyScene handler)

1. Linear scan `sceneRegistry` for entity matching `scene_id`.
2. Fires `OnSceneDestroyed` event.
3. `sceneRegistry.destroy(entity)` — removes all components.

## Player Count Tracking

- `EnterScene` → `IncrInstancePlayerCount(sceneId)` — Redis INCR on `instance:{sceneId}:player_count`.
- `LeaveScene` → `DecrInstancePlayerCount(sceneId)` — Redis INCRBY -1, clamped to 0 (guards against double-leave or disconnect without leave).
- Instance lifecycle manager periodically scans active instances. If player count is 0 for longer than `InstanceIdleTimeoutSeconds`, the instance is destroyed.

## Instance Lifecycle Manager

- Runs as a goroutine started alongside the SceneManager.
- Polls every `InstanceCheckIntervalSeconds` (default 30s).
- For each active instance: if `player_count > 0`, resets idle clock (updates sorted set score to now). If `player_count == 0` and idle duration exceeds timeout, calls `destroyInstance`.
- `destroyInstance` notifies C++ node via `RequestNodeDestroyScene` before cleaning Redis state.

## gRPC Connection Cache (Go → C++ SceneNode)

- `scene_node_client.go` maintains a `nodeConnCache` (`sync.RWMutex`-guarded `map[string]*grpc.ClientConn`).
- Node endpoint discovered from etcd: `SceneNodeService.rpc/` prefix scan (all zones), JSON parsing of `sceneNodeRegistration.Endpoint`.
- `RemoveNodeConn(nodeId)` called by `load_reporter.go` when a node's grace period expires.

## Idempotent Convergence (Node Reconnect/Restart Safety)

Industry-standard pattern (same principle as K8s controller reconciliation loop). No per-node state machine required.

### Three-Layer Protection

1. **C++ CreateScene idempotent by `config_id`**: Before creating a new entity, scans `sceneRegistry.view<SceneInfoComp>()` for an existing entity with the same `scene_confid`. If found, returns the existing scene info. This makes the RPC safe to call any number of times.

2. **Go `initMainScenesForZone` dual-layer idempotency**:
   - Redis layer: `Hget` checks if `confId` already has a scene ID → skips ID allocation.
   - C++ layer: **always sends `CreateScene` RPC** regardless of Redis state (C++ deduplicates).
   - This handles the scenario where Redis has stale data (scene ID exists) but the C++ node restarted and its ECS registry is empty.

3. **Go `syncSceneNodes` lightweight re-appearance detection**:
   - `prevSeenByZone map[uint32]map[string]struct{}` stores the previous tick's node set (flat, replaced each tick — **not** a state machine).
   - On each sync: if a node appears that wasn't in the previous set → treated as new/re-appeared → triggers `initMainScenesForZone`.
   - Covers: first startup, node reconnect, node restart, SceneManager restart.

### Scenario Matrix

| Scenario | Redis State | C++ ECS State | Outcome |
|----------|-------------|---------------|----------|
| First startup | empty | empty | Allocate scene ID + create ECS entity |
| Node reconnect (data alive) | has data | has entity | RPC returns existing entity, zero side-effect |
| Node restart (data lost) | has data | empty | No re-allocation, RPC creates new entity |
| SceneManager restart | has data | has entity | `prevSeenByZone` empty → full re-ensure → C++ dedup |

## Main Scene Initialization

No blocking startup function. `syncSceneNodes` (called every 5s by `LoadReporter`) handles init:
1. Reads all `SceneNodeService.rpc/` entries from etcd, builds `seenByZone` map.
2. Compares with `prevSeenByZone`: any new/re-appeared node triggers `initMainScenesForZone`.
3. `initMainScenesForZone`: iterates `MainSceneConfIds`, performs Redis idempotent check (skip ID allocation if exists), always sends C++ CreateScene RPC, assigns nodes via FNV-1a consistent hashing.

## Key Redis Keys

| Key | Type | Purpose |
|-----|------|---------|
| `scene:id_counter` | String (int) | Auto-increment scene ID |
| `scene:{id}:node` | String | Scene → node mapping |
| `main_scenes:zone:{zoneId}` | Hash | confId → sceneId for main worlds |
| `instances:zone:{zoneId}:active` | ZSet | Active instances (score = create/last-active timestamp) |
| `instance:{id}:player_count` | String (int) | Player count per instance |
| `scene_nodes:zone:{zoneId}:load` | ZSet | Node load balancing (score = scene count) |
| `node:{nodeId}:scene_count` | String (int) | Node's hosted scene count |

## Configuration

In `etc/scene_manager_service.yaml`:

```yaml
MainSceneConfIds:         # List of scene_conf_ids that are main world scenes
  - 1001
  - 1002
InstanceIdleTimeoutSeconds: 300   # Auto-destroy idle instances after this (0 = disabled)
InstanceCheckIntervalSeconds: 30  # How often to check for idle instances
```

## File Index

| File | Purpose |
|------|---------|
| `go/scene_manager/internal/logic/createscenelogic.go` | CreateScene routing: main world vs instance |
| `go/scene_manager/internal/logic/main_scene_init.go` | Startup init of persistent main world scenes |
| `go/scene_manager/internal/logic/instance_lifecycle.go` | Auto-destroy idle instances + player count helpers |
| `go/scene_manager/internal/logic/scene_node_client.go` | gRPC client cache + RequestNodeCreateScene/DestroyScene |
| `go/scene_manager/internal/logic/enterscenelogic.go` | EnterScene: location update, gate routing, player count |
| `go/scene_manager/internal/logic/leavescenelogic.go` | LeaveScene: location cleanup, player count decrement |
| `go/scene_manager/internal/logic/destroyscenelogic.go` | Admin DestroyScene RPC handler |
| `go/scene_manager/internal/logic/load_reporter.go` | etcd → Redis node discovery, load sync, grace period |
| `proto/scene/scene.proto` | C++ Scene service (CreateScene, DestroyScene) |
| `proto/scene_manager/scene_manager_service.proto` | Go SceneManager service definition |
| `cpp/nodes/scene/handler/rpc/scene_handler.cpp` | C++ CreateScene/DestroyScene ECS handlers |
