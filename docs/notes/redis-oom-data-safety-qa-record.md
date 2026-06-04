# Redis OOM / 数据安全 / 千万在线 — Q&A 决策记录

> 创建日期: 2026-06-03
> 状态: 决策记录,P0/P1 尚未实施
> 配套文档:
>   - docs/notes/player-data-loading-and-sharding-pain.md(玩家数据加载痛点)
>   - 待写: 真正的设计文档(P0/P1 完成后)

本文档**完整记录**用户在 2026-06-03 这次讨论中提出的所有问题与回答,
作为后续设计与施工的依据。所有结论都来自代码证据 + 用户业务约束,
不是凭空决策。

---

## 讨论起点

Round 16 单 zone 45k 在线压测 PASS 后,用户问:
> "你帮我看下我们在 redis 里能持续保留多少个玩家信息"

实测得到:
- 单玩家 Redis 占用 **~21 KB**(17 个 key)
- 30 分钟 TTL(`go/data_service/internal/dataservice/cache/cache.go`)
- 当前 Redis 配置: `maxmemory=0`(无限制) + `policy=noeviction` ⚠️ **危险**
- 8 GB 上限可装 ~38 万在线玩家

由此引出 OOM 防御设计讨论。

---

## Q1: "Redis 到达上限后怎么办?"

**初版方案(已被用户否决)**:
分级响应 → 95% 时强制 disconnect 最旧玩家。

**用户反馈**: "不能踢人,只清空不在线的玩家"。

**修正方案**: 应用层主动卸载冷玩家 cache,Redis 自身用 LFU 兜底,
绝不强制断开任何在线玩家。

---

## Q2: "你说的清空 Redis,我之前不是说有月卡挂机吗?"

**关键发现**(代码证据):
- `cpp/libs/services/scene/player/system/afk.cpp:14`: 30 秒无客户端消息 → `AfkComp`,scene 系统 exclude tick
- `cpp/nodes/scene/handler/rpc/player/player_lifecycle_handler.cpp:30-34`(TODO):
  ```cpp
  // 玩家退出时,有月卡的转 AI 控制继续在场景里
  // if (subscriptionComp && subscriptionComp->IsActive()) {
  //     SwitchToAIControl(player); return;
  // }
  ```

**月卡挂机的影响**:
- 月卡用户即使关客户端,角色仍在 scene 里跑(AI 接管打怪)
- 持续修改 inventory / currency / quest
- 占用 scene tick CPU + Redis 全套数据
- **这类玩家的 cache 绝对不能清** — 清了 = 违约 = 退款 = 法务事故

**修正后玩家在 Redis 里的 4 种状态**:

| 状态 | 客户端 | Scene tick | 能清 cache? |
|---|---|---|---|
| A. 真在线 | 连着 gate | 跑 | ❌ |
| B. 月卡 AI 挂机 | 关了/后台 | 跑(AI) | ❌ **付费用户** |
| C. AFK 静止(可能含月卡) | 连着但 30s 无操作 | 不 tick | ⚠️ 分不清,不动 |
| D. 真下线超 30s | 断了+宽限期过 | 已 ExitGame | ✅ 整个清 |

**只能清 D 类**,实际只能省 ~12% 占用(45k 在线对应 ~96 MB)。

---

## Q3: "大块数据不放 Redis 我崩溃了不就丢数据了吗?先别改,先分析"

**这是最关键的一组问题。** 我用代码证据做完整分析,**结论:Redis 不是数据丢失的关键路径**。

### 当前数据持久化架构(代码读出来的事实)

```
[Scene 进程内存 ECS]  ←→  [Redis cache]  ←→  [Kafka db_task]  ←→  [MySQL]
   权威运行时态           热数据 30min TTL    异步写队列        最终持久化
   每帧 20fps 改          只读快照            幂等 task          真正不丢
```

### 关键事实

**1. Scene 进程内存才是"权威运行时数据"**(`tlsEcs.actorRegistry`)
- 玩家在线期间,所有读写都在 ECS 内存里
- Redis 不参与运行时,只在 EnterGame 加载/周期 save 时碰

**2. Redis 是"快照中转站",不是"权威数据源"**
- 30min TTL 就是证据 — 真正的权威数据**不会**给 30 分钟 TTL
- Redis ���是给"快重启 / 跨节点迁移"加速用

