# BroadcastToPlayers 消息体积分析

**日期**: 2026-04-19  
**背景**: uint64→uint32 优化后，单条广播仍观测到 2292 字节

2026-10-15 按 `6281e4d27` 核对传输：C++ Scene → Gate 使用 muduo TCP RPC。
本文体积对比是原测量记录；当前单玩家目标还带身份栅栏，不能直接把旧体积当作当前整包大小。

## 观测数据

```
method: BroadcastToPlayers, msg_id: 32, size: 2292, body(15925)
session_list: ~750 个 uint32 session ID (范围 262145–263149)
message_content: msg_id=70, serialized_message ~20 bytes
```

## 体积构成

| 部分 | 大小 | 占比 |
|------|------|------|
| `session_list` (~750 个 uint32 varint, 每个 ~3 字节) | ~2250 bytes | 98% |
| `message_content` (msg_id=70, payload ~20 bytes) | ~25 bytes | 1% |
| protobuf 开销 (tag, length prefix) | ~17 bytes | 1% |

## 根因

同一 gate 上有 ~750 个玩家在同一场景，广播时逐个列出 `session_id`。消息负载本身只有 25 字节，session 列表占据 98% 的体积。

## 已实现优化

### Phase 1: session_list uint64→uint32

| 优化 | 变更 | 效果 |
|------|------|------|
| session_list uint64→uint32 | proto field type 变更 + 两端适配 | ~50% 减小 (varint ~5 bytes → ~3 bytes) |

### Phase 2: 三级广播体系 (bitmap + scene + all)

#### 1. Bitmap 编码 (自动, count >= 32)
- `session_bitmap_base` (uint32) + `session_bitmap` (bytes)。Bit N 置位 = session (base+N) 在列表中。
- 当前 C++ 发送侧由 `broadcast_targets::Encode` 编码，底层 `EncodeBroadcastSessionList()` 在
  `count >= 32 && bitmapBytes + 6 < count * 3` 时走 bitmap。身份列表按同一目标顺序编码，供 Gate 校验。
- 750 players: 2292 → ~155 bytes (**-93%**)
- 密度盈亏平衡点 ~4.2%，AOI 广播总是远高于此。

#### 2. BroadcastToScene (场景级广播)
- 只发 `scene_id + message_content`，Gate 遍历已连接 session 按 `info.sceneId` 过滤分发。
- 用于场景全员广播（非 AOI 子集），~30 bytes，与玩家数无关。

#### 3. BroadcastToAll (全服广播)
- 只发 `message_content`，Gate 向所有已连接 session 分发。
- 用于系统公告、维护通知等，~25 bytes。

## API 总览

### C++（Scene Node → Gate，muduo TCP RPC）

| 函数 | 传输 | 场景 |
|------|------|------|
| `BroadcastMessageToPlayers(msgId, msg, playerList)` | 每 Gate 一次 muduo TCP RPC | AOI 子集（自动 bitmap） |
| `BroadcastMessageToScene(msgId, msg, sceneId, playerList)` | 每 Gate 一次 muduo TCP RPC | 场景全员 |
| `BroadcastMessageToAll(msgId, msg)` | 向已连接 Gate 发送 muduo TCP RPC | 全服 |

### Go (Service → Gate, Kafka)

| 函数 / Builder | 传输 | 场景 |
|-----------|-----------|----------|
| `kafkautil.BroadcastToPlayers()` + `BuildBroadcastCommand()` | Kafka per gate | 玩家子集 (自动 bitmap) |
| `kafkautil.BroadcastToScene()` + `BuildBroadcastToSceneCommand()` | Kafka per gate | 场景全员 |
| `kafkautil.BroadcastToAll()` + `BuildBroadcastToAllCommand()` | Kafka to all gates | 全服 |

## 关键文件

**Proto:**
- `proto/gate/gate_service.proto` — bitmap fields (3,4) + `BroadcastToSceneRequest` + `BroadcastToAllRequest` + RPCs
- `proto/contracts/kafka/gate_event.proto` — bitmap fields (3,4) + `BroadcastToSceneEvent` + `BroadcastToAllEvent`

**C++ 发送侧 (scene node):**
- `cpp/libs/engine/core/network/player_message_utils.cpp/h` — 三种广播的组包与 `RpcSession::SendRequest`
- `cpp/libs/engine/core/network/broadcast_target_codec.h` — `broadcast_targets::Encode`，与 Gate 共用目标顺序和身份栅栏编码

**C++ Gate 接收侧:**
- `cpp/nodes/gate/handler/rpc/gate_service_handler.cpp` — muduo TCP RPC：`BroadcastToPlayers`（bitmap 解码及身份校验）、`BroadcastToScene`、`BroadcastToAll`
- `cpp/nodes/gate/handler/event/gate_event_handler.cpp` — Kafka: 同上 + `PushToPlayerEventHandler`

**Go 共享工具:**
- `go/shared/kafkautil/gate_push.go` — `GateCommandBuilder` 接口, `BroadcastToScene()`, `BroadcastToAll()`, `EncodeBitmapFields()`

**Go 服务 Builder:**
- `go/guild/internal/kafka/gate_command_builder.go`
- `go/friend/internal/kafka/gate_command_builder.go`

## Session ID 布局

`SessionIdGenerator = TransientNodeCompositeIdGenerator<uint32_t, kNodeBits=17>`:
- 高 15 位 = gate node_id，低 17 位 = sequence（序列按 17 位回绕；分配器另查活跃会话避免复用在用 ID）
- 同一 gate 上所有 session 共享高 15 位 → bitmap span 受实际 seq 分配限制
- bitmap 最坏情况: 131071/8 = ~16KB, 但 `EncodeBroadcastSessionList` 会自动退回 list
