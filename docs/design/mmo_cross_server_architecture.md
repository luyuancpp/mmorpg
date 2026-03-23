# MMO 跨服架构设计（完整沉淀）

## 1. 背景与目标

- 游戏形态：MMO（魔兽类），约 1000+ 区服（Zone）。
- 玩家归属：每个玩家有归属服（home zone），但可自由访问其他服场景。
- 当前约束：每个大区/区域（Region）有独立 Redis 集群。
- 设计优先级：稳定性与正确性优先，切场额外延迟可接受。

核心目标：

- 跨服能力对业务逻辑层“透明”。
- 不做玩家数据跨服搬迁。
- 让跨服复杂度收敛到少数基础组件，避免扩散到 Scene/Gate 业务代码。

## 2. 总原则：位置透明（Location Transparency）

> 无论玩家当前在哪个服，逻辑层处理方式都应与“本服玩家”完全一致。

落地规则：

- 业务层禁止出现 `is_cross_server` 分支。
- Scene/Gate 不关心玩家归属服或 Redis 拓扑。
- 玩家数据永远保存在归属服（home zone）的存储中。
- 路由变化、跨服策略调整仅在路由层/数据服务层生效。

反模式（历史问题，明确禁止）：

- 把数据复制到目标服（带来一致性灾难）。
- 在登录/切场/技能/背包等逻辑中到处判断“是否跨服”。
- 在每个 Scene 节点内自行维护跨服 Redis 连接与路由规则。

## 3. 架构选择：方案 A（独立数据代理服务）

### 3.1 结论

采用独立 Data Service（Go/go-zero）作为跨服数据代理层，统一承接 Scene 的读写请求并内部路由到归属服存储。

### 3.2 为什么选 A，不选 B（Scene 内嵌路由）

- 若每个 Scene 直连全量区服 Redis，会出现连接数爆炸。
- 路由逻辑分散在所有 Scene 节点，升级和排障成本高。
- 方案 A 将复杂度集中到一个可水平扩展的服务层，更利于稳定运行与运维治理。

### 3.3 逻辑拓扑

```text
Player -> Gate -> Scene (任意目标服)
                   |
                   | gRPC（统一接口，服务端无感跨服）
                   v
              Data Service
                   |
                   | player_id -> home_zone_id -> region -> redis
                   v
             Home Redis/DB
```

## 4. 控制面与数据面拆分

### 4.1 控制面（玩家路由/切场调度）

- Centre / SceneManager / PlayerLocator 负责“玩家当前在哪个 Scene 节点”。
- Gate 负责客户端连接承载与转发。
- Gate 与 SceneManager 之间使用 Kafka 控制消息，避免大规模 gRPC 长连接网状拓扑。

当前约定：

- Kafka Topic：`gate-{gate_id}`。
- 命令协议：`GateCommand`（例如 `RoutePlayer`、`KickPlayer`）。
- 安全字段：`target_instance_id`，用于过滤僵尸消息（节点重启后旧消息误投）。

### 4.2 数据面（玩家数据读写）

- Scene 只调用统一数据接口（如 `GetPlayerBag(player_id)`），不感知跨服。
- Data Service 负责：
  - `player_id -> home_zone_id` 映射解析。
  - `home_zone_id -> region_id -> redis_cluster` 路由。
  - 重试、熔断、降级、观测、热点缓存。

## 5. 区服分层与锁区规则

- Zone：游戏逻辑服（玩家创建角色与长期归属的“服”）。
- Region：Zone 的上层分组（运维与存储分配单位）。
- 存储策略：Redis 按 Region 分配，而非按单 Zone 一服一 Redis。

锁区（Lock Region）规则：

- 锁区开启：玩家仅可访问归属 Region 内 Zone。
- 锁区关闭：玩家可访问任意 Zone。
- 锁区判定归 SceneManager（控制面），不放在 Data Service（数据面）。

## 6. 玩家 ID 与映射表设计

### 6.1 Player ID 规则

- 使用现有 Snowflake：`[time:32][node_id:17][step:15]`。
- 禁止将 `zone_id` 编入 `player_id`。
- `node_id` 仅表示“生成 ID 的物理节点”，不是区服语义。

原因：

- 合服时若 ID 含 zone 语义，会导致系统性重写风险。
- 玩家 ID 必须全生命周期不变（跨服、合服、迁移都不改）。

### 6.2 独立映射表

- `player_id -> home_zone_id`（全局真值映射）。
- `home_zone_id -> region_id`（配置映射）。
- `region_id -> redis_addr`（路由配置）。

建议存储：

- `player_id -> home_zone_id` 放 Global Redis 或 MySQL+本地缓存。
- 注册角色时写入映射，作为后续全部数据路由依据。

