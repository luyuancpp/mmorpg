# Scene Node Role Split Runbook

Split a zone's `scene` workload into two pools — a persistent main-world pool
and an on-demand instance pool — so that WoW-style routing (world vs
dungeon/battleground) is honoured end to end.

This runbook assumes you have already applied the Go-side changes in
`docs/design/scene-creation-architecture.md` ("Node Role Separation") and
that `scene_manager_service.yaml` ships with `StrictNodeTypeSeparation:
true`.

## 1. How the pieces connect

```
K8s Deployment (scene-world)     K8s Deployment (scene-instance)
  env SCENE_NODE_TYPE=0            env SCENE_NODE_TYPE=1
  etcd register → eSceneNodeType=0   etcd register → eSceneNodeType=1
           \                               /
            \                             /
       SceneManager LoadReporter
         mirrors scene_node_type → redis node:{id}:scene_node_type
                     |
                     v
       GetBestNodeForPurpose(world|instance)
         chooses only pods that match the requested purpose
```

`scene_manager_service.yaml` keys that matter:

| Key | Role |
|-----|------|
| `StrictNodeTypeSeparation` | `true` → routing never crosses pools; `false` → falls back to any pod if the matching pool is empty (useful during rollout) |
| `WorldChannelCountByConfId` | Per-confId channel override, independent of the role split |
| `NodeLoadWeightSceneCount` / `NodeLoadWeightPlayerCount` | Composite load score tuning |

## 2. C++ override knobs

The scene container reads two environment variables in addition to
`etc/game_config.yaml`:

| Env var | Effect |
|---------|--------|
| `SCENE_NODE_TYPE` | Overrides `GameConfig.scene_node_type` at startup. `0` = main world, `1` = instance, `2` = main world cross, `3` = instance cross. |
| `GAME_CONFIG_PATH` | Overrides the YAML file path (default `etc/game_config.yaml`). Use when you prefer two separate ConfigMaps over two env-var overrides. |

`bin/etc/game_config_instance.yaml` is shipped as a minimal reference file.

## 3. Rollout order (zero-downtime)

The safe order is: loosen Go routing → bring up the new pool → tighten Go
routing again. This way the switch does not race with pod readiness.

### 3.1 Phase 1 — Prepare
1. Confirm all existing scene pods still have `SCENE_NODE_TYPE` unset
   (defaults to `0`, treated as world-capable).
2. Temporarily set `StrictNodeTypeSeparation: false` in
   `scene_manager_service.yaml` and re-apply the ConfigMap. This makes
   the selector fall back to the whole zone pool if a purpose-specific
   pool is empty, so no request can be rejected during the cutover.
3. Scale down any in-flight instance creations if convenient (not
   required; rejection cannot happen while the flag is `false`).

### 3.2 Phase 2 — Add the instance pool
1. Edit the zone definition (for example
   `deploy/k8s/zones.ops-recommended.yaml`) to use the split form:
   ```yaml
   replicas:
     centre: 1
     gate: 2
     scene_world: 2
     scene_instance: 2
   ```
2. Re-run your `k8s-zone-up` / `k8s-all-up` command. The orchestration
   script is expected to emit two Deployments per zone:
   - `scene-world`    with `env.SCENE_NODE_TYPE=0`
   - `scene-instance` with `env.SCENE_NODE_TYPE=1`
3. Wait for both Deployments to report ready. Verify:
   ```bash
   kubectl -n mmorpg-zone-<zone> get deploy
   redis-cli SMEMBERS scene_nodes:zone:<zoneId>
   redis-cli HGETALL node:<nodeId>:scene_node_type  # expect "0" or "1"
   ```

### 3.3 Phase 3 — Tighten routing
1. Set `StrictNodeTypeSeparation: true` in `scene_manager_service.yaml`,
   re-apply, restart scene_manager.
2. Watch logs for `ErrNoNodeForPurpose` — if it fires, either pool is
   smaller than expected or `scene_node_type` failed to propagate.
