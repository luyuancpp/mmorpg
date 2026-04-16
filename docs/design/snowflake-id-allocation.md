# SnowFlake ID Allocation

## Layout
Matches C++ `SnowFlake` in `cpp/libs/engine/core/utils/id/snow_flake.h`:
- `[time:32][node:17][step:15]`
- Epoch: 2026-03-14 00:00:00 UTC (1773446400)
- Node range: 0..131071 (17 bits)
- Step range: 0..32767 per second (15 bits)

## Go Implementation
- `go/shared/snowflake/snowflake.go` — thread-safe generator (mutex)
- `go/shared/snowflake/snowflake_test.go` — 7 tests (uniqueness, monotonic, concurrency, etc.)

## Node ID Allocation (etcd atomic)
Located in `go/scene_manager/internal/svc/servicecontext.go` → `mustAllocNodeID()`.

1. Use `os.Hostname()` as identity key (K8s pod name is naturally unique per replica)
2. Check etcd `/scene_manager/snowflake_nodes/{hostname}` — if exists, reuse (restart-safe)
3. If not, CAS increment `/scene_manager/snowflake_counter` + write hostname mapping in one transaction
4. Race retry: if txn fails, re-check hostname key (another replica may have raced)

**Guarantees:**
- Same hostname → same node ID (idempotent across restarts)
- Different hostnames → different node IDs (atomic counter, no hash collision)
- Zero configuration needed from ops

## Scene ID Generation
All scene IDs (world channels, main scenes, instances) use `svcCtx.SceneIDGen.Generate()`.
Replaced former `Redis.Incr("scene:id_counter")` which was not globally unique across Redis resets.

Call sites:
- `world_init.go` — world scene channel allocation
- `main_scene_init.go` — main scene channel allocation  
- `createscenelogic.go` → `allocateScene()` — instance creation

## Cross-Node-Type ID Isolation Invariant (2026-04-16)

Node IDs are allocated **per (zone, node_type)** — different node types can share the same `node_id` value. This means two SnowFlake generators on different node types can produce identical 64-bit IDs.

**Rule**: Each SnowFlake-generated ID type must be produced by exactly ONE node type:
- Scene nodes → item GUIDs (`SnowFlakeManager`)
- Gate nodes → session IDs (`session_id_gen`)
- Scene Manager (Go) → scene IDs (`SceneIDGen`)

**NEVER** introduce a SnowFlake ID that is produced by multiple node types — the 17-bit worker field will collide.

If a cross-node-type globally-unique ID is ever needed:
1. Encode `node_type` into the worker field (e.g. 5-bit type + 12-bit node)
2. Switch node ID allocation to a single global namespace (remove `node_type` from etcd key)
