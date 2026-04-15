# Scene Creation Architecture

**Updated:** 2026-04-14

## Overview

The scene system supports two scene types managed by Go SceneManager and hosted on C++ SceneNodes:
- **Main World** scenes: persistent, one per config ID per zone, created at startup.
- **Instance** scenes: on-demand, lifecycle-managed, auto-destroyed when idle.

### Zone-Agnostic Design

SceneManager is **stateless and horizontally scalable**. It does not bind to any specific zone:
- Zone information flows through **RPC requests** (`zone_id` field), not config.
- `LoadReporter` uses **list-watch** (etcd Watch) to reactively discover scene node changes. No polling.
- On Watch PUT (node appeared) → triggers `initWorldScenesForZone`. On Watch DELETE → grace period then cleanup.
- `fullSync` on startup / watch reconnect does full etcd Get, rebuilds `knownNodes` baseline.
- `InstanceLifecycleManager` iterates over `GetActiveZones()` when cleaning up idle instances.
- Multiple SceneManager instances can run behind a load balancer, each serving any zone.

## Scene Types

| Type | Constant | Behavior |
|------|----------|----------|
| Main World | `SceneTypeMainWorld=1` | Persistent. Created at startup via `InitWorldScenes`. Idempotent (returns existing if already created). Never auto-destroyed. |
| Instance | `SceneTypeInstance=2` | On-demand. Unique per request. Tracked in Redis sorted set. Auto-destroyed after `InstanceIdleTimeoutSeconds` with 0 players. |

Auto-detection: if `scene_type` is unset in the request, the system checks `WorldConfIds` config. If the `scene_conf_id` is in that list → main world; otherwise → instance.

## Create Flow