3. Run smoke: create a dungeon instance and enter a main-world scene,
   confirm they land on the intended pods (compare scene id in
   `scene:{id}:node_id` against the pod's `scene_node_type`).

### 3.4 Rollback
- Flip `StrictNodeTypeSeparation` back to `false` and scale the instance
  Deployment to zero. Existing main-world scenes are untouched; the
  LoadReporter will clean up the instance pods' Redis entries within
  `LoadReporterInterval` (default 10s).

## 4. Observability

### 4.1 Redis surface

Keys worth a Grafana / redis-cli dashboard:

| Key | Meaning |
|-----|---------|
| `scene_nodes:zone:{zoneId}` | Live node IDs in the zone |
| `scene_nodes:zone:{zoneId}:load` | ZSET, lowest score = most attractive |
| `node:{id}:player_count` | Aggregate online players on that pod |
| `node:{id}:scene_node_type` | 0=world, 1=instance, 2=world-cross, 3=instance-cross |
| `scene:{id}:player_count` | Per-scene online players (subset of the above) |

### 4.2 Prometheus metrics

Set `MetricsListenAddr: ":9150"` in `scene_manager_service.yaml` to expose
`/metrics`. Gauges (all labelled `{node_id, zone_id, role}` unless noted):

| Metric | Meaning |
|--------|---------|
| `scene_manager_node_player_count` | Mirrors `node:{id}:player_count` |
| `scene_manager_node_scene_count` | Mirrors `node:{id}:scene_count` |
| `scene_manager_node_load_score` | Composite score actually used for scheduling |
| `scene_manager_nodes_by_role{zone_id,role}` | Live-node count per role — use for alerting when a role pool empties |

Recommended alerts:
- `sum by (zone_id) (scene_manager_nodes_by_role{role="instance"}) == 0`
  while strict mode is on — paging alert.
- `node:{id}:player_count` stays ≥ 90% of expected cap for > 5 min on
  any world pod → consider bumping `WorldChannelCountByConfId`.
- ZSET score dispersion on `scene_nodes:zone:{zoneId}:load` < 10% →
  consider raising `NodeLoadWeightPlayerCount` so live players carry
  more weight than scene count.
- `scene_node_type` value outside `{0,1,2,3}` (logged as `[LoadReporter]
  unknown scene_node_type`) — almost always a stale image or env typo;
  page ops.

## 5. Rebalance on node join / leave

When a world-hosting node joins or leaves the zone, SceneManager runs
`RebalanceWorldChannelsForZone` which moves channels toward the new hash
distribution, subject to two safety rules:

1. **Urgent**: channels whose current node is dead or no longer
   world-hosting are always re-homed (possibly to the same target node
   where the C++ `CreateScene` handler idempotently re-creates the ECS
   scene).
2. **Opportunistic**: channels on a live node but mapped to a non-ideal
   hash target are only moved when their `player_count == 0`. Hot
   channels stay put — live player migration is out of scope and would
   need cross-node state transfer that this codebase does not have.

`MaxRebalanceMigrationsPerTick` (default 10, set to 0 to disable) caps
how many moves fire per event. Rebalance runs in three contexts:

1. **Event-driven** — on every etcd PUT/DELETE of a world-hosting node.
2. **Periodic** — every `RebalanceCheckIntervalSeconds` (default 5 min)
   catches drift that has no corresponding etcd event (e.g. a hot
   channel drained naturally).
3. **Startup** — `fullSync` calls rebalance for every zone so drift that
   built up while SceneManager was down converges on reboot.

Watch for this log line to confirm rebalance fired:

```
[Rebalance] zone=1 urgent=2 opportunistic=0 migrated=2 skipped=0 pending=0 budget=10
```

If `skipped` is consistently non-zero, the target nodes are refusing
`CreateScene` — check the C++ side logs on those pods.

### 5.1 Inspecting the queue without triggering migrations

`GET /debug/rebalance-plan` on `MetricsListenAddr` returns the current
planner output as JSON (urgent + opportunistic lists per zone), without
actually migrating anything. Add `?zone=<id>` to narrow scope.

```
$ curl -s localhost:9150/debug/rebalance-plan | jq '.zones[].urgent | length'
0
```

Pair with the Prometheus gauges `scene_manager_rebalance_pending{reason}`
and counter `scene_manager_rebalance_migrations_total{reason,outcome}`
for dashboards and alerting (see `deploy/k8s/scene-manager-alerts.yaml`).

## 6. Orphan cleanup

When `CleanupOrphanChannelsOnStartup: true` (default), every fullSync
iteration scans Redis for `world_channels:*` sets whose confId is no
longer in `World.json` and removes them (plus their per-scene
`scene:{sid}:node` / `zone` / `player_count` keys, and best-effort
`DestroyScene` on the hosting node).

Safety guards:

- Refuses to run when `worldConfIds()` returns empty (treats it as a
  table-load failure, not an intentional wipe).
- Only matches on `confId not in World.json`; unknown zones with valid
  confIds are skipped so a zone restart does not trigger deletion.
- Scans via `SCAN` cursor in batches of 256 keys to avoid blocking Redis.

Disable with `CleanupOrphanChannelsOnStartup: false` when sharing a
Redis cluster across incompatible World.json versions (e.g. a canary
deploy alongside stable), otherwise leave it on.

## 7. Pre-packaged alerts

`deploy/k8s/scene-manager-alerts.yaml` ships a PrometheusRule covering:

| Alert | Severity | Trigger |
|-------|----------|---------|
| `SceneManagerInstancePoolEmpty` | critical | no instance-hosting pods in zone for 2 min |
| `SceneManagerWorldPoolEmpty` | critical | no world-hosting pods in zone for 2 min |
| `SceneManagerWorldNodeSaturated` | warning | max player_count > 1800 for 10 min |
| `SceneManagerLoadScoreDispersionLow` | info | stddev/mean of load_score < 0.1 for 15 min |
| `SceneManagerRebalanceStalled` | warning | urgent migration queue non-zero for 5 min |
| `SceneManagerRebalanceFailureRate` | warning | >50% migration failure rate for 10 min |
| `SceneManagerUnknownSceneNodeType` | warning | any pod publishes scene_node_type outside {0,1,2,3} |

Apply with:

```
kubectl apply -n observability -f deploy/k8s/scene-manager-alerts.yaml
```

Thresholds are starting points — tune `SceneManagerWorldNodeSaturated`
against your actual per-pod player cap and the `RebalanceFailureRate`
ratio against your observed steady-state failure rate.

## 8. Verifying the pipeline in CI

`go/scene_manager/internal/logic/integration_test.go` wires the rebalance
pipeline end-to-end against in-process fakes and is the cheapest way to
catch regressions before a cluster rollout:

| Test | What it locks in |
|------|-------------------|
| `TestIntegration_Rebalance_NodeGone_EndToEnd` | Dead node → new node actually receives CreateScene; Redis `scene:{sid}:node` flips; scene_count counters move; dead node is NOT called for DestroyScene. |
| `TestIntegration_Rebalance_BetterHome_EndToEnd` | Hash-ring opportunistic move between two live nodes; old (live) node gets best-effort DestroyScene. |
| `TestIntegration_Rebalance_CreateSceneFailure_KeepsOldMapping` | If the new node rejects CreateScene, Redis mapping STAYS on the old node — no player is ever routed to a scene that doesn't exist. |
| `TestIntegration_Rebalance_ActivePlayersBlockMigration` | A channel with online players is never migrated, even when the hash ring wants to move it. |

The harness uses `google.golang.org/grpc/test/bufconn` for the scene-node
gRPC server and miniredis for Redis, so it runs without Docker, without
etcd, and in under a second. When adding a new rebalance safety rule,
add a matching integration test here before landing the production
change; the `fakeSceneNode` counters double as a contract for what C++
RPCs the SceneManager is allowed to issue on any given code path.

Swappable seams exposed for test authors:

- `logic.SetNodeDialerForTest(dialer)` — routes `CreateScene` / `DestroyScene`
  / `ReleasePlayer` traffic through a caller-supplied dialer (bufconn in
  the stock harness). Always pair with `t.Cleanup(restore)`.
- `logic.ResetNodeConnCacheForTest()` — evicts cached gRPC ClientConns so
  a second sub-test doesn't re-use a previous test's already-closed
  listener. Call in the test setup, not just teardown.

## 9. Tuning checklist

| Symptom | Action |
|---------|--------|
| Capital city laggy | Add entry to `WorldChannelCountByConfId`, e.g. `1: 4` |
| Instance creation rejected (`ErrNoNodeForPurpose`) | Verify `SCENE_NODE_TYPE=1` is set on the instance Deployment; confirm pods are Ready |
| All instances landing on one pod | Check that etcd NodeInfo publish includes `sceneNodeType`; verify Redis ZSET score is being updated |
| Mirrors spread across pods instead of co-locating | Expected only for brand-new mirrors when the source pod is at capacity; otherwise check mirror co-location path in `createscenelogic.go` |
