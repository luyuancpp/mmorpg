# 玩家数据加载与分库分表的痛点分析

> 创建日期: 2026-06-03
> 状态: 痛点记录,未实施任何改动
> 上下文: Round 16 压测后(单 zone 45k 在线 PASS),用户在讨论 OOM 防御 +
> 千万在线规划时提出此核心矛盾。

---

## 用户原话(2026-06-03)

> "其实我不是玩家数据分库分表吗?我感觉一次性把玩家数据加载出来这种效率有些低,
> 但是我是 mmo,但是不一次性加载出来,中间有的要去改金钱数据了,发现找不到金钱
> 的 proto,playeralldata 就是放各种玩家表数据,而且随着玩家越往后数据量越大,
> 加载越慢怎么办"

这是 MMO 后端最深的设计矛盾之一,值得单独记一篇。

---

## 当前架构事实(从代码读出来,不是猜)

### PlayerAllData 结构 (`proto/common/database/player_cache.proto`)

```proto
message PlayerAllData {
  player_database   player_database_data   = 2;  // 7 个 ECS component 的混合大杂烩
  player_database_1 player_database_1_data = 3;  // 同上,扩展槽
  BagAllData        bag_data               = 4;  // 背包(已落地 schema)
  QuestAllData      quest_data             = 5;  // 任务(预留槽,cpp 还没实现)
  MailAllData       mail_data              = 6;  // 邮件(预留槽,计划走独立服务)
}
```

### `player_database` 内容 (`proto/common/database/mysql_database_table.proto:90`)

```proto
message player_database {
  PlayerUint64Comp        uint64_pb_component       = 3;
  PlayerUint32Comp        uint32_pb_component       = 5;
  BaseAttributesComp      derived_attributes_component = 6;
  LevelComp               level_component           = 7;
  // ... 7 个 component 混在一起
}
```

注释自己说 `player_database` 是 "**already a 7-component grab-bag**" — 设计者自己都
意识到这是个杂烩,但没拆开。

### CurrencyComp 在哪?

`proto/common/component/currency_comp.proto`:

```proto
message CurrencyComp {
  repeated uint64 values = 1;            // 各种货币(金币/钻石/...)
  repeated uint32 blocked_types = 2;     // GM 封禁类型
  repeated CurrencyDebtEntry debts = 3;  // 补缴债务
}
```

**修正(2026-06-03 二次核对代码)**: 之前判断错误。CurrencyComp **已经接进 player_database 全链路**:

| 层 | 文件:行 | 状态 |
|---|---|---|
| Proto | `mysql_database_table.proto:103` | `CurrencyComp currency = 8;` ✅ |
| Marshal (save) | `player_database_loader.cpp:24` | `message.mutable_currency()->CopyFrom(... CurrencyComp ...)` ✅ |
| Unmarshal (load) | `player_database_loader.cpp:14` | `emplace<CurrencyComp>(player, message.currency())` ✅ |
| MySQL 列 | `go/db/model/mysql_database_table.sql:24` | `currency MEDIUMBLOB` ✅ |
| Lifecycle | `player_lifecycle.cpp:788, 878` | SavePlayerToRedis → Kafka → DB sub-table task ✅ |

容易混淆的是项目里有**两个** currency 相关 component:

- `CurrencyComp`(proto, `currency_comp.proto`): 持久化的余额/封禁/补缴债务,
  进 `player_database` blob,正是上表里的那个。
- `PlayerCurrencyComp`(runtime, `player_currency_comp.h`): runtime-only,只存
  dirty 标记和 debt 运行时缓存,**不持久化**(注释自陈
  "Persistent balances are stored in proto CurrencyComp")。

用户说的"找不到金钱 proto",真正的根源是:
1. 字段名叫 `currency` 而不是 `gold`/`money`,搜索关键字时容易漏掉。
2. `CurrencyComp.values` 是 `repeated uint64`,通过 type id 索引,**没有具名的
   "金币"字段**,看不出来哪个 slot 是哪个币种。
3. 项目里两个同名前缀的 comp(`CurrencyComp` vs `PlayerCurrencyComp`),
   review 代码时容易看错哪个是持久化的。

这是**命名 / 可发现性问题**,不是数据安全 bug。

> ✅ **数据安全侧**: currency 在 PlayerAllData 全量 save 路径里,玩家正常下线
> 会触发 SavePlayerToRedis,金币不丢。
>
> ⚠️ **真正的风险窗口**: 进程崩溃(kill -9)在两次 save 之间发生,期间的金币变更
> 会丢——这正是 P2 "金钱独立 delta 写"要解决的问题,而不是"currency 完全没
> 持久化"。下文阶段 0.1 的"kill -9 验证"目的是**测量这个丢失窗口的大小**,
> 不是验证"金币能否落地"。

