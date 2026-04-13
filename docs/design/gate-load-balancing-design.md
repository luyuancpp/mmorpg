# Gate Load Balancing Design / Gate 负载均衡设计

**Date / 日期:** 2026-03-29

---

## Q&A 记录

### Q1: 怎么让客户端负载均衡地选 Gate？游戏中选择服务器的时候连 Gate 负载均衡一般怎么做？

游戏行业里"选服→连 Gate"的负载均衡常见三种模式：

| 模式 | 流程 | 适用场景 |
|------|------|----------|
| **Login 分配单个 Gate**（最常见） | Client → Login "我选了XX区" → Login 返回**一个** gate 地址+token → Client TCP 连 gate | 大多数 MMO |
| Client 从列表自选 | Client → Login → 返回 gate list → Client 自己挑 | 仅限受信客户端/测试 |
| L4 LB 透传 | Client → NLB/HAProxy → 转发到后端 gate | 所有 gate 无状态、可互换时 |

**推荐做法（Login 分配模式）：** 生产环境应该让 Login 做决策，只返回一个 gate 地址，而不是让客户端拿到列表自己选。原因：

1. **防作弊** — 客户端拿到列表可以故意挤满某个 gate 或绕过限流
2. **连接令牌** — Login 分配 gate 的同时签发一个一次性 token，gate 验证后才允许连接，防伪造/重放
3. **Zone 感知** — 玩家选的是"区服"，一个 zone 可能对应多个 gate，Login 按负载选

### Q2: L4 LB 透传模式能用在这个架构里吗？

**不能。** 我们的 Gate 是**有状态的**：

- `SessionInfo` 持有 `playerId`、节点绑定（`SetNodeId`）、`sessionVersion`
- Session 绑定了具体的 Scene/Login 节点 — 换一个 Gate 没有这个 session 上下文
- 断线通知依赖 `session_id`，必须由同一个 Gate 发出
- Kafka 控制命令路由到 `gate-{gate_id}` — Login/SceneManager 用 gate_id 定向推送

如果 HAProxy 把同一个玩家的重连分到另一个 Gate，那个 Gate 上没有这个 session，所有后续消息都会被丢弃。

| 对比 | 我们的 Gate | L4 LB 要求 |
|------|-----------|-----------|
| 连接状态 | SessionInfo + 节点绑定 | 无 |
| 定向推送 | Kafka `gate-{id}` | 不需要 |
| 断线处理 | Gate 通知 Login | 由 LB 或后端自行感知 |
| 重连 | 必须回到同一 Gate | 任意 Gate 均可 |

L4 LB 只适合无状态网关（如 REST API gateway），不适合 MMO 长连接 Gate。

**结论：Login 分配 + HMAC token 是正确选择。**

---

## Pattern Comparison

| Pattern | Flow | When to Use |
|---------|------|-------------|
| **Login assigns single Gate** (chosen) | Client → Login `AssignGate(zone_id)` → Login returns 1 gate addr + HMAC token → Client TCP connects | Stateful gate (MMO standard) |
| Client picks from list | Client → Login → gate list → Client self-selects | Trusted client / test only |
| L4 LB transparent proxy | Client → NLB/HAProxy → any backend gate | All gates stateless & interchangeable |

## Why L4 LB Doesn't Fit This Architecture

Our Gate is **stateful**:
- `SessionInfo` holds `playerId`, node bindings (`SetNodeId`), `sessionVersion`
- Session is bound to specific Scene/Login nodes — another Gate has no session context
- Disconnect notification depends on `session_id` from the originating Gate
- Kafka control commands route to `gate-{gate_id}` — Login/SceneManager push to a specific gate

If an L4 LB routes a reconnect to a different Gate, that Gate has no session → all messages silently dropped.

L4 LB only works when gates are fully stateless (e.g., REST API gateway doing pure protocol translation with no per-connection state). Incompatible with MMO long-connection architecture.

## Chosen Design: Login Assigns + HMAC Token

### Requirements Satisfied
1. **Anti-cheat**: Client never sees the gate list. Login decides which gate.
2. **Connection token**: Login signs `GateTokenPayload` (HMAC-SHA256). Gate verifies before allowing any game messages.
3. **Zone-aware**: `AssignGate(zone_id)` filters gates by zone, picks least-loaded.

### Flow
```
Client                    Login                    Gate
  │  gRPC AssignGate(zone_id)  │                      │
  │──────────────────────────>│                      │
  │                           │ pick least-loaded gate│
  │                           │ sign HMAC token       │
  │  {ip, port, token, deadline}                      │
  │<──────────────────────────│                      │
  │                                                   │
  │  TCP connect                                      │
  │──────────────────────────────────────────────────>│
  │  ClientTokenVerifyRequest{payload, signature}     │
  │──────────────────────────────────────────────────>│
  │                           verify HMAC + gate_node_id + expiry
  │  ClientTokenVerifyResponse{success=true}          │
  │<──────────────────────────────────────────────────│
  │  ClientRequest (game messages now allowed)        │
  │──────────────────────────────────────────────────>│
```

### Token Details
- **Payload**: `GateTokenPayload { gate_node_id, zone_id, expire_timestamp }` (protobuf)
- **Signature**: HMAC-SHA256(shared_secret, serialized_payload) → hex string
- **TTL**: 60 seconds (configurable in Login)
- **Shared secret**: `GateTokenSecret` in Login YAML / `gate_token_secret` in C++ `BaseDeployConfig`
- **Dev mode**: Empty secret → Gate auto-verifies all connections (no token required)

### Gate Enforcement
- `SessionInfo.verified` flag (default `false`)
- `DispatchTokenVerify`: validates HMAC, gate_node_id, expiry → sets `verified = true`
- `DispatchClientRpcMessage`: rejects + disconnects unverified sessions
- Dev mode: `HandleConnectionEstablished` auto-sets `verified = true` when secret is empty

### Files Touched
- `proto/login/login.proto` — `AssignGate` RPC, `AssignGateRequest/Response`
- `proto/common/base/message.proto` — `GateTokenPayload`, `ClientTokenVerifyRequest/Response`
- `proto/common/base/config.proto` — `gate_token_secret` field
- `go/login/internal/logic/pregate/` — `AssignGate` logic with HMAC signing
- `go/login/internal/config/config.go` — `GateTokenSecret` field
- `go/login/etc/login.yaml` — `GateTokenSecret` config
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp` — token verify handler + enforcement
- `cpp/libs/services/gate/session/comp/session_info_comp.h` — `verified` flag
- `bin/etc/base_deploy_config.yaml` — `GateTokenSecret` config
- `robot/main.go` — `assignGate()` + token handshake
- `robot/pkg/client.go` — `VerifyGateToken()` method
