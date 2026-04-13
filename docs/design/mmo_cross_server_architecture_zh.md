# MMO 跨服架构设计（完整参考）

## 1. 背景与目标

- 游戏类型：MMO（WoW 类），约 1000+ 区服。
- 玩家归属：每位玩家有一个归属区服，但可以自由访问其他区服上的场景。
- 当前约束：每个大区拥有独立的 Redis 集群。
- 设计优先级：稳定性和正确性优先；场景切换时额外延迟可以接受。

核心目标：

- 跨服能力必须对业务逻辑层透明。
- 不进行跨服玩家数据迁移。
- 将跨服复杂度收敛到少量基础组件内，防止泄漏到 Scene/Gate 业务代码中。

## 2. 核心原则：位置透明

> 无论玩家当前在哪台服务器上，逻辑层处理方式应与本地玩家完全一致。

实现规则：

- 业务层绝不可基于 `is_cross_server` 产生分支。
- Scene/Gate 不应关心玩家的归属区服或 Redis 拓扑。
- 玩家数据始终持久化在归属区服的存储中。
- 路由变更和跨服策略调整仅发生在路由/数据服务层。

反模式（历史遗留问题，明确禁止）：

- 将数据复制到目标服务器（一致性噩梦）。
- 在登录、场景切换、技能、背包等各处检查"这是不是跨服？"。
- 每个 Scene 节点自行维护跨服 Redis 连接和路由规则。

## 3. 架构选择：方案 A（独立数据代理服务）

### 3.1 结论

采用独立的 Data Service（Go/go-zero）作为跨服数据代理层，统一处理 Scene 的读写请求并在内部路由到归属区服的存储。

### 3.2 为什么选方案 A 而非方案 B（Scene 内嵌路由）

- 如果每个 Scene 直接连接所有区服的 Redis 实例，连接数会爆炸。
- 路由逻辑分散在所有 Scene 节点中，升级和排障成本高。
- 方案 A 将复杂度集中到可水平扩展的服务层，更有利于稳定运营和运维治理。

### 3.3 逻辑拓扑

```text
Player -> Gate -> Scene（任意目标区服）
                   |
                   | gRPC（统一接口，服务端跨区透明）
                   v
              Data Service
                   |
                   | player_id -> home_zone_id -> region -> redis
                   v
             归属 Redis/DB
```

## 4. 控制面 vs 数据面

### 4.1 控制面（玩家路由/场景调度）

- Centre / SceneManager / PlayerLocator 负责"玩家当前在哪个 Scene 节点上"。
- Gate 负责客户端连接承载和转发。
- Gate 与 SceneManager 通过 Kafka 控制消息通信，避免大规模 gRPC 长连接网状拓扑。

当前约定：

- Kafka Topic：`gate-{gate_id}`。
- 命令协议：`GateCommand`（如 `RoutePlayer`、`KickPlayer`）。
- 安全字段：`target_instance_id`，用于过滤僵尸消息（来自已重启节点的过期消息）。

### 4.2 数据面（玩家数据读写）

- Scene 只调用统一的数据接口（如 `GetPlayerBag(player_id)`），不感知跨服细节。
- Data Service 职责：
  - `player_id -> home_zone_id` 映射解析。
  - `home_zone_id -> region_id -> redis_cluster` 路由。
  - 重试、熔断、降级、可观测性、热点缓存。

## 5. 区服分层与大区锁规则

- 区服（Zone）：游戏逻辑服务器（玩家创角并永久归属的单位）。
- 大区（Region）：区服之上的分组层（运维和存储分配单位）。
- 存储策略：Redis 按大区分配，而非按单个区服分配。

大区锁规则：

- 锁开启：玩家只能访问归属大区内的区服。
- 锁关闭：玩家可以访问任意区服。
- 锁决策归属 SceneManager（控制面），而非 Data Service（数据面）。

## 6. 玩家 ID 与映射表设计

### 6.1 玩家 ID 规则

- 使用现有 Snowflake：`[time:32][node_id:17][step:15]`。
- `zone_id` 绝不可编码进 `player_id`。
- `node_id` 仅表示"生成该 ID 的物理节点"，不承载区服语义。

理由：

- 若 ID 中包含区服语义，合服时将面临系统性重写风险。
- 玩家 ID 在其整个生命周期内必须保持不可变（跨服、合服、迁移——永不改变）。

### 6.2 独立映射表

- `player_id -> home_zone_id`（全局真值映射）。
- `home_zone_id -> region_id`（配置映射）。
- `region_id -> redis_addr`（路由配置）。

推荐存储方式：

- `player_id -> home_zone_id` 存储在全局 Redis 或 MySQL + 本地缓存中。
- 在角色注册时写入；作为后续所有数据路由的依据。

## 7. 跨服场景切换流程（关键一致性约束）

核心不变式：在任意时刻，只有一个 Scene 可以写入某玩家的数据（Single Writer）。

