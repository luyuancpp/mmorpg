# BroadcastToPlayers 消息体积分析

**日期**: 2026-04-19  
**背景**: uint64→uint32 优化后，单条广播仍观测到 2292 字节

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

## 已完成优化

| 优化 | 变更 | 效果 |
|------|------|------|
| session_list uint64→uint32 | proto field type 变更 + 两端适配 | ~50% 减小 (varint ~5 bytes → ~3 bytes) |

## 候选下一步优化方案

### 方案 1: 场景级广播 (推荐)

新增 `BroadcastToScene` RPC，只发 `scene_id` + `message_content`。Gate 侧维护 `scene_id → session_set` 映射，收到后自查表分发。

- **效果**: 2292 → ~30 bytes (**减少 98%**)
- **改动范围**:
  - `proto/gate/gate_service.proto` — 新增 `BroadcastToSceneRequest { uint64 scene_id; MessageContent message_content; }`
  - Gate 侧 `BindSession` / `EnterScene` 时记录 `session → scene_id` 映射
  - `player_message_utils.cpp` — 场景广播走新 RPC
- **优点**: 消息体积与玩家数无关，彻底解决规模问题
- **缺点**: Gate 需维护 scene 映射状态，增加 enter/leave scene 时的同步开销

### 方案 2: Bitmap 编码

保留 session_list 字段，但编码为 `base_session_id` + bitmap。

- **效果**: 2292 → ~130 bytes (**减少 94%**)
- **改动范围**: proto 新增 bitmap 字段 + 两端编解码
- **优点**: 不改架构，纯编码优化
- **缺点**: session ID 分散时退化；编解码复杂度

### 方案 3: 维持现状

2292 bytes 绝对值不大。仅在高频广播场景下有意义。

- 低频广播 (1-5 Hz): ~2-11 KB/s per gate → **可接受**
- 高频 AOI tick (10-30 Hz): ~23-69 KB/s per gate → **值得优化**

## 决策

待评估广播频率后决定。如果只是聊天/系统公告级别 (低频)，方案 3 足够；如果涉及 AOI 位置同步 (高频)，推荐方案 1。
