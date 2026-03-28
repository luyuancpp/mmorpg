# Centre Decentralisation Migration Plan (2026-03-15)

## Overview
## Latest Decisions
- 2026-03-15: The `request_id` reconnect / idempotency optimisations related to `centre` are frozen — no further changes under `cpp/nodes/centre/**`; they will be migrated together with the `centre` removal path to Login + player_locator.


Centre essentially does 6 things, each of which can be distributed to existing, already multi-instance components.
Goal: no single-instance component at any level — every layer is multi-instance or clustered.

---

## Centre Current Responsibilities → Decentralisation Migration Plan

### 1. Session Map (session → player routing)

| Current State | Centre maintains `SessionMap[session_id] → player_id` in memory; Gate depends on it |
|------|------|
| **Migration** | **Gate local maintenance** — Gate already knows which sessions it holds; no centralised lookup needed. Cross-Gate lookups go through `player_locator` (already exists, multi-instance + Redis) |
| **Single point?** | No — Gates are independent; player_locator is already multi-instance |

### 2. Login Decision Logic (FirstLogin / Reconnect / ReplaceLogin)

| Current State | Centre's `DecideEnterGame()` makes the decision |
|------|------|
| **Migration** | Move to **Login service** (Go, already multi-instance). Login queries `player_locator` for current player state → makes decision → notifies Gate via Kafka to bind / kick |
| **Single point?** | No — Login is already stateless multi-instance |

### 3. Scene Registration & Scene-Switch Orchestration

| Current State | Scene calls `RegisterScene` on Centre; scene switches are coordinated through Centre |
|------|------|
| **Migration** | Scene registration → **write directly to etcd** (already in progress). Scene-switch orchestration → **SceneManager** (Go, multi-instance cluster under construction), issues `RoutePlayer` / `KickPlayer` via Kafka |
| **Single point?** | No — SceneManager is partitioned by player_id (consistent hash), multiple instances back each other up |

### 4. Cross-Node Message Routing (RoutePlayerStringMsg / RouteNodeStringMsg)

| Current State | Centre acts as message relay |
|------|------|
| **Migration** | **Kafka topic direct delivery** — sender queries `player_locator` for target scene/gate → publishes to `scene-{scene_id}` or `gate-{gate_id}` topic. Consistent with the cross-scene-player-messaging design |
| **Single point?** | No — Kafka RF≥3 |

### 5. Disconnection Delayed Cleanup (30 s reconnect window)

| Current State | Centre maintains a `DelayedCleanupTimer` |
|------|------|
| **Migration** | **player_locator lease mechanism** — Gate detects disconnect → notifies player_locator to mark "disconnecting" + 30 s TTL. Reconnect within TTL → lease renewed; expired → player_locator fires a cleanup event (Kafka notification to relevant Scenes) |
| **Single point?** | No — player_locator multi-instance + Redis TTL |

### 6. Request Idempotency (in-memory request_id dedup)

| Current State | Centre in-memory map, 5-minute TTL |
|------|------|
| **Migration** | **Redis SET with TTL** (`request:{player_id}:{request_id}` → SETEX 300 s). Whoever processes the request checks it — no centralised component needed |
| **Single point?** | No — Redis is inherently distributed |

---

## Post-Migration Architecture

```
Client → Gate (multi-instance, each maintains local sessions)
           │
           ├─ Login (Go, multi-instance, stateless)
           │    └─ Queries player_locator for login decisions
           │    └─ Kafka notifies Gate to bind / kick
           │
           ├─ SceneManager (Go, multi-instance, hash-partitioned)
           │    └─ Orchestrates scene switches
           │    └─ Kafka issues RoutePlayer / KickPlayer
           │
           ├─ player_locator (Go, multi-instance + Redis)
           │    └─ Player location source of truth
           │    └─ Disconnect lease / reconnect window
           │
           ├─ Kafka (RF≥3)
           │    └─ All control-plane messages
           │
           └─ etcd (3/5 nodes)
                └─ Service discovery & scene registration
```

**No single-instance component exists. Every layer is multi-instance or clustered.**

---

## Migration Sequence

| Phase | Action | Risk | Status |
|------|------|------|------|
| **Phase 1** | Scene registration goes directly through etcd (mostly done); SceneManager takes over scene switching (in progress) | Low — runs in parallel with existing path | ~90 % complete (proto/RPC/Gate Kafka in place, remaining pbgen re-run) |
| **Phase 2** | Login decision moves to Login service + player_locator; disconnect cleanup switches to lease model | Medium — requires dual-write verification | player_locator service implemented (7 RPCs + lease monitor), pending Login integration |
| **Phase 3** | Cross-node message routing switches to Kafka direct delivery; idempotency moves to Redis | Low — purely incremental | Not started |
| **Phase 4** | Centre demoted to read-only observation node (no decision-making); dual-path verification runs | Low | Not started |
| **Phase 5** | Gradual Centre shutdown; emergency rollback switch retained for one release cycle | Low — rollback guarantee | Not started |

## Phase 2 Priority Analysis

Phase 2 should start with **player_locator** because it is a foundational dependency for multiple subsequent tasks:

| Dependent | Needs player_locator |
|---|---|
| Login decision (FirstLogin / Reconnect / Replace) | Yes — Login queries it to make decisions |
| Disconnect lease cleanup | Yes — TTL lives here |
| Cross-node Kafka direct routing (Phase 3) | Yes — sender resolves target via locator |

### What player_locator Needs to Implement
1. **Redis-backed player state store**: `player:{player_id}` → `{gate_node_id, gate_instance_id, scene_node_id, scene_instance_id, status, home_zone_id}` with TTL support
2. **gRPC API**: `Register`, `Unregister`, `Locate`, `UpdateStatus` (+ lease renew / expire)
3. **Lease mechanism**: Gate disconnect → mark "disconnecting" + 30 s TTL; reconnect → renew lease; expired → publish `PlayerLeaseExpiredEvent` to Kafka
4. **Redis keyspace notifications** (or polling) to detect TTL expiration and trigger Kafka events