```
Client/System → Go SceneManager.CreateScene(scene_conf_id)
│
├─ resolveSceneType (auto-detect or explicit)
│
├─ Main World path:
│  ├─ Check Redis hash → return existing (idempotent)
│  ├─ allocateScene → INCR scene:id_counter, SET scene:{id}:node
│  ├─ HSET world_channels:zone:{zoneId} confId → sceneId
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

### K8s 设计模式在本项目中的映射

#### 1. List-Watch（K8s Informer 模式）

**K8s 原理**：K8s 的 Informer 不是每秒轮询 API Server，而是：
- **List**：启动时执行一次全量 GET，拉取所有资源，建立本地缓存（indexer），记录 `resourceVersion`。
- **Watch**：从 `resourceVersion+1` 开始监听变更事件流（ADDED / MODIFIED / DELETED），增量更新本地缓存。
- **Re-list**：Watch 连接断开时，重新 List 建立基线，再重新 Watch。
- 优势：全量 + 增量结合，不丢事件，不做无谓轮询，99% 的时间只处理增量。

**本项目映射**（`load_reporter.go`）：

| K8s Informer 概念 | 本项目实现 |
|-------------------|-----------|
| List（全量拉取） | `fullSync()` — `etcd.Get(prefix, WithPrefix())` 拉取所有 `SceneNodeService.rpc/*` |
| Local cache / indexer | `knownNodes map[string]nodeEntry` — 按 etcd key 索引的节点信息 |
| resourceVersion | `resp.Header.Revision` — etcd 全局递增版本号 |
| Watch（增量监听） | `watchAndRefresh()` — `etcd.Watch(prefix, WithRev(rev+1))` 接收 PUT/DELETE |
| Event handler | `handleWatchEvent()` — 分发 PUT（新增/更新）和 DELETE（删除）事件 |
| Re-list on watch error | Watch channel 关闭或报错 → 回到外层 for 循环 → 重新 `fullSync()` |

```
StartLoadReporter (外层 for 循环)
  │
  ├─ fullSync()                          ← K8s List
  │   ├─ etcd Get 全量拉取
  │   ├─ 建立 knownNodes (= indexer)
  │   ├─ 清理 Redis 中的陈旧节点
  │   ├─ 对所有 zone 执行 initWorldScenesForZone (= 全量 reconcile)
  │   └─ 返回 revision
  │
  └─ watchAndRefresh(rev)                ← K8s Watch
      ├─ etcd.Watch(prefix, WithRev(rev+1))
      ├─ SELECT:
      │   ├─ watchCh event
      │   │   └─ handleWatchEvent()      ← Event handler (增量 reconcile)
      │   │       ├─ PUT  → 更新 knownNodes, 更新 Redis load, 触发 initWorldScenesForZone
      │   │       └─ DELETE → 从 knownNodes 移除, 启动 grace period
      │   └─ 5s ticker
      │       ├─ refreshLoadScores()     ← 定期刷新负载分数
      │       └─ checkGracePeriodExpirations()
      └─ watch 断开 → return → 外层 for 自动 re-list
```

#### 2. Reconciliation Loop（K8s Controller 模式）

**K8s 原理**：Controller 不记录"我做过什么操作"的步骤状态机，而是：
- 声明 **期望状态**（Desired State）：例如 Deployment spec 声明要 3 个 Pod。
- 观察 **当前状态**（Current State）：查看实际 Pod 数量。
- **收敛**（Reconcile）：计算 diff，执行动作让 current → desired。
- 关键特性：**幂等**。执行多次结果一样。不管中间崩溃多少次，下次运行时重新 observe + reconcile 即可自愈。

**本项目映射**：

| K8s Controller 概念 | 本项目实现 |
|--------------------|-----------|
| Desired State | `WorldConfIds` 配置："每个 zone 应该有这些主场景" |
| Current State | Redis `world_channels:zone:{zoneId}` 哈希 + C++ ECS registry |
| Reconcile 函数 | `initWorldScenesForZone()` — 对比 + 收敛 |
| Reconcile 触发方式 | Watch PUT event（增量）或 fullSync（全量） |
| 幂等保证 | Redis 层跳过已有 ID + C++ CreateScene 按 config_id 去重 |

```
Reconcile (initWorldScenesForZone):

  for each confId in WorldConfIds:
      ┌─ Redis Hget(world_channels:zone:X, confId)
      │
      ├─ 不存在 → 分配 scene ID (INCR), 写 Redis (HSET)
      │           └─ 调 C++ CreateScene RPC → 创建 ECS entity
      │
      └─ 已存在 → 跳过 ID 分配
                  └─ 仍然调 C++ CreateScene RPC → C++ 幂等返回已有 entity
                     (覆盖 C++ 重启后 ECS 为空的情况)
```

#### 3. 两者结合 = 事件驱动的收敛控制器

不轮询（Watch 推送），不搞状态机（幂等收敛），两个 K8s 核心模式的结合：

```
etcd Watch (事件驱动，不轮询)
  │
  ├─ PUT event (节点新增/重现)
  │   └─ handleWatchEvent → initWorldScenesForZone (reconcile)
  │
  ├─ DELETE event (节点消失)
  │   └─ handleWatchEvent → 启动 grace period → 超时后清理 Redis
  │
  └─ Watch 断开
      └─ fullSync (re-list) → 对所有 zone 执行 initWorldScenesForZone (全量 reconcile)
```

旧方案（轮询）vs 新方案（list-watch）对比：

| 方面 | 轮询 (旧) | List-Watch (新) |
|------|-----------|-----------------|
| 发现节点变化 | 每 5s `etcd.Get` 全量扫描 | etcd Watch 事件推送，毫秒级 |
| 节点新增检测 | `prevSeenByZone` 前后帧 diff | Watch PUT 事件，`!existed` 判断 |
| 节点删除检测 | 遍历 Redis vs seenByZone diff | Watch DELETE 事件直接触发 |
| 负载分数刷新 | 每帧全量刷所有节点 | 5s ticker 仅更新已知节点 |
| etcd 压力 | 每 5s 一次全量 Get | 建连 + 增量流，几乎零轮询开销 |
| 容错 | 单帧失败跳过，下帧重试 | Watch 断开 → re-list + re-watch |
| 延迟 | 最差 5s（一个 tick 间隔） | 毫秒级（etcd 推送延迟） |

### Three-Layer Protection

1. **C++ CreateScene idempotent by `config_id`**: Before creating a new entity, scans `sceneRegistry.view<SceneInfoComp>()` for an existing entity with the same `scene_confid`. If found, returns the existing scene info. This makes the RPC safe to call any number of times.

2. **Go `initWorldScenesForZone` dual-layer idempotency**:
   - Redis layer: `Hget` checks if `confId` already has a scene ID → skips ID allocation.
   - C++ layer: **always sends `CreateScene` RPC** regardless of Redis state (C++ deduplicates).
   - This handles the scenario where Redis has stale data (scene ID exists) but the C++ node restarted and its ECS registry is empty.

3. **Go LoadReporter list-watch**: etcd Watch 事件驱动。节点 PUT → 触发 reconcile。Watch 断开 → fullSync re-list + 全量 reconcile。

### Scenario Matrix

| Scenario | Redis State | C++ ECS State | Outcome |
|----------|-------------|---------------|----------|
| First startup | empty | empty | fullSync → allocate scene ID + create ECS entity |
| Node reconnect (data alive) | has data | has entity | Watch PUT → reconcile → RPC returns existing, zero side-effect |
| Node restart (data lost) | has data | empty | Watch PUT → reconcile → no re-allocation, RPC creates new entity |
| SceneManager restart | has data | has entity | fullSync → 全量 re-ensure → C++ dedup |
| Watch 断开重连 | has data | has entity | re-list → 全量 reconcile → C++ dedup |

## Main Scene Initialization

事件驱动，不阻塞启动：
1. `StartLoadReporter` → `fullSync()` 全量拉 etcd → 对所有发现的 zone 执行 `initWorldScenesForZone`（全量 reconcile）。
2. 之后 Watch 接管：每当有 C++ 场景节点注册（PUT 事件），`handleWatchEvent` 触发该 zone 的 `initWorldScenesForZone`（增量 reconcile）。
3. `initWorldScenesForZone`: iterates `WorldConfIds`, performs Redis idempotent check (skip ID allocation if exists), always sends C++ CreateScene RPC, assigns nodes via FNV-1a consistent hashing.

## Key Redis Keys

| Key | Type | Purpose |
|-----|------|---------|
| `scene:id_counter` | String (int) | Auto-increment scene ID |
| `scene:{id}:node` | String | Scene → node mapping |
| `world_channels:zone:{zoneId}` | Hash | confId → sceneId for main worlds |
| `instances:zone:{zoneId}:active` | ZSet | Active instances (score = create/last-active timestamp) |
| `instance:{id}:player_count` | String (int) | Player count per instance |
| `scene_nodes:zone:{zoneId}:load` | ZSet | Node load balancing (score = scene count) |
| `node:{nodeId}:scene_count` | String (int) | Node's hosted scene count |

## Configuration

In `etc/scene_manager_service.yaml`:

```yaml
WorldConfIds:         # List of scene_conf_ids that are main world scenes
  - 1001
  - 1002
InstanceIdleTimeoutSeconds: 300   # Auto-destroy idle instances after this (0 = disabled)
InstanceCheckIntervalSeconds: 30  # How often to check for idle instances
```

## File Index

| File | Purpose |
|------|---------|
| `go/scene_manager/internal/logic/createscenelogic.go` | CreateScene routing: main world vs instance |
| `go/scene_manager/internal/logic/world_init.go` | Startup init of persistent world scenes |
| `go/scene_manager/internal/logic/instance_lifecycle.go` | Auto-destroy idle instances + player count helpers |
| `go/scene_manager/internal/logic/scene_node_client.go` | gRPC client cache + RequestNodeCreateScene/DestroyScene |
| `go/scene_manager/internal/logic/enterscenelogic.go` | EnterScene: location update, gate routing, player count |
| `go/scene_manager/internal/logic/leavescenelogic.go` | LeaveScene: location cleanup, player count decrement |
| `go/scene_manager/internal/logic/destroyscenelogic.go` | Admin DestroyScene RPC handler |
| `go/scene_manager/internal/logic/load_reporter.go` | etcd → Redis node discovery, load sync, grace period |
| `proto/scene/scene.proto` | C++ Scene service (CreateScene, DestroyScene) |
| `proto/scene_manager/scene_manager_service.proto` | Go SceneManager service definition |
| `cpp/nodes/scene/handler/rpc/scene_handler.cpp` | C++ CreateScene/DestroyScene ECS handlers |
