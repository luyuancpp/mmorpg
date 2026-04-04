# SnowFlake Guard & Node ID Conflict Handling

## Problem Statement

When a node experiences network partition:
1. Node A (node_id=5) loses connectivity → etcd lease expires → key deleted
2. Node B starts, CAS succeeds → also gets node_id=5
3. Danger window: both A and B hold same node_id → SnowFlake ID collision

SnowFlake ID layout: `[time:32][node_id:17][step:15]` — same node_id in same second = duplicate IDs.

## Solution: Redis SnowFlake Guard (Zero-Delay Startup)

### Design Principle
**Node restart must be instant.** No delayed startup. Player experience should feel like "reconnecting a cable."

### Write Path (Heartbeat)
Every keepalive heartbeat writes a Redis guard key:
```
SETEX snowflake_guard:{zone_id}:{node_type}:{node_id} 600 {now_utc_seconds}
```
- TTL = 600 seconds (10 minutes), fixed — long enough for any restart scenario
- Value = UTC timestamp of last heartbeat
- When node dies, no one renews → key auto-expires

### Read Path (New Node Startup)
After CAS success in etcd:
1. `GET snowflake_guard:{zone_id}:{node_type}:{node_id}`
2. If guard exists → `SetGuardTime(now_utc)` → SnowFlake skips current second
3. Immediately start RPC server — zero delay
4. If no guard → start immediately (old node died long ago, no collision risk)

### Why `SetGuardTime(now)` Not `SetGuardTime(lastTs)`
- `lastTs` was written seconds/minutes ago → already in the past → Generate() sees `current_second >> lastTs` → no protection
- `now` is the new node's current time → exhausts current second → next Generate() starts from `now+1` → guaranteed no overlap

### Why This Is Safe
- etcd CAS success = old node's lease has expired = old node is network-isolated
- Network-isolated node: can't receive RPC, can't write to DB → its SnowFlake IDs exist only in local memory, cannot persist
- SetGuardTime(now) skips current second → even if old node survives HCI more seconds, its IDs can't escape

### Code Locations
| File | What |
|------|------|
| `cpp/libs/engine/core/utils/id/snow_flake.h` | `SetGuardTime()` method |
| `cpp/libs/engine/threading/snow_flake_manager.h` | `SetGuardTime()` pass-through |
| `cpp/libs/engine/core/node/system/etcd/etcd_manager.cpp` | `WriteSnowFlakeGuard()` — heartbeat writes |
| `cpp/libs/engine/core/node/system/etcd/etcd_service.cpp` | `ActivateSnowFlakeAfterGuard()` — read + activate |

## OnNodeIdConflictShutdown Pre-Exit Hook

Before `LOG_FATAL` terminates the process, subclasses get a callback to save/migrate players.

```cpp
enum class NodeIdConflictReason {
    kLeaseExpiredByEtcd,     // keepalive returned TTL=0
    kLeaseDeadlineExceeded,  // local health check: no ACK within TTL
    kReRegistrationFailed,   // re-register CAS failed, another node owns this ID
};

virtual void OnNodeIdConflictShutdown(NodeIdConflictReason reason);
```

### Call Sites (3)
1. `OnKeepAliveResponse` — etcd returns TTL=0
2. `OnTxnFailed` — re-registration CAS failed
3. `StartNodeRegistrationHealthMonitor` — local lease timeout detection

### Subclass Override Plans
- **SceneNode**: flush player data → notify Centre to migrate → kick players
- **GateNode**: disconnect all client TCP connections
- **InstanceNode**: dungeon is lost (not recoverable) → notify clients → kick

## Three-Layer Protection Summary

| Layer | Mechanism | What It Prevents |
|-------|----------|-----------------|
| 1. etcd CAS | PutIfAbsent with version comparison | Two healthy nodes claiming same ID |
| 2. Lease TTL | keepalive TTL=0 → LOG_FATAL | Stale node running after lease death |
| 3. Redis Guard | SetGuardTime(now) on startup | SnowFlake ID collision in overlap window |

## Design Iteration History

| Version | Approach | Outcome |
|---------|----------|---------|
| v1 | Delay startup by safety window | **Rejected** — node restart must be instant |
| v2 | SetGuardTime(lastTs) from Redis | **Wrong** — lastTs in past, no protection |
| v3 | SetGuardTime(now) + instant start | **Final** — zero delay, zero collision |