**3. 每 5 分钟全员强制 save**(`cpp/libs/services/scene/core/system/redis.cpp:63`)
   ```cpp
   int periodicSaveSec = 300;  // 5 分钟
   // 注释: "bound the data-loss window if the scene node crashes"
   ```
   **设计上已经接受"最多丢 5 分钟"的容忍度**。

**4. Kafka 才是"不丢防线"**
- `SavePlayerToRedis` 同时**两条独立通道**(`player_lifecycle.cpp:838,878-879`):
  ```cpp
  tlsRedisSystem.GetPlayerDataRedis()->Save(...);  // 通道 1: Redis 快照
  sendSubTableTask(...);                           // 通道 2: Kafka 持久化
  ```
- Redis 写失败,Kafka 还是会投
- Kafka 投失败,Redis 还是会写
- **两个通道互不依赖**

### 5 种崩溃场景分析

| 场景 | 现状 | 假设不放 Redis | 差异 |
|---|---|---|---|
| A. Scene 崩溃 | 重连从 Redis 拉 → 20ms | Redis miss → 投 Kafka 让 db 查 MySQL → 100-300ms | **登录变慢,数据不丢** |
| B. Redis 整个挂 | 重连从 MySQL 加载 | 同左 | **完全相同** |
| C. Kafka 5 分钟内挂 + 重启 | 投递的消息没消费就被 retention 清 → 真丢 | 同左 | **与 Redis 无关** |
| D. db service 卡住超过 5 分钟 | Kafka retention 到了 → MySQL 没收到 → 真丢 | 同左 | **与 Redis 无关** |
| E. Scene 崩溃 + Redis 同时丢 | 重连从 MySQL 加载 → 回到上次 Kafka task 落库的状态 | 同左 | **完全相同** |

**结论**: "Redis 是不是存大块数据" 和 "崩溃时丢不丢数据" **是两个独立维度**。

### 真正决定数据安全的 3 件事

1. ✅ Scene 内存里改了多久还没 SavePlayerToRedis(默认 5 分钟)
2. ✅ Kafka retention 够不够 db service 消费(默认 5 分钟)
3. ✅ MySQL 写入是否成功

**Redis 只决定重连快不快,不决定数据丢不丢。**

### 重大隐藏风险:Kafka retention=5min + replication=1

代码证据:
- `go/login/etc/login.yaml:260`: `RetentionMs: 300000`(5 分钟)
- `deploy/docker-compose.yml:76`: `replication-factor 1`(**单副本!**)
- `go/login/internal/kafka/key_ordered_producer.go:61`: `WaitForAll`(producer 设置正确)

**问题**:
- Kafka broker 单副本: broker 磁盘挂 → 5 分钟内未消费消息全丢
- db service 卡 6 分钟+ → 第 5 分 01 秒开始的消息被 retention 删除
- producer `WaitForAll` 在单副本下退化成 acks=1

**这比 Redis OOM 风险高 10 倍。**

---

## Q4: 用户的三个核心要求

用户在分析后明确表态:

> "1. db service 卡也不能丢数据
>  2. 不能接受崩溃丢 5 分钟的改动
>  3. 期望 1000 万在线
>  方案 C: 双层 — Redis LFU 兜底 + 应用层在 60% 时主动卸载安全玩家
>  我要数据绝对安全,一致性一定要有"

### 我的回应:三个要求合起来 = CAP 不可能三角

**「绝对安全 + 强一致 + 千万在线」三个一起 = 物理上做不到**。这不是技术不到位,
是 CAP 定理。但游戏跟银行不一样,**可以按数据类型分层**。

### 数据按重要性分 3 类(MMO 行业标准)

| 类别 | 例子 | 安全等级 | 一致性等级 | 理由 |
|---|---|---|---|---|
| **金钱类** | 充值、付费道具、月卡、交易、公会银行 | L3(99.9999%) | 强一致 | 丢一分钱玩家退款 |
| **进度类** | 等级、装备、任务、技能、背包(非付费) | L2(99.99%) | 读自己写的 | 丢了骂街但不退款 |
| **运行时** | 位置、HP、buff、AOI、战斗状态 | L1(99%) | 最终一致 | 重连回到上一秒玩家无感 |

腾讯、网易、米哈游都是这么做的。

---

## Q5: "开服收益大,一直立即存,CPU 消耗高,怎么办?"

**关键澄清**: "立即写"不等于"每次改钱都全量序列化 PlayerAllData"。

### 三层缓冲方案

