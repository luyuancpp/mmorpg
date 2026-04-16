# Gate-Scene Connection Explosion & Solution

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

## Recommended Solution: Gate Per Zone + Client Gate-Switching

### Core Idea

Gates are grouped by zone. Each Gate only connects to Scenes in its own zone. When a player crosses zones, the **client switches to a Gate in the target zone**.

```
Zone A:  Gate-A1, Gate-A2  <-->  Scene-A1 ~ Scene-A20   (40 x 20 = 800 connections)
Zone B:  Gate-B1, Gate-B2  <-->  Scene-B1 ~ Scene-B20   (40 x 20 = 800 connections)

Gate-to-Scene connections are ALWAYS local to the zone. Connection explosion eliminated.
```

### Why This Works

The existing codebase **already supports Gate switching** (Different-Gate Reconnect, Scenario B):

```
1. Client -> Gate B (TCP) -> Login (gRPC EnterGame)
2. player_locator: update gate_id = B, session_version++
3. Kafka -> gate-{B}: BindSession
4. SceneManager.EnterScene -> Kafka -> gate-{B}: RoutePlayer
5. Gate B -> Scene (gRPC, in-zone direct connect)
```

### Cross-Zone Flow

```
Player is in Zone A, wants to enter a scene in Zone B:

1. Client -> Gate-A -> SceneManager: EnterScene(scene_in_zone_b)

2. SceneManager detects target Scene is in Zone B, current Gate is in Zone A
   -> Does NOT directly RoutePlayer
   -> Triggers gate redirect flow

3. Login/SceneManager assigns a Zone B Gate:
   AssignGate(zone_id = B) -> {gate_b_ip, gate_b_port, token}

4. Gate-A sends RedirectToGate {ip, port, token} to client
   (Client sees: server transfer / loading screen)

5. Client disconnects Gate-A, connects Gate-B, verifies token

6. Standard Scenario B reconnect flow:
   Login.EnterGame -> player_locator update -> BindSession -> RoutePlayer
   Gate-B direct-connects Zone B Scene -> player enters scene
```

### What Needs to Change

1. **Proto**: Add `RedirectToGate` message (Gate -> Client)
2. **SceneManager**: When cross-zone, trigger Gate redirect instead of direct RoutePlayer
3. **Client**: Handle `RedirectToGate` -> disconnect -> connect new Gate -> normal login
4. **Gate etcd registration**: Include zone_id label (for zone-filtered discovery)

### What Does NOT Change

- Gate -> Scene connection logic (still in-zone lazy gRPC direct connect)
- Kafka control plane
- No new node types needed
- Token verification flow (reuse existing HMAC-SHA256 mechanism)

### Player Experience

- Cross-zone: 1~2s loading screen (same as WoW cross-realm instances, FF14 World Visit)
- In-zone: zero impact, no additional latency

## Alternative: Relay Node (Not Adopted)

An intermediate stateless forwarding layer between Gate and Scene.

```
                    +----------+
  40,000 Gates ---- |  Relay   | ---- 2,000 Scenes
  (each -> ~3)      |  Nodes   |  (each Relay -> all Scenes)
                    |  (~50)   |
                    +----------+

Connections: 40,000 x 3 + 50 x 2,000 = 220K
```

- **Pros**: Client doesn't notice cross-zone transitions
- **Cons**: Higher architecture complexity, persistent forwarding overhead, new node type required
- **Not adopted because**: Gate-switching is simpler, reuses existing infrastructure, zero runtime overhead

## Comparison

| Approach | Latency | Connections | Complexity | Player Experience |
|----------|---------|-------------|-----------|-------------------|
| **Client Gate-Switch (recommended)** | Cross-zone: one-time 1~2s | Per-zone ~thousands | Low | Loading screen on cross-zone |
| Relay Node | Per-message +0.1~0.5ms | ~220K | Medium | Seamless |
| Kafka data plane | +2~10ms | 0 | Low | Non-realtime only |

## Integration with Existing Architecture

### Compatibility
- Consistent with **cross-server architecture principle**: Scene doesn't care about player origin
- Reuses **existing Gate-switch flow** (Scenario B: Different-Gate Reconnect)
- Reuses **existing AssignGate with zone_id** parameter (`getgatelistlogic.go`)
- Reuses **existing token mechanism** (HMAC-SHA256 sign/verify)
- Control plane stays on Kafka, unchanged

### Data Flow

```
[In-Zone]
Player -> Gate -> Scene (gRPC direct)
                   |
                   | gRPC (Data Service)
                   v
                 Redis (home realm)

[Cross-Zone Transition]
Player -> Gate-A -> SceneManager: "target is Zone B"
                         |
                         v
                    AssignGate(zone_id=B) -> token
                         |
                         v
Gate-A -> Client: RedirectToGate {ip, port, token}
Client disconnects Gate-A
Client connects Gate-B -> token verify -> EnterGame -> BindSession -> RoutePlayer
Gate-B -> Scene-B (gRPC direct, in-zone)

[Control Plane]
SceneManager/Login -> Kafka topic gate-{id} -> Gate
```

### Connection Count

| Scale | Direct Mesh (no zoning) | Gate-Per-Zone (recommended) |
|-------|------------------------|----------------------------|
| 100 Gate x 50 Scene (1 zone) | 5,000 | 5,000 (same, single zone) |
| 1,000 Gate x 200 Scene (10 zones) | 200,000 | 100 x 20 x 10 = 20,000 |
| 40,000 Gate x 2,000 Scene (100 zones) | 80,000,000 | 400 x 20 x 100 = 800,000 |

## Key Files

| File | Purpose |
|------|---------|
| `go/login/internal/logic/pregate/getgatelistlogic.go` | AssignGate (already supports zone_id) |
| `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` | Gate token verify + message routing |
| `cpp/libs/engine/core/node/system/grpc_channel_cache.h` | Existing channel pooling |
| `cpp/nodes/gate/main.cpp` | Gate startup (Kafka consumer + gRPC direct) |
| `go/scene_manager/` | SceneManager (add cross-zone redirect logic) |

## TODO (Implementation)

- [ ] Proto: define `RedirectToGate` message (Gate -> Client)
- [ ] SceneManager: detect cross-zone and trigger gate redirect flow
- [ ] Gate: send `RedirectToGate` to client when instructed
- [ ] Client: handle `RedirectToGate` (disconnect + reconnect + re-login)
- [ ] Gate etcd registration: ensure zone_id label is present
- [ ] Integration test: cross-zone scene entry via gate switch
