# SnowFlake Node ID Lease-Based Recycling

**Date:** 2026-04-15

## Problem

The original `mustAllocNodeID` in the Go scene_manager used a monotonically incrementing etcd counter and permanent hostname keys. Node IDs were never released:

- Counter only went up; dead hostname keys stayed in etcd forever.
- K8s Deployments with random hostnames (e.g. `sm-7f8b9c-xk2jf`) would consume a new ID on every pod recreation.
- Eventually exhausts the 17-bit node ID pool (131072 IDs).

StatefulSets with stable hostnames were safe (restart = reuse), but Deployments were not.

## Solution: Lease-Based Dual-Key Recycling

### Key Layout

Both keys share a single etcd lease (60s TTL, background KeepAlive):

| Key | Value | Purpose |
|-----|-------|---------|
| `/scene_manager/snowflake_nodes/{hostname}` | id | Hostname -> ID (restart idempotency) |
| `/scene_manager/snowflake_ids/{id}` | hostname | ID -> Hostname (uniqueness index, recycling) |

### Allocation Flow

1. **Reclaim**: If hostname key already exists, CAS-refresh both keys with new lease (restart-safe).
2. **Scan**: Collect all in-use IDs from both `snowflake_ids/` (new keys) and `snowflake_nodes/` (old permanent keys for backward compat).
3. **Pick**: Find smallest free ID in [0, 131071].
4. **Claim**: CAS create both keys atomically. If another instance raced, retry with fresh scan.

### Recycling

Pod dies -> KeepAlive stops -> lease expires after 60s -> both keys auto-deleted -> ID is free.

### Backward Compatibility

- Old permanent hostname keys (from before this change) are scanned into `usedIDs` to avoid collision.
- Old `snowflake_counter` key remains in etcd but is harmless (no longer read).

## C++ Node ID Allocation — Comparison

The C++ side (`node_allocator.cpp` → `AcquireNode()`) already supports natural recycling:

1. Reads a **local snapshot** of live nodes (populated by etcd watch).
2. Picks `maxUsedId + 1` or the smallest gap.
3. CAS `PutIfAbsent` to etcd with the **node's lease**.
4. Node dies -> lease expires -> key auto-deleted -> ID is free in next snapshot.

**Key difference:** C++ doesn't use hostname-based identity. It picks from a live snapshot every time.
Both approaches are now safe for ephemeral K8s pods.

### Why C++ doesn't need the hostname pattern

The C++ node's etcd key includes `node_type` and `node_id` (not hostname), and the etcd watch gives every node a real-time view of which IDs are alive. Since the key is lease-bound, dead nodes' keys vanish automatically, and a restarting node just grabs the next free slot. There is no "stale key" problem because the lease is the garbage collector.

## Safety Analysis

| Scenario | Go (new) | C++ |
|----------|----------|-----|
| Same hostname restart | Reuses same ID (idempotent) | Reuses if slot is still free (lease may not have expired yet — SnowFlake guard protects) |
| New random hostname | Picks smallest free recycled ID | Picks smallest free from live snapshot |
| Pod crash (no graceful shutdown) | Lease expires in 60s, ID freed | Lease expires per config TTL, ID freed |
| K8s rolling update | Old pod's lease expires, new pod may get same or different ID | Same behavior |
| Pool exhaustion | Impossible in practice (IDs recycle) | Impossible in practice (IDs recycle) |

## Design Decision: Keep Both Approaches

Considered unifying Go to use C++ pure-scan approach. Decided to **keep both as-is**.

### Trade-off Comparison

| | C++ (pure scan slot) | Go (hostname + ID dual key) |
|---|---|---|
| Code | Simpler (one key) | Slightly more (two keys) |
| Restart identity | Not guaranteed same ID | Same hostname -> same ID |
| Log correlation | ID may drift across restarts | Stable ID, easier to grep |
| Extra protection | Has `SnowFlakeGuard` (Redis SETEX 600s) | Not needed -- hostname key is the guard |
| Correctness | Equal | Equal |

### Why not change Go to match C++

- Go has no etcd watch snapshot -- `mustAllocNodeID` runs once at startup, must self-scan.
- Losing hostname idempotency degrades observability with no offsetting benefit.
- Go lacks the `SnowFlakeGuard` Redis mechanism that C++ uses to protect ID reuse windows.
- Dual-key code is already written, compiled, and clear.

### Why not change C++ to match Go

- C++ already has watch-maintained live snapshot -- hostname mapping is redundant.
- Adding hostname key + dual-key CAS would increase code complexity with zero benefit.
- `SnowFlakeGuard` already covers the restart-reuse safety window.

### Conclusion

Each language uses the approach that best fits its runtime model. Both are safe for ephemeral K8s pods. No unification needed.