## 7. 跨服切场时序（关键一致性约束）

核心不变量：同一玩家同一时刻只能有一个 Scene 写入其数据（Single Writer）。

标准时序：

1. SceneManager 通知旧 Scene：释放玩家并落盘。
2. 旧 Scene 确认“已保存并释放”。
3. SceneManager 才通知新 Scene：加载玩家。
4. 若步骤 2 超时，则中止迁移，不允许新 Scene 先行加载。

该时序保证：

- 避免两边 Scene 并发写同一玩家。
- 避免切场过程中的状态覆盖与回滚困难。

## 8. 一致性防线（分层兜底）

第一层（主防线）：

- SceneManager 串行化切换流程，保障 Single Writer。

第二层（异常兜底）：

- Data Service 对玩家键做短期分布式锁（例如 Redis `SETNX` + TTL 约 3 秒）。

第三层（最终保险）：

- 关键数据引入版本号（乐观锁）或事务检测（WATCH/MULTI）。

说明：Redis 单线程只保证同 key 单连接 FIFO；真正风险来自上游并发写入，因此必须把 Single Writer 放在架构层保证。

## 9. 合服策略（不改玩家 ID）

策略 A（推荐）：

- 将目标服数据迁入主服存储，批量更新 `player_id -> home_zone_id` 映射。

策略 B（过渡）：

- 将多个 Zone 路由到同一 Redis 集群，逐步完成数据合并。

共同点：

- 都不修改 `player_id`。
- 业务代码（Scene/Gate）无需改动。

## 10. NodeId 冲突与快速恢复设计

### 10.1 问题定义

etcd lease 过期重注册窗口可能导致同一 `node_id` 双活（旧节点恢复网络 + 新节点已接管）。

风险：

- 双活处理玩家请求。
- Snowflake 同 `node_id` 并发发号冲突。

### 10.2 冲突时处理要求

当节点确认“自己的 `node_id` 已被其他 uuid 占用”时，必须执行：

1. 立即停止接受新业务请求。
2. 尽快 flush 在线玩家数据。
3. 踢出或迁移在线玩家（按节点类型策略）。
4. 自杀退出，避免双活持续。

节点类型策略：

- SceneNode：存档后迁移到同 zone 的可用节点。
- GateNode：断开客户端并触发重连路由。
- InstanceNode：副本态不可平移，直接通知副本失败并踢出。

### 10.3 Snowflake 冲突防护

- keepalive 同步写 `snowflake_guard:{zone}:{type}:{node_id}`（TTL 600s）。
- 新节点接管后 `SetGuardTime(now_utc)`，跳过当前秒发号窗口。
- 不做“等待安全窗口”延迟启动，保障灰度/重连恢复速度。

## 11. 与当前工程的集成边界

- Gate：保留 Kafka 控制面消费者，按 `gate-{id}` 接收 SceneManager 命令。
- SceneManager：负责切场编排、锁区策略、Single Writer 时序执行。
- PlayerLocator：记录玩家当前 Scene 位置，可扩展携带 home_zone 信息。
- Data Service：可扩展现有 `go/db/` 或新建 `go/data_service/`。

## 12. 必须长期遵守的设计红线

1. 任何业务模块都不允许引入“跨服模式分支”。
2. 玩家数据不做跨服复制作为常态手段。
3. 所有跨服路由真值必须集中在数据代理层。
4. 任何新特性若需要关心“玩家来自哪个服”，应先重审架构。
5. 稳定性高于延迟；正确性高于局部性能。

## 13. 后续实施清单

- 完成 Node 子类 `OnNodeIdConflictShutdown` 的差异化实现（Scene/Gate/Instance）。
- 在优雅停机流程加入：存档玩家 -> 迁移/踢出 -> 主动注销节点键。
- Data Service 增加 per-player lock 与关键写入版本字段。
- 建立跨服切场链路观测：切场阶段耗时、失败原因、回滚计数。
## 14. 跨场景玩家消息投递（Cross-Scene Player Messaging）

### 14.1 问题

Scene A 需要给玩家发消息（如任务奖励、交易结果、系统通知），但玩家可能不在 Scene A 上。
旧方案通过 Centre 转发，Centre 成为瓶颈且流程复杂。新方案基于 Kafka 投递，但引入了路由窗口期一致性问题。

### 14.2 两种场景

**场景 1：玩家不在本 Scene（跨场景投递）**

- 通过 Kafka 将消息投递到玩家当前所在 Scene 的 topic。
- 关键边界：消息已入 Kafka（目标 Scene B），路由过程中玩家从 B 切换到 C → Scene B 收到时玩家已不在。

**场景 2：玩家在本 Scene（本地投递）**

- 本地场景可直接处理，也可统一走玩家消息队列。