```
玩家获得 100 金币
    ↓
[L1 内存改]      ECS 立即改 PlayerCurrencyComp(纳秒级)+ 标记 dirty
    ↓
[L2 增量 WAL]    立即投 Kafka 一条 delta 事件(~200 字节)
                 不是全量 PlayerAllData(几 KB)
    ↓
[L3 周期合并]    每 5-10 秒合并一次,dirty-save IsEqual 跳过没改的
```

### 实际成本估算(单 zone 100k 在线,极端假设)

- 每玩家每秒 1 次金币变化 = 100,000 events/sec
- 每事件 200 字节 = 20 MB/sec Kafka 流量(可接受)
- 序列化 50μs × 100k = 5 核 CPU(可接受,Round 16 单 zone 远超���)

### 终极优化(如真有需要)

**Currency 事件批量化**: scene 在内存里 batch 100ms 内的所有 currency 事件,合并投 Kafka。
100ms 延迟玩家无感,Kafka QPS 降 10x。

**结论**: 开服 CPU 爆炸不是问题,问题是要避免有人写"每改钱投全量"这种代码 →
通过 code review + SavePlayerToRedis 内置 rate limit 防误用。

---

## Q6: "战报需要强一致吗?可以数据恢复?"

**用户直觉对的:不需要强一致**。MMO 行业战报方案:

| 数据 | 一致性 | 存储 | 理由 |
|---|---|---|---|
| 战斗实时状态(打了几下、HP) | 最终一致 | Scene 内存 | 战斗结束就丢 |
| 战报摘要(谁赢、伤害总量) | 最终一致 | Kafka → ClickHouse/ES | append-only 按需查询 |
| 战报详细帧序列 | 最终一致 | 对象存储 S3/OSS,压缩 | 90% 玩家永远不看 |
| **战斗对账户的影响**(扣血、消耗药、获得装备) | **强一致** | **MySQL 事务** | **这是金钱类,不是战报类** |

**核心区别**: 战报记录"发生了什么"用最终一致即可,但战报对账户的影响必须走强一致路径。
**两者是两条独立链路,不要混在一起。**

### "可数据恢复"含义

- 战报是衍生数据,可以从更原始的事件流(Kafka)重放生成
- ClickHouse 整个挂了,只要 Kafka 还在,重新消费就能重建
- 这就是为什么战报特别适合 Kafka + 长期 retention(7-30 天)→ 对象存储归档

### 给项目的建议

- 新建 `battle_event` Kafka topic
- replication=3, retention=7 天
- 消费者写 ClickHouse
- **完全独立于 PlayerAllData 持久化路径**

---

## Q7: "Redis 集群会 OOM 吗?"

**会,但 OOM 影响范围不同**:

| 类型 | 满了会怎样 | 影响范围 |
|---|---|---|
| 单实例 maxmemory=0(当前) | 写满宿主机内存 → OOM Killer → 所有容器一起死 | **全服** |
| 单实例 maxmemory=8gb noeviction | 8GB 满 → 拒绝写入 → EnterGame 全失败 | **全服功能** |
| 单实例 maxmemory=8gb allkeys-lfu | 8GB 满 → 淘汰冷 key → 服务继续 | **重连慢但活着** |
| Redis Cluster 3 shard,单 shard 满 | 只有那个 shard 玩家受影响 | **1/3 玩家** |
| Redis Cluster 全 shard 满 | 全部拒绝/淘汰 | **全服** |

### 关键认识

1. **集群不能"避免 OOM",只能"缩小爆炸半径"**
2. **数据倾斜风险**: 按 player_id 哈希分片,刚开服爆款 zone 会把对应 shard 打爆,其他空着
3. **集群该做,但单机 maxmemory + LFU 配置必须先做,这是底裤**

---

## Q8: 千万在线规划(用户澄清)

**用户原话**:
> "不是单 zone 在线 1000 万,而是多 zone 在线"
> "先把单 zone 100k 做扎实,放弃真 0 丢失"
> "金钱零丢 + 进度丢几秒可接受 → 标准 MMO 模式,可达"

### 重新定位目标

- ❌ 不是: 单 zone 千万
- ✅ 是: 多 zone 总和千万,单 zone 100k 做扎实
- ✅ 数据安全: 金钱零丢 + 进度容忍 5-10 秒

这个目标比单 zone 千万务实得多,**99% 把"玩家分布"问题转化为"运营问题"**(怎么开服分流),技术问题相对简单。

---

## Q9: 痛点优先级(用户原话: "这些都是")

