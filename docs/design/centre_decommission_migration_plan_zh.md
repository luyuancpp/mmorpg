# Centre 去中心化迁移方案 (2026-03-15)

## 概述
## 最新决策
- 2026-03-15: `centre` 相关的 `request_id` 重连/幂等优化先冻结，不再在 `cpp/nodes/centre/**` 做新增改动；后续随 `centre` 删除路径统一迁移到 Login + player_locator。


Centre 本质上做了 6 件事，每件都可以分散到现有的、已经是多实例的组件上。
目标：无任何单实例组件，每一层都是多实例或集群化。

---

## Centre 当前职责 → 去中心化迁移方案

### 1. Session Map（session → player 路由）

| 现状 | Centre 内存维护 `SessionMap[session_id] → player_id`，Gate 依赖它 |
|------|------|
| **迁移** | **Gate 本地维护** — Gate 自己就知道自己连着哪些 session，不需要中央查表。跨 Gate 查找走 `player_locator`（已有，多实例 + Redis） |
| **单点？** | 否，Gate 各自独立，player_locator 已多实例 |

### 2. 登录决策逻辑（FirstLogin / Reconnect / ReplaceLogin）

| 现状 | Centre 的 `DecideEnterGame()` 做判断 |
|------|------|
| **迁移** | 移到 **Login 服务**（Go，已多实例）。Login 查 `player_locator` 获取玩家当前状态 → 做决策 → 通过 Kafka 通知 Gate 执行绑定/踢人 |
| **单点？** | 否，Login 已是 stateless 多实例 |

### 3. 场景注册 & 场景切换编排

| 现状 | Scene 向 Centre 调 `RegisterScene`；切场景通过 Centre 协调 |
|------|------|
| **迁移** | Scene 注册 → **直接写 etcd**（已经在做）。切场景编排 → **SceneManager**（Go，已在建的多实例集群），走 Kafka 下发 `RoutePlayer` / `KickPlayer` |
| **单点？** | 否，SceneManager 按 player_id 分区（consistent hash），多实例互为备份 |

### 4. 跨节点消息路由（RoutePlayerStringMsg / RouteNodeStringMsg）

| 现状 | Centre 充当消息中转站 |
|------|------|
| **迁移** | **Kafka topic 直投** — 发送方查 `player_locator` 得到目标 scene/gate → 发到 `scene-{scene_id}` 或 `gate-{gate_id}` topic。与 cross-scene-player-messaging 方案一致 |
| **单点？** | 否，Kafka RF≥3 |

### 5. 断线延迟清理（30s reconnect window）

| 现状 | Centre 维护 `DelayedCleanupTimer` |
|------|------|
| **迁移** | **player_locator 的 lease 机制** — Gate 检测到断线 → 通知 player_locator 标记 "disconnecting" + TTL 30s。TTL 内重连 → 续租；过期 → player_locator 触发清理事件（Kafka 通知相关 Scene） |
| **单点？** | 否，player_locator 多实例 + Redis TTL |

### 6. 请求幂等性（in-memory request_id dedup）

| 现状 | Centre 内存 map，5分钟 TTL |
|------|------|
| **迁移** | **Redis SET with TTL**（`request:{player_id}:{request_id}` → SETEX 300s）。谁处理请求谁检查，不需要集中式 |
| **单点？** | 否，Redis 本来就是分布式的 |

---

## 迁移后架构

```
Client → Gate (多实例，各自维护本地session)
           │
           ├─ Login (Go, 多实例, stateless)
           │    └─ 查 player_locator 做登录决策
           │    └─ Kafka 通知 Gate 绑定/踢人
           │
           ├─ SceneManager (Go, 多实例, hash分区)
           │    └─ 编排场景切换
           │    └─ Kafka 下发 RoutePlayer/KickPlayer
           │
           ├─ player_locator (Go, 多实例 + Redis)
           │    └─ 玩家位置真值
           │    └─ 断线 lease / reconnect window
           │
           ├─ Kafka (RF≥3)
           │    └─ 所有控制面消息
           │
           └─ etcd (3/5节点)
                └─ 服务发现 & scene注册
```

**没有任何单实例组件。每一层都是多实例或集群化的。**

---

## 迁移顺序

| 阶段 | 动作 | 风险 | 状态 |
|------|------|------|------|
| **Phase 1** | Scene 注册直接走 etcd（已基本完成）；SceneManager 接管场景切换（已在做） | 低 — 与现有并行 | ~90% 完成（proto/RPC/Gate Kafka 已就位，剩余 pbgen 重跑） |
| **Phase 2** | 登录决策移到 Login 服务 + player_locator；断线清理改 lease 模式 | 中 — 需要双写验证 | player_locator 服务已实现（7 RPCs + lease monitor），待 Login 集成 |
| **Phase 3** | 跨节点消息路由改 Kafka 直投；幂等性移 Redis | 低 — 纯增量 | 未开始 |
| **Phase 4** | Centre 降级为只读观测节点（不参与任何决策）；跑双路径验证 | 低 | 未开始 |
| **Phase 5** | 灰度关闭 Centre；保留紧急回滚开关一个版本周期 | 低 — 有回滚保障 | 未开始 |

## Phase 2 优先级分析

Phase 2 应从 **player_locator** 开始，因为它是多个后续任务的基础依赖：

| 依赖方 | 需要 player_locator |
|---|---|
| Login 登录决策 (FirstLogin/Reconnect/Replace) | 是 — Login 查询它来做决策 |
| 断线 lease 清理 | 是 — TTL 住在这里 |
| 跨节点 Kafka 直投路由 (Phase 3) | 是 — 发送方通过 locator 解析目标 |

### player_locator 需要实现的内容
1. **Redis-backed 玩家状态存储**: `player:{player_id}` → `{gate_node_id, gate_instance_id, scene_node_id, scene_instance_id, status, home_zone_id}` with TTL 支持
2. **gRPC API**: `Register`, `Unregister`, `Locate`, `UpdateStatus` (+ lease renew/expire)
3. **Lease 机制**: Gate 断线 → 标记 "disconnecting" + 30s TTL；重连 → 续租；过期 → 发布 `PlayerLeaseExpiredEvent` 到 Kafka
4. **Redis keyspace notifications**（或 polling）检测 TTL 过期并触发 Kafka 事件
