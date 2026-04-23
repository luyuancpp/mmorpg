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

Keys worth a Grafana / redis-cli dashboard:

| Key | Meaning |
|-----|---------|
| `scene_nodes:zone:{zoneId}` | Live node IDs in the zone |
| `scene_nodes:zone:{zoneId}:load` | ZSET, lowest score = most attractive |
| `node:{id}:player_count` | Aggregate online players on that pod |
| `node:{id}:scene_node_type` | 0=world, 1=instance, 2=world-cross, 3=instance-cross |
| `scene:{id}:player_count` | Per-scene online players (subset of the above) |

Recommended alerts:
- Instance pool empty while strict mode is on — paging alert.
- `node:{id}:player_count` stays ≥ 90% of expected cap for > 5 min on
  any world pod → consider bumping `WorldChannelCountByConfId`.
- ZSET score dispersion on `scene_nodes:zone:{zoneId}:load` < 10% →
  consider raising `NodeLoadWeightPlayerCount` so live players carry
  more weight than scene count.

## 5. Tuning checklist

| Symptom | Action |
|---------|--------|
| Capital city laggy | Add entry to `WorldChannelCountByConfId`, e.g. `1: 4` |
| Instance creation rejected (`ErrNoNodeForPurpose`) | Verify `SCENE_NODE_TYPE=1` is set on the instance Deployment; confirm pods are Ready |
| All instances landing on one pod | Check that etcd NodeInfo publish includes `sceneNodeType`; verify Redis ZSET score is being updated |
| Mirrors spread across pods instead of co-locating | Expected only for brand-new mirrors when the source pod is at capacity; otherwise check mirror co-location path in `createscenelogic.go` |