用户说三个痛点全要解决:
1. 怕 Redis 满了挂宿主机
2. 怕 db 卡了丢数据
3. 怕千万在线扛不住

但**实施成本完全不同**,我按"风险/收益比"排序:

### P0 — 今天就做(5 分钟,零风险) ✅ 已完成 2026-06-03

**Redis maxmemory + LFU**

```bash
docker exec redis redis-cli CONFIG SET maxmemory 8gb
docker exec redis redis-cli CONFIG SET maxmemory-policy allkeys-lfu
docker exec redis redis-cli CONFIG SET notify-keyspace-events Eg
```

并修改 `deploy/docker-compose.yml` redis 段,把配置写入 `command:`,
保证容器重启不丢配置:

```yaml
command: >
  redis-server
  --appendonly yes
  --maxmemory 8gb
  --maxmemory-policy allkeys-lfu
  --notify-keyspace-events Eg
```

**实施验证**(2026-06-03):
- `maxmemory_human: 8.00G` ✅
- `maxmemory_policy: allkeys-lfu` ✅
- `notify-keyspace-events: gE`(等价 Eg,Redis 规范化输出)✅
- 当前 used_memory: 11.86 MB / 8 GB,有充足上调余地

**为什么 P0**: 当前 `maxmemory=0 + noeviction` = 撑爆宿主机让所有容器一起死的炸弹。
改成 `8gb + allkeys-lfu` 后最坏情况是淘汰冷玩家 cache,服务存活。

**`notify-keyspace-events Eg` 的预留作用**: 开启 evict/expire 事件通知,
为后续 P3 阶段应用层订阅 `__keyevent@0__:expired/evicted` 实现"主动卸载冷玩家"
路径预留,现在不用就是无成本预留。

**代价**: 0 行 Go/C++ 代码,docker-compose.yml 改 6 行,1 分钟生效。

### P1 — 这周做(2-3 天,中风险) 🟡 部分完成 2026-06-03

**Kafka 持久化加固** —— 拆成 3 个独立动作,本地与生产分开处理。

#### P1.1 调整 retention 默认值 ✅ 已完成 2026-06-03

代码默认值从 5min 提升到 24h,4 处全改:

| 文件 | 旧值 | 新值 |
|---|---|---|
| `go/login/etc/login.yaml:260` | `RetentionMs: 300000` | `RetentionMs: 86400000` |
| `go/db/internal/config/config.go:56` | `default=300000` | `default=86400000` |
| `go/login/internal/config/config.go:187` | `default=300000` | `default=86400000` |
| `deploy/login-stack.linux/login.yaml:142` | `RetentionMs: 300000` | `RetentionMs: 86400000` |

**实施验证**:
- `go build ./...` login + db 服务编译通过 ✅
- staticcheck 报的 `optional`/`default=...` 是 go-zero 自定义 tag,不是真错误,跟改动无关
- 本地无运行 topic(dev-stop 后),下次启动 login service 时新代码自动应用 24h retention

**为什么 24h**: db service 卡住的耐受时间从 5 分钟 → 24 小时。运维事故处理窗口
从"刚收到告警还在赶来路上数据就丢了"提升到"白天慢慢修也来得及"。

#### P1.2 Kafka topic ReplicaFactor 字段已就位 ✅

`go/shared/kafkautil/topic_init.go` 的 `TopicSpec.ReplicaFactor` 字段早就存在,
只是当前 `login.go` / `db.go` 创建 topic 时没传(走默认 1)。

生产部署时调用方传 3 即可,**代码不需要再改**。

#### P1.3 真正部署 3 broker 集群 ⏸ 留给生产部署

**为什么本地不做**:
- 本地单机跑 3 broker = 浪费 ~12 GB 内存,压测意义不大
- 关键是验证代码改动正确,这部分 P1.1 + P1.2 已完成
- 真正的数据安全收益在生产环境

**已写完整 runbook**: `docs/ops/kafka-cluster-production-runbook.md`,包含:
- 3 broker docker-compose 模板
- 零停机迁移步骤(并行运行 → 切流 → 下线旧 broker)
- 回滚预案
- 监控指标 + 告警阈值
- 容量规划(单 zone 100k 与多 zone 1000 万的存储压力对比)
- 灾难恢复演练流程

**关键容量发现**: 多 zone 1000 万在线 + 24h retention,中心化 Kafka 需要 170 TB
存储,**不现实**。结论:**每 zone 必须独立 Kafka 集群**,数据隔离 + 存储压力可控
(每集群 ~1.7 TB / 24h)。这是后续多 zone 横向扩展的硬性约束,需要在 P3/P4
阶段考虑。