标准流程：

1. SceneManager 通知旧 Scene：释放玩家并将数据刷入存储。
2. 旧 Scene 确认"已保存并释放"。
3. 此后 SceneManager 才通知新 Scene：加载该玩家。
4. 若第 2 步超时，则中止迁移——新 Scene 绝不可抢先加载。

该流程保证：

- 不会有两个 Scene 同时写入同一玩家的数据。
- 场景切换期间不会出现状态覆盖或回滚困难。

## 8. 一致性防御层次

第 1 层（主要防线）：

- SceneManager 串行化切换流程，强制执行 Single Writer。

第 2 层（异常兜底）：

- Data Service 对玩家键施加短时分布式锁（如 Redis `SETNX` + TTL 约 3 秒）。

第 3 层（最后手段）：

- 关键数据使用版本号（乐观锁）或事务检查（WATCH/MULTI）。

注意：Redis 单线程仅保证同一 key 单连接的 FIFO；真正的风险来自上游并发写入，因此 Single Writer 必须在架构层强制执行。

## 9. 合服策略（玩家 ID 不变）

策略 A（推荐）：

- 将目标服务器的数据迁移到主服务器的存储中；批量更新 `player_id -> home_zone_id` 映射。

策略 B（过渡方案）：

- 将多个区服路由到同一 Redis 集群；逐步完成数据合并。

共同特性：

- 均不修改 `player_id`。
- 业务代码（Scene/Gate）无需任何改动。

## 10. NodeId 冲突与快速恢复设计

### 10.1 问题定义

etcd 租约过期/重新注册的时间窗口可能导致同一 `node_id` 双活（旧节点恢复网络 + 新节点已接管）。

风险：

- 双活节点同时处理玩家请求。
- 相同 `node_id` 并发签发导致 Snowflake ID 碰撞。

### 10.2 冲突处理要求

当节点确认"我的 `node_id` 已被另一个 uuid 占用"时，必须：

1. 立即停止接受新的业务请求。
2. 尽快将在线玩家数据刷入存储。
3. 踢出或迁移在线玩家（按节点类型策略执行）。
4. 自行终止以防止持续双活。

节点类型策略：

- SceneNode：保存状态，然后将玩家迁移到同区服内的可用节点。
- GateNode：断开客户端连接并触发重连路由。
- InstanceNode：副本状态无法迁移——通知副本失败并踢出玩家。

### 10.3 Snowflake 冲突保护

- 心跳写入 `snowflake_guard:{zone}:{type}:{node_id}`（TTL 600 秒）。
- 新节点接管后，调用 `SetGuardTime(now_utc)` 跳过当前秒的 ID 签发窗口。
- 不设"等待安全窗口"的启动延迟——保留快速灰度发布/断线恢复能力。

## 11. 与当前代码库的集成边界

- Gate：保留 Kafka 控制面消费者，在 `gate-{id}` 上接收 SceneManager 命令。
- SceneManager：负责场景切换编排、大区锁策略、Single Writer 流程执行。
- PlayerLocator：记录玩家当前所在的 Scene 位置；可扩展以携带 home_zone 信息。
- Data Service：可扩展现有 `go/db/` 或创建新的 `go/data_service/`。

## 12. 永久设计红线

1. 任何业务模块不得引入"跨服模式分支"。
2. 玩家数据不得作为常规机制进行跨服复制。
3. 所有跨服路由真值必须集中在数据代理层。
4. 任何需要知道"玩家来自哪个服务器"的新功能，必须先触发架构评审。
5. 稳定性优于延迟；正确性优于局部性能。

## 13. 后续实施清单

- 在 Node 子类（Scene/Gate/Instance）中完成 `OnNodeIdConflictShutdown` 的差异化实现。
- 在优雅停机流程中增加：持久化玩家数据 -> 迁移/踢出玩家 -> 主动注销节点键。
- Data Service：添加逐玩家锁和关键写入版本字段。
- 建立跨服场景切换可观测性：各阶段延迟、失败原因、回滚次数。

## 14. 跨场景玩家消息

### 14.1 问题

Scene A 需要向某玩家发送消息（如任务奖励、交易结果、系统通知），但该玩家可能不在 Scene A 上。
旧方案通过 Centre 转发，导致 Centre 成为瓶颈且流程复杂。新方案通过 Kafka 投递，但引入了路由窗口一致性问题。

### 14.2 两种场景

**场景 1：玩家不在本地 Scene（跨场景投递）**

- 通过 Kafka 将消息投递到玩家当前所在 Scene 的 topic。
- 关键边界情况：消息已进入 Kafka（目标 Scene B），但在路由过程中玩家从 B 切换到了 C；当 Scene B 收到消息时，玩家已经离开。

**场景 2：玩家在本地 Scene（本地投递）**

- 本地 Scene 可以直接处理，或统一通过玩家消息队列路由。