### 14.3 消息重要性分级（Protobuf Option 标记）

在 RPC service 方法定义上通过 `option` 标记消息重要性，代码生成层据此生成不同的投递策略：

| 级别 | 含义 | 投递保证 | 路由失败处理 |
|------|------|---------|-------------|
| IMPORTANT | 交易结果、奖励发放、状态变更等 | at-least-once | 目标 Scene 发现玩家已离开 → 重查 PlayerLocator → 转投新 Scene；若仍失败 → 落库/Redis 持久化，玩家下次进场景补发 |
| NORMAL | 一般通知、非关键 UI 提示 | best-effort | 目标 Scene 发现玩家不在 → 直接丢弃 |

示例 proto option 定义（待细化）：

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

### 14.4 本地投递策略

**方案 A：直接处理（同步路径）**

- 延迟最低，适合移动同步等高频低延迟消息。
- 缺点：本地 / 跨场景两条路径逻辑不一致，消息顺序无法统一保证。

**方案 B：统一走玩家消息队列（异步路径）**

- 所有发给玩家的消息（不论本地还是跨场景）都入玩家消息队列，由统一消费逻辑处理。
- 优点：符合"位置透明"原则，逻辑一致，消息有序。
- 缺点：本地消息多一次队列中转开销。

**结论**：默认走统一消息队列（方案 B），与跨服"位置透明"原则一致。对移动同步等超低延迟场景，可标记走直接路径（方案 A）作为例外。

### 14.5 投递流程

```text
发送方 Scene A
    |
    |-- 查 PlayerLocator：玩家在哪？
    |
    +-- 在本 Scene A --> 入本地玩家消息队列 --> 统一处理
    |
    +-- 在 Scene B --> Kafka 投递到 scene-{scene_b_id} topic
                          |
                          v
                       Scene B 消费消息
                          |
                          +-- 玩家在 --> 入本地玩家消息队列 --> 统一处理
                          |
                          +-- 玩家已离开
                                |
                                +-- IMPORTANT --> 重查 PlayerLocator --> 转投新 Scene / 落库补发
                                |
                                +-- NORMAL --> 丢弃
```

### 14.6 与现有架构的关系

- 复用 Kafka 基础设施：与 Gate 控制面共享 Kafka 集群，新增 `scene-{scene_id}` topic。
- PlayerLocator 查询：复用已有 `player_locator` 服务获取玩家当前位置。
- 单写者保证：消息投递不破坏 Single Writer（§7），消息只在玩家当前所在 Scene 被消费处理。
- 持久化兜底：重要消息落库复用 Data Service（§3）的统一数据通道。

### 14.7 待实施

- [ ] 定义 `MessagePriority` protobuf option 并集成到 proto-gen（历史名 pbgen）代码生成流程。
- [ ] 实现 Scene 侧玩家消息队列（ECS Comp + 消费 System）。
- [ ] Scene Kafka Consumer 增加 `scene-{scene_id}` topic 订阅。
- [ ] 重要消息失败后的重试 / 落库补发逻辑。
- [ ] 确定 Kafka topic 分区策略（按玩家 hash 还是按场景 ID）。

## 15. 无状态微服务的就绪门控

### 15.1 问题

在分布式游戏后端中，服务常常需要先完成数据加载（配置表、缓存预热、依赖连接检查）才能安全处理请求；
但微服务又希望保持无状态与可弹性扩缩。

### 15.2 原则

- 就绪状态外置：把"可接流量"判断交给服务发现与编排层，不把复杂等待状态耦合进业务流程。
- 调用方只发现 ready 实例：未就绪实例不应进入服务发现可见集合。

### 15.3 推荐启动顺序

```text
启动进程
  -> 加载配置/静态数据
  -> 预热缓存与依赖连通性检查
  -> 打开服务端口（可选）
  -> 注册服务发现（etcd）
  -> 切换 readiness=ready
```

### 15.4 依赖等待策略

- 强依赖链路：watch 服务发现（etcd）并在目标实例 ready 后恢复业务流。
- 弱依赖链路：快速失败 + 指数退避重试，避免全链路阻塞。

### 15.5 项目落地约定

- Go 服务：etcd 注册应晚于加载与预热完成。
- C++ 节点：对外可路由时机应晚于节点初始化完成。
- Kubernetes：readiness probe 与注册时机保持一致语义，禁止出现"已注册但未 ready"的长期窗口。

### 15.6 待实施

- [ ] 形成统一"服务就绪门控"检查清单（Go/C++ 共用）。
- [ ] 为关键服务补齐 readiness 健康检查与启动期指标埋点。
---

本文件用于替代零散会话结论，作为跨服架构的统一设计基线。后续改动应直接更新本文件，避免“只在会话里说、不进文档”的信息丢失。