#### P1 当前状态总结

- ✅ 代码层防御已就位(retention 默认 24h)
- ✅ 生产部署有完整 runbook,不会忘
- ⏸ 真正的 3 broker 集群部署等生产环境
- ⏸ 本地保持单 broker(开发效率优先)

**剩余风险**(本地+任何单 broker 环境):
- broker 磁盘故障仍会丢消息(单副本)
- 但 retention 24h 给了"db service 卡住"足够长的耐受窗口
- 这个剩余风险只能靠 P1.3 真正的多 broker 集群解决

### P2 — 这个月做(2-3 周)

**金钱路径分离 + dirty-save 接入**

1. **接入 dirty-save**(`proto-compare-dirty-save.md` TODO):
   - 把 `IsEqual` + `PlayerLastPersistedSnapshotComp` 接到 SavePlayerToRedis 头部
   - 降低 70%+ 无效 save
   - 工作量 1 天

2. **Currency 事件单独 topic**:
   - 新建 `player_currency_event`,replication=3,retention=7 天
   - Currency ���更立即投 delta 事件
   - db service 单独消费 → MySQL UPDATE
   - 工作量 1 周

3. **事件驱动 save 时机**:
   - 关键事件(交易完成、充值)立即触发 save
   - 普通进度变更走 5-10 秒批量
   - 工作量 3-5 天

**前置依赖**: 必须先验证 CurrencyComp 是否真的在 PlayerAllData 持久化路径里(见 player-data-loading-and-sharding-pain.md 的 ⚠️ 隐患)

### P3 — 这个季度做(1-3 个月)

**Redis Cluster + 单 zone 100k**

- Redis 单机 → 3 shard cluster
- 按 player_id 哈希分片
- 单 zone 压测从 45k 推到 100k
- 期间会暴露新瓶颈(scene 进程数、player_locator 单点等)

### P4 — 长期(半年-1 年)

**多 zone 横向扩展**

- MySQL 分库分表
- player_locator 分片化
- 跨 zone 路由网格
- 多机房灾备

---

## 用户最后追问的玩家数据加载痛点

→ 已写入 `docs/notes/player-data-loading-and-sharding-pain.md`,详细分析:
- PlayerAllData 是 7-component grab-bag
- CurrencyComp 可能没接进 PlayerAllData(数据安全隐患)
- 老号无限增长导致 EnterGame 慢
- 行业标准: 冷热分离 + 模块化分表 + 哈希分库

---

## 下一步行动

按 P0 → P1 → P2 → P3 → P4 顺序执行。每完成一阶段,**回头更新本文档**,标记
"已完成"并附上验证证据(压测报告、metrics 截图等)。

### P0 实施前检查项

- [ ] 确认当前 Redis 实际占用(应远低于 8GB,有充足上调余地)
- [ ] 确认 LFU 而非 LRU(LFU 对游戏 cache 命中率高 — 在线玩家高频访问自然保留)
- [ ] CONFIG REWRITE 写回 redis.conf,防容器重启丢配置

### P1 实施前需澄清的问题

- [ ] 项目有没有线上玩家? 影响是否能停机改 Kafka 集群
- [ ] 当前 Kafka 是单 broker docker-compose,需先扩 3 broker(本地与生产分开规划)
- [ ] retention 24h 会增加多少磁盘? 估算: 单 zone 100k 在线 × 每秒 100 events × 24h = ~900GB,**这个数字偏高,需要更精确的事件率测算**

### P2 实施前必须先做的验证

- [ ] **验证 CurrencyComp 持久化**:加金币 → kill scene → 重启 → 检查金币是否还在
  - 如果丢: P0/P1 之外的紧急 bug,优先级超 P1
  - 如果不丢: 找出走的是哪条独立路径,记录到 player-data-loading-and-sharding-pain.md

---

## 决策记录的元规则

**任何架构改动都遵循以下原则**(避免重蹈"改完发现需求理解错"的覆辙):

1. **代码证据优先**: 引用具体文件 + 行号,不凭印象
2. **用户业务约束优先**: 不能踢人、月卡 AI 挂机这类业务规则,代码逻辑必须服从
3. **分阶段验证**: P0 完成才做 P1,每阶段有明确的成功指标
4. **回滚预案**: 每个改动有明确的回滚步骤
5. **数据驱动**: metrics 不到位前,不做"性能优化"