---

## 核心矛盾梳理

### 矛盾 1: "全量加载慢" vs "MMO 玩法需要全部数据"

**全量加载的代价**:
- EnterGame 必须把整个 PlayerAllData 拉进来才能进游戏
- 数据量随玩家时长增长 — 1 个月号背包 1000 件,1 年号 10000 件
- Round 16 测试是新号(空背包),登录 ~20ms;老号背包/邮件/任务全满后估计 100-300ms
- 这是 EnterGame P99 延迟的最大威胁

**不全量加载的代价**:
- 玩家在 scene 里随时可能开背包、查任务、看邮件
- 这些操作如果要"按需从 Redis/MySQL 拉",每次都是一次跨进程 RPC
- MMO 战斗中开背包嗑药要 50ms 延迟 = 玩家骂街

**这就是 MMO 的根本困境**: 数据要么全 hot(吃内存)、要么按需(吃延迟),
没有第三条路。

### 矛盾 2: "立即写金钱" vs "找不到金钱 proto"

用户的实际感受是: "我想立即存金钱,但代码里 PlayerAllData 是个大杂烩,我没法只动金钱。"

这是因为 `SavePlayerToRedis` 是个**全量序列化**操作:

```cpp
// cpp/libs/services/scene/player/system/player_lifecycle.cpp:775
void PlayerLifecycleSystem::SavePlayerToRedis(entt::entity player) {
    SaveMessage message = std::make_shared<...>();
    PlayerAllDataMessageFieldsMarshal(player, *message);  // 序列化全部
    // ... 投 Kafka 写 Redis
}
```

要"只立即存金钱",需要的是 **delta 写**,不是全量 save。当前架构没有这个原语。

### 矛盾 3: "数据越多加载越慢" vs "玩家会一直玩下去"

老玩家的数据增长是不可避免的:
- 背包: 几十 → 几千件
- 邮件: 0 → 几百封(包含附件)
- 任务: 0 → 上千条历史完成记录
- 战报: 0 → 上万场战斗

如果继续走"全量 PlayerAllData"模式,**3 年后老玩家 EnterGame 会需要秒级**,完全无法接受。

---

## 行业是怎么解的(MMO 标准答案)

### 答案 1: 数据**冷热分离**,不再有"PlayerAllData"

把玩家数据按访问频率分 3 层:

| 层 | 内容 | 加载时机 | 持久化 | 单玩家大小 |
|---|---|---|---|---|
| **Hot Core** | 等级/位置/HP/MP/属性/金钱/当前装备 | EnterGame 立即加载 | 每次变更立即写 | < 2 KB |
| **Warm Module** | 背包/技能书/任务进度 | EnterGame 立即加载,但可懒序列化 | 5-30 秒批量写 | 5-50 KB |
| **Cold Archive** | 邮件正文/战报详情/历史成就/聊天历史 | **按需加载**,玩家点开才拉 | 独立服务,不占主路径 | 不限 |

EnterGame 只加载 Hot + Warm,**Cold 完全不进 PlayerAllData**。
单玩家 EnterGame 加载量从"无上限"变成"上限 ~50KB",老号新号一致。

### 答案 2: **按模块分表**,不再有"player_database 大杂烩"

现在 `player_database` 是 1 张表 + 7 个 component 序列化进 1 个 blob。
行业做法:

```
player_basic        (id, name, level, exp, gold, diamond)  ← 强一致表
player_inventory    (player_id, item_id, count, attrs)     ← 行级存储
player_quest        (player_id, quest_id, progress, state)
player_skill        (player_id, skill_id, level)
player_mail         (player_id, mail_id, ...)
player_battle_log   (player_id, ts, ...)                    ← 写多读少
```

每张表独立分库分表,独立加载策略,独立持久化频率。

### 答案 3: 玩家数据按 **player_id 哈希分库**(物理分库分表)

千万级在线必须做的事:

```
player_id → hash(player_id) % 64 → db_shard_0 ... db_shard_63
```

每个 shard 独立 MySQL 实例,独立 Redis 分片。
这跟"分模块分表"是**两个正交维度**:
- 分模块: 一个玩家的不同数据放不同表(背包/任务/技能)
- 分库分表: 不同玩家放不同库(玩家 A 在 shard_3,玩家 B 在 shard_17)

两者都要做。

### 答案 4: **延迟加载 + 预测预取**

