# MMO Cross-Server Architecture Design (Complete Reference)

## 1. Background & Goals

- Game type: MMO (WoW-style), approximately 1000+ zones.
- Player affiliation: each player has a home zone, but can freely visit scenes on other zones.
- Current constraint: each region has an independent Redis cluster.
- Design priority: stability and correctness first; extra latency during scene transitions is acceptable.

Core goals:

- Cross-server capability must be transparent to the business logic layer.
- No cross-server player data migration.
- Confine cross-server complexity to a small set of base components; prevent it from leaking into Scene/Gate business code.

## 2. Core Principle: Location Transparency

> Regardless of which server a player is currently on, the logic layer should handle them exactly the same as a local player.

Implementation rules:

- Business layer must never branch on `is_cross_server`.
- Scene/Gate must not care about the player's home zone or Redis topology.
- Player data is always persisted in the home zone's storage.
- Routing changes and cross-server policy adjustments happen only at the routing/data-service layer.

Anti-patterns (historical issues, explicitly prohibited):

- Copying data to the destination server (consistency nightmare).
- Checking "is this cross-server?" everywhere — login, scene switch, skills, bag, etc.
- Each Scene node maintaining its own cross-server Redis connections and routing rules.

## 3. Architecture Choice: Option A (Independent Data Proxy Service)

### 3.1 Conclusion

Adopt an independent Data Service (Go/go-zero) as the cross-server data proxy layer, uniformly handling Scene read/write requests and internally routing them to the home zone's storage.

### 3.2 Why Option A, Not Option B (Scene-Embedded Routing)

- If every Scene directly connects to all zone Redis instances, connection count explodes.
- Routing logic scattered across all Scene nodes makes upgrades and troubleshooting expensive.
- Option A concentrates complexity into a horizontally scalable service layer, better for stable operation and ops governance.

### 3.3 Logical Topology

```text
Player -> Gate -> Scene (any target zone)
                   |
                   | gRPC (unified interface, server-side cross-zone transparent)
                   v
              Data Service
                   |
                   | player_id -> home_zone_id -> region -> redis
                   v
             Home Redis/DB
```

## 4. Control Plane vs Data Plane

### 4.1 Control Plane (Player Routing / Scene Scheduling)

- Centre / SceneManager / PlayerLocator handle "which Scene node is the player currently on".
- Gate handles client connection hosting and forwarding.
- Gate and SceneManager communicate via Kafka control messages, avoiding large-scale gRPC long-connection mesh topology.

Current conventions:

- Kafka Topic: `gate-{gate_id}`.
- Command protocol: `GateCommand` (e.g. `RoutePlayer`, `KickPlayer`).
- Safety field: `target_instance_id`, used to filter zombie messages (stale messages from restarted nodes).

### 4.2 Data Plane (Player Data Read/Write)

- Scene only calls unified data interfaces (e.g. `GetPlayerBag(player_id)`), unaware of cross-server details.
- Data Service responsibilities:
  - `player_id -> home_zone_id` mapping resolution.
  - `home_zone_id -> region_id -> redis_cluster` routing.
  - Retry, circuit breaking, degradation, observability, hotspot caching.

## 5. Zone Layering & Region Lock Rules

- Zone: game logic server (where players create characters and permanently belong).
- Region: grouping layer above Zones (ops and storage allocation unit).
- Storage strategy: Redis is allocated per Region, not per individual Zone.

Region Lock rules:

- Lock enabled: players can only access Zones within their home Region.
- Lock disabled: players can access any Zone.
- Lock decisions belong to SceneManager (control plane), not Data Service (data plane).

## 6. Player ID & Mapping Table Design

### 6.1 Player ID Rules

- Uses existing Snowflake: `[time:32][node_id:17][step:15]`.
- `zone_id` must NOT be encoded into `player_id`.
- `node_id` only represents "the physical node that generated the ID", not a zone semantic.

Rationale:

- If IDs contain zone semantics, server merges risk systemic rewrites.
- Player IDs must remain immutable for their entire lifecycle (cross-server, merge, migration — never change).

### 6.2 Independent Mapping Tables

- `player_id -> home_zone_id` (global truth mapping).
- `home_zone_id -> region_id` (configuration mapping).
- `region_id -> redis_addr` (routing configuration).

Recommended storage:

- `player_id -> home_zone_id` in Global Redis or MySQL + local cache.
- Written at character registration time; serves as the basis for all subsequent data routing.

## 7. Cross-Server Scene Transition Sequence (Critical Consistency Constraint)

Core invariant: at any given moment, only one Scene may write a player's data (Single Writer).

Standard sequence:

1. SceneManager notifies the old Scene: release the player and flush to storage.
2. Old Scene confirms "saved and released".
3. Only then does SceneManager notify the new Scene: load the player.
4. If step 2 times out, abort the migration — the new Scene must not load preemptively.

