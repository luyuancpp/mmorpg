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