大部分模块可以延迟加载:
- 玩家进游戏 → 只加载 Hot Core,T+0 完成 EnterGame
- T+0.5s 后台开始拉 Warm Module(背包/技能)
- 玩家点开邮件 → 此时拉 Cold(邮件列表)
- 玩家点开具体邮件 → 拉邮件正文

预测预取减少等待感:
- 玩家走向 NPC → 预测会接任务,提前拉任务进度
- 玩家进副本入口 → 提前拉副本数据

---

## 我们项目的现实状况

### 好消息

1. `PlayerAllData` 字段编号已预留(`= 4 = 5 = 6`),proto 兼容性 OK,**未来加新模块不会破坏旧数据**。
2. 已有 `proto-compare-dirty-save.md` 设计,**已全套接通(2026-06-03 核对)**——
   IsEqual 原语 + LastPersistedSnapshotComp + SavePlayerToRedis fast-path +
   HandlePlayerAsyncSaved snapshot Replace + 30s LOG_INFO 跳过率,都在生产路径。
   实测降幅待 Round 17 跑出数据。
3. Round 16 单 zone 45k PASS,基础架构是稳的,可以在上面加层。
4. Mail 已经规划走独立服务(`cross-zone-readiness-audit.md §3.3`),思路是对的。

### 坏消息

1. `player_database` 是 7-component grab-bag,**短期内拆不开**(动了的话所有跨服迁移路径要重写)。
2. CurrencyComp **已经接进 PlayerAllData**(2026-06-03 核对),持久化路径打通,
   但崩溃窗口期 delta 仍会丢——需要 dirty-save + 独立 delta 写收敛丢失窗口。
3. 没有 delta 写原语,所有持久化都是全量 PlayerAllData 序列化。
4. 没有 Cold Archive 概念,邮件/战报全部进 PlayerAllData,老号必然慢。

---

## 建议的解决路径(分阶段,不是大重构)

### 阶段 0(本周内):验证 + 加固

**0.1 验证 currency 持久化丢失窗口**
- 已确认(2026-06-03)CurrencyComp 走 PlayerAllData → Kafka → MySQL 全链路,
  正常下线不丢。
- 还需测量的是**崩溃窗口**: 加金币 → 立刻 kill -9 scene → 重启 → 重新登录
  → 检查金币。
- 期望结果: 在两次 SavePlayerToRedis 之间崩溃,会丢失这段时间的 delta;
  数据要量化(丢几秒、丢多少金币)写进文档,作为 P2 dirty-save + 独立
  delta 写要收敛的窗口大小依据。
- **完整方案见 [currency-crash-window-verification.md](./currency-crash-window-verification.md)**
  (4 个测试 case + 工具栈 + 输出物 + 风险提醒)。

**0.2 测量加载耗时分布**
- 给 SavePlayerToRedis / loadPlayerData 加分 component 的 metrics
- 跑长期老号(模拟背包 1k+,任务 500+)的 EnterGame 看 P99
- 数据驱动决策:确认"老号慢"是真的还是猜的

### 阶段 1(2-4 周):金钱独立路径(对应 P2 已规划工作)

**1.1 ~~接入 CurrencyComp 到 PlayerAllData~~(已完成)**
- 2026-06-03 核对:`CurrencyComp currency = 8` 已在 player_database,
  marshal/unmarshal/MySQL schema 三端齐全,无需补接。

**1.2 新建 `player_currency_event` Kafka topic**
- 专门走金钱 delta 事件
- replication=3,retention=7 天
- db service 单独消费 → MySQL UPDATE 单字段

**1.3 ~~SavePlayerToRedis 接入 dirty-save~~(2026-06-03 已完成)**
- `proto-compare-dirty-save.md` slice A/B/C 全部落地
- `[DirtySave] total=X skipped=Y skip_pct=Z%` 30s 一行,
  `stress_summarize.ps1` section 2bb 自动解析
- 实测 Kafka 降幅(原估 70%)等 Round 17 跑出数据再确认

完成阶段 1: 金钱真零丢 + 进度可控丢 5-10 秒 + 现有架构不破坏。

### 阶段 2(1-2 个月):Cold 数据剥离

**2.1 邮件独立服务**(已规划)
- 邮件不再进 PlayerAllData
- EnterGame 不加载邮件
- 玩家点开邮件 UI → 走 mail service RPC

**2.2 战报独立服务**
- 新建 `battle_event` topic → ClickHouse
- 战报不进 PlayerAllData
- 玩家查战报 → 走 battle service RPC

**2.3 任务历史归档**
- `player_quest` 表分 active / completed
- completed 超过 30 天归档到冷存储
- EnterGame 只加载 active

