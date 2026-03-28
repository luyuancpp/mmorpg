# Node ID Conflict Handling Design (Critical Architectural Decision)

## Core Scenario

**Stale node reconnects after network failure (most dangerous):**

```
T=0    NodeA(node_id=5, SceneNode) running, players online
T=30   NodeA's network cable disconnected, cannot send keepalive
T=60   etcd TTL expires, automatically deletes /node/5 key
T=61   NodeB discovers key disappearance via Watch → CAS succeeds → also gets node_id=5
T=65   NodeA's health check detects elapsed>TTL → LOG_FATAL → should self-terminate

But a window exists: T=61~T=65, A and B simultaneously hold node_id=5
Even worse: NodeA's cable is plugged back in at T=63 → A never triggers LOG_FATAL → A and B coexist with the same node_id long-term
```

## Existing Safeguards (3 layers)
1. CAS PutIfAbsent — prevents two healthy processes from writing the same key simultaneously
2. keepalive TTL=0 → LOG_FATAL self-termination
3. health_check_interval periodic check: elapsed>TTL → LOG_FATAL self-termination

## Uncovered Gap
- NodeA reconnects before the health_check_interval (5s) check fires
- After NodeA's network recovers, its etcd watch reconnects and sees node_id=5 is now held by B
- **At this point NodeA has no handling logic and continues serving players → split-brain + SnowFlake ID collision**

## Handling Logic To Be Implemented

### When NodeA detects its node_id has been taken by a new node:
1. **Immediately stop accepting new game logic requests** (stop processing any business messages)
2. **Save all online player data** (flush to DB/Redis)
3. **Kick all players**, redirecting them to another node of the same type under the same zone_id
4. **Self-terminate**

### Instance Node (InstanceNode) Special Case:
- Instances are stateful; players cannot simply be redirected to another instance (instance progress is unique)
- Handling strategy TBD: Option 1: kick players directly (instance fails), notify the client to retry
- Option 2: serialize and migrate instance state (too complex, not considered for now)
- **Current conclusion: when an instance node detects an ID conflict on reconnect, kick players directly (instance is lost), tell the client to re-enter**

## Detection Timing
- etcd Watch event: receives a PUT event where the key matches own node_id key, but the uuid differs from own
- i.e., detect in `HandlePutEvent`: key == own nodeKey && value.node_uuid != own uuid

## Relevant Code Locations
- `cpp/libs/engine/core/node/system/node/node_connector.cpp:51` — existing warning log for uuid mismatch; migration logic should be added here or in the service_discovery layer
- `cpp/libs/engine/core/node/system/etcd/etcd_service.cpp:HandlePutEvent` — watch event entry point
- `cpp/libs/engine/core/node/system/node/node.cpp:HandleServiceNodeStop` — node shutdown handling
- `IsZoneSingletonNodeType` — singleton nodes are not affected (ForcePut overwrite semantics)

## SnowFlake Guard (Redis — Prevents Time-Window ID Collision)
- On every keepalive, write `SETEX snowflake_guard:{zone_id}:{node_type}:{node_id} 600 {now_seconds}`
- After the new node's CAS succeeds, it reads the guard and **does not delay startup**; instead it calls `SetGuardTime(now_utc)` to skip the current second
- Uses `now` (new node's current time) rather than `lastTs` (time written by the old node), because lastTs is in the past and SetGuardTime would be ineffective
- `now` is already > the lease expiration point; the old node is network-isolated at this time (lease expired = network unreachable), so any IDs it generates cannot leave
- Guard TTL is fixed at 600 seconds (10 minutes), sufficient to cover any restart scenario without affecting correctness
- Node starts within seconds, no stalling

### Key Design Decision (User Emphasis)
**Node restart must be extremely fast.** In canary update / cable-reconnect scenarios, the player experience should feel like "plugging the cable back in" — server maintenance time must be as short as possible.
Any scheme that introduces startup delay (e.g., waiting for a safety window) is unacceptable.

## OnNodeIdConflictShutdown Virtual Function Interface
- `Node::OnNodeIdConflictShutdown(NodeIdConflictReason reason)` — called before LOG_FATAL
- 3 call sites:
  1. `etcd_service.cpp:OnKeepAliveResponse` — TTL=0 (kLeaseExpiredByEtcd)
  2. `etcd_service.cpp:OnTxnFailed` — re-registration CAS failure (kReRegistrationFailed)
  3. `node.cpp:StartNodeRegistrationHealthMonitor` — local lease timeout detection (kLeaseDeadlineExceeded)
- Base class default: no-op + logging. Subclass overrides implement:
  - SceneNode: save player data → migrate to new node
  - GateNode: disconnect all client connections
  - InstanceNode: notify players of abnormal instance termination (instance loss is unrecoverable, unlike normal node migration)
- Status: interface is in place; subclass overrides are pending implementation

## Canary Update Scenario
- When the old node receives SIGTERM, it should proactively: 1) save player data 2) kick players 3) deregister etcd key → new node takes over seamlessly
- This flow is not yet implemented; needs to be added to the graceful shutdown path