This sequence guarantees:

- No concurrent writes to the same player from two Scenes.
- No state overwrites or rollback difficulties during scene transitions.

## 8. Consistency Defense Layers

Layer 1 (primary defense):

- SceneManager serializes the transition flow, enforcing Single Writer.

Layer 2 (anomaly fallback):

- Data Service applies short-lived distributed locks on player keys (e.g. Redis `SETNX` + TTL ~3 seconds).

Layer 3 (last resort):

- Critical data uses version numbers (optimistic locking) or transactional checks (WATCH/MULTI).

Note: Redis single-threading only guarantees same-key single-connection FIFO; the real risk comes from upstream concurrent writes, so Single Writer must be enforced at the architecture layer.

## 9. Server Merge Strategy (Player IDs Unchanged)

Strategy A (recommended):

- Migrate target server data into the primary server's storage; batch-update `player_id -> home_zone_id` mappings.

Strategy B (transitional):

- Route multiple Zones to the same Redis cluster; gradually complete data merging.

Common properties:

- Neither modifies `player_id`.
- Business code (Scene/Gate) requires no changes.

## 10. NodeId Conflict & Fast Recovery Design

### 10.1 Problem Definition

The etcd lease expiry/re-registration window can cause the same `node_id` to be dual-active (old node recovers network + new node has already taken over).

Risks:

- Dual-active nodes processing player requests.
- Snowflake ID collision with same `node_id` issuing concurrently.

### 10.2 Conflict Handling Requirements

When a node confirms "my `node_id` has been taken by another uuid", it must:

1. Immediately stop accepting new business requests.
2. Flush online player data as quickly as possible.
3. Kick or migrate online players (per node-type strategy).
4. Self-terminate to prevent continued dual-active state.

Node-type strategies:

- SceneNode: save state, then migrate players to an available node in the same zone.
- GateNode: disconnect clients and trigger reconnection routing.
- InstanceNode: instance state cannot be migrated — notify instance failure and kick players.

### 10.3 Snowflake Conflict Protection

- Keepalive writes `snowflake_guard:{zone}:{type}:{node_id}` (TTL 600s).
- After a new node takes over, call `SetGuardTime(now_utc)` to skip the current-second ID issuance window.
- No "wait for safe window" startup delay — preserve fast gray-release / reconnection recovery.

## 11. Integration Boundaries with Current Codebase

- Gate: retains Kafka control-plane consumer, receiving SceneManager commands on `gate-{id}`.
- SceneManager: responsible for scene-transition orchestration, region-lock strategy, Single Writer sequence execution.
- PlayerLocator: records the player's current Scene location; extensible to carry home_zone info.
- Data Service: can extend existing `go/db/` or create new `go/data_service/`.

## 12. Permanent Design Red Lines

1. No business module may introduce a "cross-server mode branch".
2. Player data must not be cross-server copied as a routine mechanism.
3. All cross-server routing truth must be centralized in the data proxy layer.
4. Any new feature that needs to know "which server the player came from" should trigger an architecture review first.
5. Stability over latency; correctness over local performance.

## 13. Follow-Up Implementation Checklist

- Complete differentiated implementations of `OnNodeIdConflictShutdown` in Node subclasses (Scene/Gate/Instance).
- Add to the graceful shutdown flow: persist player data -> migrate/kick players -> actively deregister node key.
- Data Service: add per-player lock and critical write version fields.
- Establish cross-server scene-switch observability: per-phase latency, failure reasons, rollback counts.
## 14. Cross-Scene Player Messaging

### 14.1 Problem

Scene A needs to send a message to a player (e.g., quest reward, trade result, system notification), but the player may not be on Scene A.
The old approach forwarded through Centre, making Centre a bottleneck with complex flow. The new approach delivers via Kafka, but introduces routing-window consistency issues.

### 14.2 Two Scenarios

**Scenario 1: Player is not on the local Scene (cross-scene delivery)**

- Deliver the message via Kafka to the topic of the Scene where the player currently resides.
- Key edge case: the message has entered Kafka (target Scene B), but during routing the player switches from B to C; by the time Scene B receives it, the player is already gone.

**Scenario 2: Player is on the local Scene (local delivery)**

- The local Scene can process directly, or route uniformly through a player message queue.

### 14.3 Message Priority Classification (Protobuf Option Annotation)

Annotate message priority on RPC service method definitions via `option`; the code generation layer produces different delivery strategies accordingly:

| Level | Meaning | Delivery Guarantee | Routing Failure Handling |
|-------|---------|--------------------|--------------------------|
| IMPORTANT | Trade results, reward grants, state changes, etc. | at-least-once | Target Scene finds player has left -> re-query PlayerLocator -> forward to new Scene; if still fails -> persist to DB/Redis, redeliver when player enters a scene next time |
| NORMAL | General notifications, non-critical UI hints | best-effort | Target Scene finds player absent -> discard |