完成阶段 2: 老号新号 EnterGame 耗时一致(<50ms)。

### 阶段 3(3-6 个月):水平分库分表

**3.1 player_id 哈希分库**
- MySQL 32-64 shard
- ProxySQL/Vitess 中间件
- Redis 也按相同规则分片

**3.2 player_locator 分片化**
- 现在是单点,千万在线必扛不住
- 按 player_id 哈希 → locator_shard_N

完成阶段 3: 多 zone 千万在线技术上可达。

---

## 关于 "PlayerAllData 是大杂烩" 的反思

设计者把 7 个 component 塞进一个 message,**短期是为了简化跨服迁移**(一个 proto
搞定所有数据),**长期是技术债**(没法做模块化优化)。

**这个不需要立即拆**,但每次加新模块时���要问:
- 这个模块需要 hot 加载吗?
- 这个模块的写频率高吗?
- 这个模块未来会无限增长吗?

三个问题都是 yes,塞进 PlayerAllData。
任何一个 no,做成独立 sub-message 或独立服务。

mail 的处理方式是对的(独立服务),应该作为后续模块的模板。

---

## 跟 OOM 防御 + 千万在线的关系

| 矛盾 | OOM 防御能解吗 | 千万在线必须解吗 |
|---|---|---|
| 全量加载慢 | ❌ Redis 容量再大也救不了序列化 CPU | ✅ 必须做冷热分离 |
| 找不到金钱 proto | ❌ | ✅ 必须做模块独立路径 |
| 数据无限增长 | ❌ | ✅ 必须做归档 / 冷存储 |

也就是说: **OOM 防御是续命,玩家数据加载架构升级才是治本**。
两件事不冲突,可以并行做,但不要混淆优先级。

---

## TODO 列表(对应到 docs/notes/todo_zh.md 加项)

- [x] ~~验证 CurrencyComp 持久化路径~~(2026-06-03 已核对,proto/marshal/MySQL 三端齐全)
- [ ] 量化 kill -9 崩溃窗口的金币丢失量(支撑 P2 dirty-save + delta 写设计)
- [ ] 给 SavePlayerToRedis 加 per-component metrics(数据驱动)
- [x] ~~接入 dirty-save fast path~~(2026-06-03 核对发现全套已接好;只差实测降幅)
- [ ] Round 17 跑出 dirty-save 实测跳过率与 Kafka 降幅
- [ ] 新建 player_currency_event topic 与 delta 写路径
- [ ] 邮件服务独立化(已规划在 cross-zone-readiness-audit.md)
- [ ] 战报独立服务设计(新)
- [ ] 任务历史归档策略(新)
- [ ] PlayerAllData 模块边界文档(让后续开发者知道什么放进去什么不放)

---

## 风险与提醒

1. **不要因为追求"完美架构"而停止当前业务开发**。阶段 0 → 阶段 3 是 6+ 个月的演进,
   期间正常加新功能,只是新功能要遵守"模块边界规范"。

2. **每个阶段必须有数据支撑**。不要凭感觉做优化,先 metrics 后改造。

3. **PlayerAllData 重构是高风险动作**,涉及:
   - 跨服迁移协议
   - rollback 快照
   - dirty-save 比较基线
   都依赖它的 proto 形状。任何重构必须有完整的数据迁移方案。

4. **冷热分离不是免费的**。Cold 数据走独立服务后,会出现一致性边界:
   - 玩家在战斗中收到邮件附件 → 邮件服务 + 玩家 inventory 跨服务事务
   - 这类问题必须在阶段 2 设计时一并处理。

---

## 附录: 用户的核心担忧逐条回应

| 用户原话 | 回应 |
|---|---|
| "一次性把玩家数据加载出来效率有些低" | ✅ 是事实,需要冷热分离 + 延迟加载 |
| "MMO 不一次性加载,改金钱找不到 proto" | ⚠️ 命名 / 可发现性问题(详见上文修正): CurrencyComp 已接进 PlayerAllData(`mysql_database_table.proto:103`),持久化无 bug;真正的痛点是字段名 `currency` 不直观 + `repeated uint64 values` 通过 type id 索引、没有"金币"具名字段 + 项目里有同名前缀的 `PlayerCurrencyComp` runtime comp 容易看错。 |
| "PlayerAllData 是各种玩家表数据" | ✅ 是大杂烩,长期需要模块化,短期不动 |
| "玩家越往后数据量越大,加载越慢" | ✅ 必须做归档 + 冷存储,否则 3 年后无法运营 |
