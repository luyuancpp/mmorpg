# Gate-Scene Relay Architecture (Gate-Scene 连接爆炸与 Relay 架构)

## Problem

全球同服 MMO 下，Gate 和 Scene 之间的 gRPC 直连会导致连接爆炸：

- 40,000 Gate x 2,000 Scene = 8000万连接 (worst case full mesh)
- 即使使用 lazy 按需建连 + gRPC channel pooling，跨区交互频繁时连接 fan-out 仍趋向 N x M
- 控制面已用 Kafka 解决 (RoutePlayer/KickPlayer)，但数据面 (玩家实时消息) 需要低延迟，不能全走 Kafka

## Current State

| Channel | Method | Connection Scale |
|---------|--------|-----------------|
| Control plane | Kafka (RoutePlayer/KickPlayer) | No direct connections, solved |
| Data plane | gRPC (player realtime messages) | Gate -> Scene lazy connect, worst case -> N x M |

### Existing Optimizations
- **gRPC channel pooling**: `grpc_channel_cache.h` -- one channel per (ip, port), weak_ptr auto-cleanup
- **Lazy connect**: connections created on first use, not pre-established full mesh
- **Single-zone scenario**: works fine -- one Gate's players typically spread across only dozens of Scenes

### Why It Breaks at Global Scale
- Cross-zone interaction means Gate-A (zone 1) players may enter Scene-X (zone 5)
- More cross-zone interaction = larger connection fan-out per Gate
- Worst case: every Gate connects to every Scene = N x M

## Solution: Relay Node

### Architecture

```
                    +----------+
  40,000 Gates ---- |  Relay   | ---- 2,000 Scenes
  (each -> ~3)      |  Nodes   |  (each Relay -> all Scenes)
                    |  (~50)   |
                    +----------+

Connections: 40,000 x 3 (redundancy) + 50 x 2,000 = 220K  vs  80M
```

### Relay Node Properties
- **Stateless**: pure message forwarding, holds no player data
- **Horizontally scalable**: assign by region/hash, add/remove dynamically
- **Latency impact**: +0.1~0.5ms within same datacenter, acceptable for MMO
- **Connection management**: each Relay maintains long-lived connections to all Scenes; Gates connect to only a few Relays

## Recommended: Hybrid Architecture

```
In-zone:   Gate --gRPC direct--> Scene         (connections under control, lowest latency)
Cross-zone: Gate --gRPC--> Relay --gRPC--> Scene  (connections: N x K + K x M)
Control:   All via Kafka                        (already implemented)
```

### Routing Decision

Gate determines local vs cross-zone by Scene ID:
- **Local Scene** -> reuse existing lazy gRPC direct connection
- **Cross-zone Scene** -> route through Relay

Most gameplay (in-zone) has zero additional latency; only cross-zone interaction adds one hop.

## Alternative Comparison

| Approach | Latency | Connections | Complexity | Use Case |
|----------|---------|-------------|-----------|----------|
| **Relay Node** | +0.1~0.5ms | N x K + K x M | Medium | General recommendation |
| **Local direct + cross-zone Relay** | local 0 / cross +0.5ms | Greatly reduced | Medium | **Recommended** |
| **Kafka data plane** | +2~10ms | 0 direct | Low | Non-realtime messages only |
| **Aggressive connection reclaim** | 0 | Peak still high | Low | Cross-zone rare |

## Integration with Existing Architecture

### Compatibility
- Consistent with **cross-server architecture principle**: Scene doesn't care about player origin
- Complementary to **gate-kafka-consumer**: control plane via Kafka, data plane via Relay
- Relay can reuse existing `grpc_channel_cache.h` connection pooling pattern
- Gate routing decision point: `Scene ID -> zone_id -> local/cross-zone -> direct/relay`

### Data Flow

```
[In-Zone]
Player -> Gate -> Scene (gRPC direct)
                   |
                   | gRPC (Data Service)
                   v
                 Redis (home realm)

[Cross-Zone]
Player -> Gate -> Relay -> Scene (gRPC)
                            |
                            | gRPC (Data Service)
                            v
                          Redis (home realm)

[Control Plane]
SceneManager/Login -> Kafka topic gate-{id} -> Gate
```

### Connection Count Example

| Scale | Direct Mesh | Hybrid with 50 Relays |
|-------|------------|----------------------|
| 100 Gate x 50 Scene | 5,000 | 300 + 2,500 = 2,800 |
| 1,000 Gate x 200 Scene | 200,000 | 3,000 + 10,000 = 13,000 |
| 40,000 Gate x 2,000 Scene | 80,000,000 | 120,000 + 100,000 = 220,000 |

## Key Files

| File | Purpose |
|------|---------|
| `cpp/libs/engine/core/node/system/grpc_channel_cache.h` | Existing channel pooling |
| `cpp/libs/engine/core/node/system/node/node_connector.cpp` | Node discovery and connection |
| `cpp/nodes/gate/main.cpp` | Gate startup (Kafka consumer + gRPC direct) |
| `cpp/nodes/scene/main.cpp` | Scene startup (gRPC server + Kafka consumer) |

## TODO (Implementation)

- [ ] Define Relay Node service in proto (pure forwarding service)
- [ ] Implement Relay Node (Go or C++, stateless forwarder)
- [ ] Gate: add routing logic -- local Scene direct, cross-zone via Relay
- [ ] Gate: add Relay discovery via etcd (same pattern as Scene discovery)
- [ ] Relay: connection management to all Scene nodes
- [ ] Load test: verify connection count reduction at scale
- [ ] Relay health check and failover strategy