### 14.3 消息优先级分类（Protobuf Option 注解）

在 RPC 服务方法定义上通过 `option` 标注消息优先级；代码生成层据此产出不同的投递策略：

| 等级 | 含义 | 投递保证 | 路由失败处理 |
|------|------|----------|--------------|
| IMPORTANT | 交易结果、奖励发放、状态变更等 | at-least-once | 目标 Scene 发现玩家已离开 -> 重新查询 PlayerLocator -> 转发到新 Scene；若仍失败 -> 持久化到 DB/Redis，玩家下次进入场景时重新投递 |
| NORMAL | 一般通知、非关键 UI 提示 | best-effort | 目标 Scene 发现玩家不在 -> 丢弃 |

proto option 定义示例（待细化）：

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

- 延迟最低；适用于高频低延迟消息，如移动同步。
- 缺点：本地和跨场景路径逻辑不一致；无法保证统一的消息排序。

**方案 B：统一玩家消息队列（异步路径）**

- 所有发给玩家的消息（无论本地还是跨场景）都进入玩家消息队列，由统一的消费者处理。
- 优点：符合"位置透明"原则；逻辑统一；消息有序。
- 缺点：本地消息多经过一次队列跳转。

**结论**：默认采用统一消息队列（方案 B），与跨服"位置透明"原则保持一致。对于移动同步等超低延迟场景，允许标记消息走直接路径（方案 A）作为例外。

### 14.5 投递流程

```text
发送方 Scene A
    |
    |-- 查询 PlayerLocator：玩家在哪？
    |
    +-- 在本地 Scene A --> 入队到本地玩家消息队列 --> 统一处理
    |
    +-- 在 Scene B --> Kafka 投递到 scene-{scene_b_id} topic
                          |
                          v
                       Scene B 消费消息
                          |
                          +-- 玩家在线 --> 入队到本地玩家消息队列 --> 统一处理
                          |
                          +-- 玩家已离开
                                |
                                +-- IMPORTANT --> 重新查询 PlayerLocator --> 转发到新 Scene / 持久化待后续重投
                                |
                                +-- NORMAL --> 丢弃
```

### 14.6 与现有架构的关系

- 复用 Kafka 基础设施：与 Gate 控制面共享 Kafka 集群，新增 `scene-{scene_id}` topic。
- PlayerLocator 查询：复用现有 `player_locator` 服务获取玩家当前位置。
- Single Writer 保证：消息投递不违反 Single Writer（§7）；消息仅在玩家当前所在的 Scene 上被消费和处理。
- 持久化兜底：重要消息通过 Data Service（§3）的统一数据通道持久化。

### 14.7 待实现

- [ ] 定义 `MessagePriority` protobuf option 并集成到 proto-gen（历史名称：pbgen）代码生成流水线中。
- [ ] 在 Scene 侧实现玩家消息队列（ECS Comp + 消费者 System）。
- [ ] 在 Scene Kafka Consumer 中添加 `scene-{scene_id}` topic 订阅。
- [ ] 重要消息投递失败后的重试/持久化-重投逻辑。
- [ ] 确定 Kafka topic 分区策略（玩家哈希 vs. scene ID）。

## 15. 无状态微服务的就绪门控

### 15.1 问题

在分布式游戏后端中，服务往往需要在开始安全处理请求之前完成数据加载（配置表、缓存预热、依赖连通性检查）；
但微服务同时追求无状态和弹性扩缩。

### 15.2 原则

- 就绪状态外化：将"可接受流量"的决策委托给服务发现和编排层；不要将复杂的等待状态逻辑耦合到业务流程中。
- 调用方只发现就绪实例：未就绪的实例不得出现在服务发现的可见集中。

### 15.3 推荐启动顺序

```text
启动进程
  -> 加载配置 / 静态数据
  -> 缓存预热 & 检查依赖连通性
  -> 开启服务端口（可选）
  -> 注册到服务发现（etcd）
  -> 切换 readiness=ready
```

### 15.4 依赖等待策略

- 硬依赖路径：监听服务发现（etcd），目标实例就绪后恢复业务流程。
- 软依赖路径：快速失败 + 指数退避重试；避免阻塞整个调用链。

### 15.5 项目级约定

- Go 服务：etcd 注册必须在加载和预热完成后进行。
- C++ 节点：对外可路由的时间点必须在节点初始化完成之后。
- Kubernetes：就绪探针和注册时机必须具有一致的语义；禁止出现长时间"已注册但未就绪"的窗口。

### 15.6 待实现

- [ ] 编制统一的"服务就绪门控"检查清单（Go/C++ 共用）。
- [ ] 为关键服务添加就绪健康检查和启动阶段指标埋点。

---

本文档作为跨服架构的统一设计基线，替代分散的对话结论。后续变更应直接修改本文档，避免"在聊天中讨论过但从未记录下来"导致信息丢失。