Example proto option definition (to be refined):

```protobuf
extend google.protobuf.MethodOptions {
  MessagePriority message_priority = 51000;
}

enum MessagePriority {
  NORMAL = 0;
  IMPORTANT = 1;
}

service ScenePlayerService {
  rpc SendReward(SendRewardRequest) returns (SendRewardResponse) {
    option (message_priority) = IMPORTANT;
  }
  rpc SendChatBubble(ChatBubbleRequest) returns (ChatBubbleResponse) {
    option (message_priority) = NORMAL;
  }
}
```

### 14.4 Local Delivery Strategy

**Option A: Direct processing (synchronous path)**

- Lowest latency; suitable for high-frequency, low-latency messages such as movement sync.
- Drawback: local and cross-scene paths have inconsistent logic; unified message ordering cannot be guaranteed.

**Option B: Unified player message queue (asynchronous path)**

- All messages destined for a player (whether local or cross-scene) enter a player message queue and are handled by a unified consumer.
- Advantages: consistent with the "location transparency" principle; uniform logic; ordered messages.
- Drawback: local messages incur one extra queue hop.

**Conclusion**: Default to the unified message queue (Option B), consistent with the cross-server "location transparency" principle. For ultra-low-latency scenarios such as movement sync, allow marking messages to take the direct path (Option A) as an exception.

### 14.5 Delivery Flow

```text
Sender Scene A
    |
    |-- Query PlayerLocator: where is the player?
    |
    +-- On local Scene A --> enqueue to local player message queue --> unified processing
    |
    +-- On Scene B --> Kafka deliver to scene-{scene_b_id} topic
                          |
                          v
                       Scene B consumes the message
                          |
                          +-- Player present --> enqueue to local player message queue --> unified processing
                          |
                          +-- Player has left
                                |
                                +-- IMPORTANT --> re-query PlayerLocator --> forward to new Scene / persist for later redelivery
                                |
                                +-- NORMAL --> discard
```

### 14.6 Relationship to Existing Architecture

- Reuses Kafka infrastructure: shares the Kafka cluster with the Gate control plane, adding `scene-{scene_id}` topics.
- PlayerLocator query: reuses the existing `player_locator` service to obtain the player's current location.
- Single Writer guarantee: message delivery does not violate Single Writer (§7); messages are consumed and processed only on the Scene where the player currently resides.
- Persistence fallback: important messages are persisted via the unified data channel of Data Service (§3).

### 14.7 To Be Implemented

- [ ] Define `MessagePriority` protobuf option and integrate into proto-gen (historical name: pbgen) code generation pipeline.
- [ ] Implement player message queue on the Scene side (ECS Comp + consumer System).
- [ ] Add `scene-{scene_id}` topic subscription to the Scene Kafka Consumer.
- [ ] Retry / persist-and-redeliver logic for important messages after delivery failure.
- [ ] Determine Kafka topic partitioning strategy (player hash vs. scene ID).

## 15. Readiness Gating for Stateless Microservices

### 15.1 Problem

In a distributed game backend, services often need to complete data loading (config tables, cache warm-up, dependency connectivity checks) before they can safely handle requests;
however, microservices also aim to remain stateless and elastically scalable.

### 15.2 Principles

- Externalize readiness state: delegate the "ready to accept traffic" decision to the service discovery and orchestration layer; do not couple complex wait-state logic into business flows.
- Callers discover only ready instances: instances that are not ready must not appear in the service discovery visible set.

### 15.3 Recommended Startup Sequence

```text
Start process
  -> Load configuration / static data
  -> Warm up caches & check dependency connectivity
  -> Open service port (optional)
  -> Register with service discovery (etcd)
  -> Switch readiness=ready
```

### 15.4 Dependency Wait Strategy

- Hard dependency path: watch service discovery (etcd) and resume business flow after the target instance becomes ready.
- Soft dependency path: fail fast + exponential back-off retry; avoid blocking the entire call chain.

### 15.5 Project-Level Conventions

- Go services: etcd registration must occur after loading and warm-up are complete.
- C++ nodes: the externally routable point-in-time must come after node initialization is complete.
- Kubernetes: readiness probe and registration timing must have consistent semantics; a prolonged "registered but not ready" window is prohibited.

### 15.6 To Be Implemented

- [ ] Produce a unified "service readiness gating" checklist (shared by Go/C++).
- [ ] Add readiness health checks and startup-phase metric instrumentation for critical services.
---

This document serves as the unified design baseline for cross-server architecture, replacing scattered conversation conclusions. Future changes should be applied directly to this document to avoid information loss from "discussed in chat but never written down."