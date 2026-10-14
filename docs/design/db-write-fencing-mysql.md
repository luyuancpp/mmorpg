# 玩家存盘 MySQL 条件写（围栏）：go/db、C++、proto、schema 详细设计

> **状态**：设计稿 v0.2（2026-10-13，Claude）。已过一轮对抗评审（§17），**待负责人授权；未落码、未编译、未测试**（AGENTS.md §10.1）。
> **范围**：C++ scene → Kafka `db_task_zone_*` → go/db → MySQL 这条玩家存盘链。给两张玩家表加围栏列 `(fence_owner_epoch, fence_save_seq)`，go/db 改为条件写。
> **上游材料**：[db-task-pipeline-bottlenecks.md](db-task-pipeline-bottlenecks.md)（下称「瓶颈文档」）§4、§6.4、§6.6；[交接](../handoff/godb-pipeline-handoff-20261013.md) §4.1 第 3 项；[附录](../handoff/godb-pipeline-handoff-20261013-appendix.md) B「mysql-fencing」（其中已作废的条目见交接 §4.0，本文不采用）。
> **库原语**：proto2mysql 分支 `feat/conditional-save`（commit `e81a1e7`）的 `SaveConditional`。它未合入 main、未打 tag、未经 Codex 验证；本设计的实现以它验证通过并发版为前提。
> **证据口径**：[C] 在 HEAD `0aa1027bc` 读代码确认；[D] 文档记载；[I] 推断。行号按该 HEAD，之后以函数名为准。
> **缩写**：KOC = `go/db/internal/kafka/key_ordered_consumer.go`；PR = `go/db/internal/kafka/placement_route.go`；PL = `cpp/libs/services/scene/player/system/player_lifecycle.cpp`；P2M = proto2mysql 的 `conditional_save.go`。

---

## 0. 结论速览

1. **围栏是行内两列，按字典序比较。** go/db 把 DBTask 的 `(owner_epoch, save_seq)` 盖到要落库的消息上，用 `SaveConditional` 写：库内围栏更旧才写，判断与写入是同一条 UPDATE。旧的执行实例恢复后发出的写会被 MySQL 自己拒掉，不再依赖 Redis 排序锁的租约。
2. **首批只实现 `off` 与 `dual` 两种模式。** `dual` = 条件写 + 现有全套 Redis 守卫。删 Redis 守卫的 `sql` 模式（附录的 Stage 2）属于性能轨，本文只定契约和门禁（§8.5），不在首批实现。
3. **被拒的写分三种去向**：库里已有同版本或更新版本（`equal` / `older_seq`）→ 复核后 ack；归属更老（`stale_owner`）或无法判定（`legacy_after_seq`）→ 进死信并告警，保住载荷。没有任何一种是「不复核就丢」。
4. **save_seq 由 C++ 进程级发号器发出**，播种来自 Redis 父键和 MySQL 行两路，墙钟只作下限。不设每玩家组件（§7）。
5. **子表缓存不加键、不改值格式**：写路径改成「持有排序锁令牌才写」的单条脚本，读路径改成 `SET NX`（§6）。
6. **规模**：手写改动约 24 个文件，另有约 10 个生成物；跨 C++ / Go / proto / schema，每个库要做一次 DDL。开工需要负责人授权（交接 §5 第 2 项）。

### 0.1 相对交接与附录的变化

| # | 交接 / 附录的说法 | 本文的做法 | 原因 |
|---|---|---|---|
| 1 | C++ 加 `PlayerSaveSeqComp`，实体内计数 | 进程级发号器，实体不存序号 | 实体在同一 epoch 内会销毁重建（PL:3623-3628）；Redis 父键不是「已发出 DBTask」的上界（§7.1） |
| 2 | 播种取 `max(PlayerAllData.last_save_seq, 同 epoch 的 fence_save_seq)` | 两路都取，不看 epoch | 父键存在时 login 不读子表，C++ 拿不到 MySQL 行里的围栏（§7.2）；不看 epoch 只会把下限抬高，没有坏处 |
| 3 | 缓存回写带版本，建议放进值封装 | `dual` 下用排序锁令牌做原子校验；值封装留给 `sql` 模式 | `dual` 还持有排序锁，令牌校验已经够用，且不动 login（§6） |
| 4 | 三种模式一起实现 | 首批只做 `off` / `dual` | `sql` 是性能优化，立项判据未满足（瓶颈文档 §6.3） |
| 5 | G3「先判 epoch 再判 seq」 | 只对带 save_seq 的写放开：游标判过期时，若 epoch 领先就交给 SQL 裁决 | 不带 save_seq 的写在同一 epoch 内没有可比的版本，放开会让旧写覆盖新写（§5.2） |
| 6 | B1：`redis-cli FLUSHALL` 会让全部存盘被判 stale_owner | 触发面比预想小，但处置不变 | FLUSHALL 同时清掉账号目录，旧 player_id 不会再被用到（§9.1） |
| 7 | 能力心跳 `db:capability:zone` 可复用为机器门禁 | 不能复用，`sql` 模式要另建按实例的登记 | 该键每 zone 一把、值是常量，merge_zone 按精确相等判定（§8.5） |
| 8 | 缺省模式 `dual` | 缺省 `off`，逐 zone 改配置切 `dual` | 二进制发布与行为切换分开，可以单 zone 金丝雀（§8.3） |

---

## 1. 问题与目标

### 1.1 问题

- MySQL 写是无条件整行覆盖 [C]（KOC:829 `store.Save(msg)`；Save 的语句链是 UPDATE → INSERT → 1062 重试，没有版本条件）。
- 「同一玩家同一时刻只有一个 go/db 在写」只靠 Redis 排序锁的租约（TTL 2 分钟、30 秒续租，KOC:1843-1844）。续租失败只置一个标志，不取消在途 SQL；失租检查在 SQL 与缓存回写都做完之后（KOC:1631-1634）[C]。
- 所以旧实例卡住再恢复时，它手里的旧快照会把 MySQL 和子表缓存整行覆盖回去，没有报错，也不自愈（瓶颈文档 §4.3 场景 3，缺口 G1）。
- 游标先于 epoch 判定，新主的写在重试队列里时可能被老主的迟到写挤掉（缺口 G3）。游标与 epoch 记录缺键时放行（缺口 G5）。

### 1.2 目标（不变量）

| # | 不变量 | 靠什么保证 |
|---|---|---|
| F-1 | 同一玩家、同一张表、同一个库里，行内围栏只进不退；带 save_seq 的写里，行内容永远是已到达的写中围栏最大的那一笔 | 条件与写入在同一条 UPDATE 里，在主键行锁下完成 |
| F-2 | 被拒的写不留下任何副作用：不写缓存，不推进游标和已落库 epoch | 结局处理表（§4.4） |
| F-3 | 不静默丢盘：只有库里已有不低于本版本的数据、且复核确认该库仍是真源时才 ack 丢弃；其余无法落库的写都进死信并告警 | §4.4、§10 |
| F-4 | 子表缓存不比 MySQL 已落库的版本旧 | §6 |
| F-5 | 新旧 go/db、新旧 C++ 并存期间，安全性不低于今天 | §8.4 |

### 1.3 不在范围内

- 删 Redis 守卫、合并 Redis 往返（`sql` 模式与瓶颈文档 §6.3 的 3a / 3b）。
- 读写分区对齐（①）、重试只交给分区属主（④）、消费组按 zone 拆（③）。
- 连接不可用时延后（L-04）与死信工具：见 [db-task-store-unavailable-deferral.md](db-task-store-unavailable-deferral.md)。本文依赖它的死信告警（§8.3 的前置条件）。
- 两张子表之间的原子性：仍然是两条独立的 DBTask、两次独立的写。
- 「scene 发 DBTask 失败后脏比较快照不作废」（PL:3731-3737）：归属待定（交接 §5 第 17 项）。

---

## 2. 术语

**围栏**：行内的 `(fence_owner_epoch, fence_save_seq)`，按字典序比较，epoch 是高位。

**任务类别**（按 DBTask 判定，只对 `op == "write"`）：

| 类别 | 条件 | 来源 | 指标 label |
|---|---|---|---|
| class-1 | `owner_epoch > 0` 且 `save_seq > 0` | 新版 C++ | `seq` |
| class-2 | `owner_epoch > 0` 且 `save_seq == 0` | 没有 save_seq 的旧版 C++（滚动升级窗口） | `legacy_epoch` |
| class-3 | `owner_epoch == 0`（不看 save_seq） | 实体没有拿到 epoch 的兼容路径（PL:3687-3691）；`go/db/cmd/data_stress`（不填 OwnerEpoch，main.go:209-215）| `legacy_zero` |

当前 write 的生产者只有 C++ scene（PL:3715）和 data_stress [C]。

**模式**（配置 `Fencing.Mode`）：

| 模式 | 含义 |
|---|---|
| `off` | 紧急开关。写入语义等同今天：Redis 守卫照旧，SQL 无条件覆盖业务列，不碰已有行的围栏列 |
| `dual` | 条件写 + 现有全套 Redis 守卫 |
| `sql` | 预留。首批二进制读到这个值时拒绝启动 |

---

## 3. 数据契约

### 3.1 DBTask（`proto/db/db_task.proto`）

- 新增 `uint64 save_seq = 9;`。**字段号待交接 §5 第 13 项拍板**（全局单点交接想把 9 给 `reply_instance`）。9、10 号都从未用过 [C]，不存在复用问题；先落码的一方在注释里写明另一个号已预留。
- 语义：生产者按玩家发出的存盘序号，同一玩家只增不减；0 表示生产者没有发号。
- 顺带订正 8 号字段的注释（:18-19）：现在比对的是「已落库的最大 epoch」（`consumer:applied_epoch:*`，KOC:546-548），不是 `player:{key}:owner_epoch`。

### 3.2 玩家表（`proto/common/database/mysql_database_table.proto`）

| message | 新字段 | 现末尾 |
|---|---|---|
| `player_database` | `uint64 fence_owner_epoch = 18;`、`uint64 fence_save_seq = 19;` | `settlement_ledger = 17`（:154）|
| `player_database_1` | `uint64 fence_owner_epoch = 3;`、`uint64 fence_save_seq = 4;` | `stress_test_probe = 2`（:170）|

- 都声明在 message 末尾（加列纪律，:139-145）。18 / 19 目前没有被占用 [C]，落码前再 grep 一次。
- 注释写明：存储版本元数据，只由 go/db 按 DBTask 写入；C++ 只在加载时读来播种（§7.2），存盘时不写。
- **为什么必须是 proto 字段**：`SaveConditional` 的 `FenceFields` 只认该表 message 的顶层 uint32 / uint64 字段，栅栏值从消息字段读，没有「外部传入栅栏值」的入口（P2M:200-208、:264）[C]。另外三个好处是附带的：`cmd/migrate` 的 Drift 自动生成 ADD COLUMN；启动闸 `assertSchemaUpToDate` 缺列拒启（`proto_sql/db.go:380-390`），「先 DDL 后代码」由机器保证；读路径会把围栏一起读回。
- `player_centre_database` 也以 player_id 为主键，但没有任何 write 生产者 [C]，不加围栏。

### 3.3 PlayerAllData（`proto/common/database/player_cache.proto:30-37`）

- 新增 `uint64 last_save_seq = 7;`（现用 2~6；1 号历史上空着，不占用）。
- 语义：这份父键快照发出时用的 save_seq。只用于 C++ 加载时播种（§7.2）。
- 它是非业务元数据，**不得参与任何相等比较**（§7.3）。

### 3.4 帮会二期的加列放行清单

`docs/design/guild-phase2/90-consistency.md` 的「G-03 `player_database` 加列与迁移」（:297-309）要求：给 `player_database` 新增字段的那次改动，必须同时把新列追加进各处放行清单，否则那一批的执行者会在不认识的新列上停下。四个新列要追加到：

| # | 位置 | G-03 是否点名 |
|---|---|---|
| 1 | `guild-phase2/90-consistency.md:306`（G-03 第 3 条） | 是 |
| 2 | `guild-phase2/03-names.md` §3.15a（:598-602、:606-607），以及 §3.20 的验证命令（:708-709） | 是 |
| 3 | `guild-phase2/92-handoff.md` 步骤 4（:306-308、:313、:315） | 是 |
| 4 | `guild-phase2/04-asset-channel.md` §4.42 第 1b 步（:1643） | 否 |
| 5 | `docs/design/turn-battle-gap-closure.md` §9.3 验证清单第 1 步（:355-361） | 否 |
| 6 | `guild-phase2/90-consistency.md:327`（G-05 的列数说明） | 否 |
| 7 | `docs/ops/merge-zone-runbook.md` §4.4（:202-219） | 否 |

同时把 G-03 的「帮会剩余批次取号从 18 起」改为从 20 起。

### 3.5 DDL

- 由 `go/db/cmd/migrate` 生成并执行，不手写。Drift 对缺列生成 `ALTER TABLE … ADD COLUMN`，不带 `AFTER`，所以追加在末尾（`internal/migrate/plan.go:149-152`）。
- 预期 4 条语句，形如 ``ALTER TABLE `player_database` ADD COLUMN `fence_owner_epoch` bigint unsigned NOT NULL DEFAULT 0 COMMENT 'pb:18'``（uint64 的类型映射在 proto2mysql 的 `proto2mysql.go:486`）[I，未运行]。4 条会合成一条迁移 `auto_proto_sync_<sum>`。
- 每个 zone 库跑一次；每个落点库加 `-storage-id <id>` 再各跑一次。
- 存量行的默认值 `(0,0)` 表示「还没见过带版本的写」，不做回填。

---

## 4. go/db：条件写

### 4.1 入口

- `dbOpHandler`（KOC:327-332）的返回值分两步改：L-04 先改成 `error`；本设计再扩成 `(writeOutcome, error)`。两步都要同步改测试里的替身 `recordingHarness`（`key_ordered_consumer_test.go:130-155`），它按现签名整张替换 `dbOpHandlers`。
- `handleDBWriteOp`（KOC:810-833）不再丢弃 ctx：用 `store.WithContext(ctx)`（proto2mysql v0.2.0 就有，`proto2mysql.go:378`）把截止时间带到每条语句（§4.6）。
- 顺序：大字段闸（不变）→ 按类别给消息盖围栏（§4.2）→ `SaveConditional`（§4.3）→ 把库的结局归类成 `writeOutcome`（§4.4）。
- `route.store` 的类型仍是 `*proto2mysql.DB`（PR:234），不需要改成 `*GameDB`：状态查询由库自己做，go/db 不再手写 SELECT。

### 4.2 盖围栏

go/db 在调用前把围栏写进 dynamicpb 消息的两个字段：

| 模式 / 类别 | `fence_owner_epoch` | `fence_save_seq` |
|---|---|---|
| `dual` class-1 | `task.OwnerEpoch` | `task.SaveSeq` |
| `dual` class-2 | `task.OwnerEpoch` | 0 |
| `dual` class-3 | 0 | 0 |
| `off` | 0 | 0 |

C++ 每次都从组件全新构造消息（PL:3587-3595），body 里这两个字段恒为 0 [C]。如果 body 里出现非 0 且与 DBTask 不一致的值，说明有别的生产者或生产端有缺陷：打 ERROR、计数，以 DBTask 为准。

### 4.3 模式 × 类别 → `SaveOptions`

记 `fo = "fence_owner_epoch"`，`fs = "fence_save_seq"`。

| 模式 | 类别 | 调用 | 写入条件 |
|---|---|---|---|
| `off` | 任意 | `SaveOptions{PreserveFields: [fo, fs]}` | 无条件。已有行的围栏列不动；新行的围栏为 0 |
| `dual` | class-1 | `SaveOptions{FenceFields: [fo, fs]}` | 库内 `(fo, fs)` < `(E, s)` |
| `dual` | class-2 | 第一步 `{FenceFields: [fo, fs], AllowEqual: true}`；被 `(E, s>0)` 拒绝时第二步 `{FenceFields: [fo], AllowEqual: true, PreserveFields: [fs]}` | 第一步：库内 ≤ `(E, 0)`。第二步：库内 `fo` ≤ `E`，不动 `fs` |
| `dual` | class-3 | `{FenceFields: [fo, fs], AllowEqual: true}` | 库内围栏等于 `(0, 0)` |

说明：

- **`off` 不能调 `Save`。** 新二进制的消息里带着值为 0 的围栏字段，`Save` 会把已有行的围栏清零。`PreserveFields` 的语义是「更新已有行时不写这些列，插入新行时照常写」（P2M:62-65）[C]。只带 `PreserveFields` 时没有任何保护，这正是「等同今天」。
- **class-1 用严格模式。** 同版本重放的结局是 `SaveRejected` 且 `Stored` 等于消息围栏，调用方据此判为重复（P2M:57-59）。
- **class-2 的第二步**解决附录复核指出的问题：`<= E` 的条件写 `(E, 0)` 会把同一 epoch 内已有的 `(E, s>0)` 退回 `(E, 0)`。第二步只比 epoch，并保留库内的 seq，围栏不退。class-2 在同一 epoch 内的先后仍然靠 Redis 的排序锁与游标（§5.1）。
- **class-3 只在行上没有围栏时写**（修 B3）。行上已有围栏说明带版本的写已经落过，这时一笔没有任何版本的写无法判断新旧，不能覆盖。严格模式下全 0 围栏会被库判为非法选项（P2M:209-212），所以 class-3 必须带 `AllowEqual`。
- 不可能出现的组合（例如严格模式被拒但 `Stored` 比消息旧）返回错误、不 ack。

### 4.4 结局处理

| `writeOutcome` | 来自 | 落点复核 `route.confirm()` | 缓存回写 | 游标 / 已落库 epoch | 终态 |
|---|---|---|---|---|---|
| `applied` | `SaveUpdated` / `SaveInserted` / `SaveUnchanged` | 做 | 写 | 推进 | ack |
| `equal` | class-1 被拒，`Stored == (E, s)` | 做 | 写（补发上次可能没发成的缓存）| 推进 | ack |
| `older_seq` | class-1 被拒，`Stored.fo == E` 且 `Stored.fs > s` | **做** | 不写 | 不推进 | 复核通过 → ack 丢弃并计数；复核不过 → 延后重试（不耗预算） |
| `stale_owner` | class-1 / class-2 被拒，`Stored.fo > E` | 不做 | 不写 | 不推进 | **进死信**，计数，打 `STALE-OWNER-WRITE` |
| `legacy_after_seq` | class-3 被拒（行上围栏不是 `(0,0)`）| 不做 | 不写 | 不推进 | **进死信**，计数 |

错误的去向：

| 错误 | 含义 | 去向 |
|---|---|---|
| `ErrConditionalSaveRace` | 两轮内一直撞上并发写者 | 延后重试，不耗预算 |
| `ErrSaveMatchMismatch` | 整行比较与 MySQL 的「值是否变化」不一致，多半是表结构问题 | 延后重试，耗预算，最终进死信 |
| `ErrInvalidSaveOptions`、`ErrSchemaDrift`、`ErrMultipleRowsFound` | 配置或表结构错误 | 同上 |
| 语句已发出后的超时、连接断开 | 结局未知，那条语句可能已经生效 | 按 L-04 的分类延后；重试时 class-1 会得到 `equal` 或 `applied`，天然幂等 |

要点：

- **`older_seq` 先复核再 ack**（附录复核的修正）。判 `older_seq` 只证明「被选中的那个库」里有更新的版本，不证明那个库仍是真源。复核不过时按 `errPlacementMoved` 延后，重投到新库。
- **`stale_owner` 不 ack 丢弃**（修 B1）。正常情况下真正的僵尸写在前面的 Redis epoch 守卫就被丢掉了（KOC:667-682，行为不变）；能走到 SQL 才被判 `stale_owner`，说明 Redis 里的已落库 epoch 记录缺失或落后，这时无法区分「真僵尸」和「epoch 计数器倒退」（§9.1），所以保住载荷交给人判断。
- 进死信复用现有的 `quarantineOrderingConflict`（KOC:2145-2170）：Kafka 来源先 LPUSH 到死信再 ack；重试来源把收据原子搬进死信。
- `equal` 视同「同一份内容」。只有生产端给不同内容发了同一个 `(E, s)` 才会出错，§7 的发号规则保证不会发生；`cmd/verifier` 可以用来事后核对。

### 4.5 `runDBOp` 的顺序

「选库 → SQL → 复核 → 缓存 / 读结果发布」的顺序不变（KOC:2035-2050）。变化只有两处：

- handler 返回 `stale_owner` / `legacy_after_seq` 时，`runDBOp` 直接把结局交回 `handleTask`，不复核、不发布。
- handler 返回 `older_seq` 时，复核照做，但不发布。

复核 MGET 不能因为有了围栏而省掉：围栏是按库内的行算的，看不出「这个库已经不是真源」（`player-storage-placement.md` §6.3）。

### 4.6 超时与取消

- 语句截止时间 = `min(处理上下文, 配置的语句超时)`，经 `WithContext` 传入。处理上下文在失租时取消（瓶颈文档 §4.4 G1 治标第 ② 条）。
- `newMysqlConfig`（`proto_sql/db.go:108-121`）目前只设了 `sql_mode`，没有 Timeout / ReadTimeout / WriteTimeout [C]。要补上，作为不经 ctx 的路径（读、迁移）的兜底。
- 这两条属于交接 §4.1 第 2 项（G1 治标小 PR）。它先落地，本设计复用它的配置键；它没落地，本批一起引入。
- 语句被取消或超时后，驱动会关掉那条连接，服务端可能仍把语句执行完。这类错误一律按「结局未知」处理：延后重试，靠围栏幂等。**治标措施撤不回已经进入 MySQL 的语句，挡住旧写的是围栏。**

### 4.7 配置

```yaml
Fencing:
  Mode: off          # off | dual；缺省 off
```

- go/db 的配置没有独立的校验函数，默认值统一在 `Normalize()` 里补（`internal/config/config.go:284-335`）[C]。`Mode` 用 go-zero 的 `options=off|dual` 约束（同 `AllowlistEnforcement` 的写法，config.go:121），空值在 `Normalize()` 里补成 `off`。写了 `sql` 或其它值即拒绝启动。
- K8s 上 db 的 ConfigMap 模板（`tools/scripts/k8s_deploy.ps1` 的 `New-GoSvcConfigMapYaml` 里 `"db"` 块，:3431-3474）不会自动带上 `db.yaml` 的新键 [C]，必须手改模板并在 `tools/scripts/tests/k8s_deploy_contract.tests.ps1` 加断言。该文件目前有另一个会话在改，落码前先对齐。

---

## 5. go/db：与 Redis 守卫的关系（`dual`）

### 5.1 保留什么

排序锁、游标（`consumer:applied:*`）、已落库 epoch（`consumer:applied_epoch:*`）全部保留，键名和值格式不变。原因：

- 旧版 go/db 只认这三样，新旧实例并存时靠它们互斥和判新旧。
- class-2 / class-3 在同一 epoch 内没有可比的版本，先后只能靠锁和游标。
- 对 class-1，它们是便宜的去重：重复投递在游标处就被丢掉，不用把整行发到 MySQL。

Redis epoch 守卫判 stale 时的处置不变（ack 丢弃，KOC:667-682）。

### 5.2 游标判过期时让 epoch 说话（G3）

现状：`orderingForTask` 判 `orderingStale` 即 ack 丢弃（KOC:1579-1588），之后才轮到 epoch 守卫。新主的写在重试队列里、老主的迟到写先落库并推进游标时，新主的写回来会被判过期丢掉。

改法（只在 `dual`、只对 class-1）：

| 游标 | 已落库 epoch 的判定 | 处理 |
|---|---|---|
| apply | — | 照旧 |
| stale | `match`（相等）或 `stale`（更小）| 照旧丢弃。同一 epoch、同一分区内，offset 更小就是更早发出的写 |
| stale | `advance`（任务 epoch 更大）或 `first`（没有记录）| **不丢**，继续走到 SQL，由围栏裁决 |
| incomparable | — | 照旧进死信 |

- 走这条路成功落库后，**不推进游标**（任务的 offset 比游标小，SET 会让游标倒退），照常推进已落库 epoch。
- 只对 class-1 放开的原因：class-2 没有 seq，围栏条件是「库内 ≤ `(E, 0)`」。已落库 epoch 的记录可能落后（`markAppliedOwnerEpoch` 失败只记日志，KOC:698-706），这时一笔同 epoch 的旧 class-2 写会被误判成 `advance`，放过去就覆盖了更新的 class-2 写。class-1 的围栏是精确的，放过去最多多花两条语句。
- `off` 模式不放开：没有围栏兜底。
- 新增计数 `db_ordering_guard_total{outcome=apply|stale|incomparable|epoch_override}`，现在 stale 只有一行 INFO 日志。

### 5.3 批内合并跟着围栏走

`processTaskBatch` 对同一 `(key, msgType, originPartition)` 保留 offset 最大的一条，其余标记为被取代并 ack（KOC:1461-1470、:1485-1499）[C]。

改法：两条都是 class-1 且 epoch 相同时，保留 `save_seq` 更大的那条（相同则保留后到的）；其它情况照旧。

- 正常情况下两种比法结果相同：C++ 生产者开了 `enable.idempotence`（`kafka_producer.cpp:59-76`），同一分区内 offset 顺序就是发送顺序。
- 这条改动是为了让合并与围栏用同一把尺子。否则一旦两者不一致，被保留的那条会被围栏判 `older_seq`，被取代的那条已经 ack，两条都没落库（附录 B2 指出的叠加）。

### 5.4 失租

现状：写完之后发现失租，按普通失败耗重试预算（KOC:1631-1634）。改为不耗预算（新增哨兵 `errOrderingLeaseLost`，并入 `retryBudgetExempt`）。理由：失租不是这笔写的问题；重试时 SQL 得到 `equal`，再在新的锁下补发缓存、推进游标。

---

## 6. 子表缓存

### 6.1 现状

- 键 `{MsgType}:{Key}`，TTL 3600 秒，普通 `SET`，没有版本也没有条件（KOC:54-64、:846-860）[C]。
- 写入点只有 go/db 两处：读路径 KOC:369、写路径 KOC:854。读取方是 login（`ensure_player_all_data_async.go:80-81`、`sync_loader.go:56-58`）和 `cmd/verifier`。删除方只有 `tools/merge_zone/player_rows.go` 的 `invalidatePlayerCaches` [C]。
- 两个缺口：写路径的缓存回写在核锁之前（旧实例恢复后会把缓存写回旧值）；读路径不拿锁、无条件覆盖（瓶颈文档 G4）。
- 父键缺失时 login 用子表缓存拼 `PlayerAllData`，所以缓存变旧会被固化成回档。

### 6.2 `dual` 下的做法

**写路径**：`SET` 换成一条脚本，原子地「核对排序锁令牌 → 写缓存」。

```lua
-- KEYS[1] 排序锁键  KEYS[2] 子表缓存键
-- ARGV[1] 锁令牌    ARGV[2] 值          ARGV[3] TTL 秒
if redis.call('GET', KEYS[1]) ~= ARGV[1] then
  return 0
end
redis.call('SET', KEYS[2], ARGV[2], 'EX', ARGV[3])
return 1
```

- 返回 0 = 已经不持有锁：不写缓存，按 `errOrderingLeaseLost` 延后（§5.4）。重试时拿到新锁，SQL 得到 `equal`，再补发缓存。
- 脚本用 `redis.NewScript(...).Run`（EVALSHA，NOSCRIPT 时自动回落 EVAL）。往返数不变（一次 SET 换成一次 EVALSHA）。
- 令牌在 `locker.TryLockResult` 里是私有字段（`go/db/internal/locker/locker.go:20-25`），给它加一个方法（例如 `SetIfOwned`），不把令牌暴露出去。
- 为什么这是真栅栏而不是又一层租约：校验与写入在 Redis 里是同一个原子步骤，而缓存本身就在这台 Redis 上。MySQL 做不到这一点（它看不到 Redis 的令牌），所以 MySQL 侧才需要围栏列。
- 被拒的写本来就不发布缓存（§4.4），所以能让缓存变旧的只剩「SQL 已落、缓存迟发」这一种，令牌校验正好挡住它。

**读路径**：`SET` 换成 `SET NX`。

- login 只在子表缓存缺失时才发读任务（`ensure_player_all_data_async.go:80-87`），正常情况下 NX 一定成功。
- 读与写并发时，读到的旧行写不进已有的新缓存；即使读先写进去，写路径随后的发布会覆盖它。
- 残余窗口：读到旧行之后、NX 之前，缓存键恰好过期或被合服工具删掉。这个窗口换成带版本的写法也存在（键不在时无从比较），可以接受。

这两处改动与 `Fencing.Mode` 无关，`off` 下也生效（只会更严）。它同时就是瓶颈文档 G1 治标第 ③ 条「缓存回写挪到核锁之后」的原子版本；G1 小 PR 若先落地，这里直接复用。

### 6.3 值里自带围栏

go/db 盖过围栏的消息原样进缓存，所以缓存值（序列化的 `player_database`）里带着 18 / 19 号字段。login 不需要改：它把整条子表消息原样拼进 `PlayerAllData`（`ensure_player_all_data_async.go:80-83`、:178-180）。C++ 冷加载时据此播种（§7.2）。

### 6.4 留给 `sql` 模式的问题（本批不做）

`sql` 模式下 class-1 不再取排序锁，令牌校验用不了，缓存必须自带可比较的版本。方向与交接要求一致：

- **不另开 `{MsgType}:{Key}:fence` 键。** 两个键必须同生同灭，任何只删或只写其中一个的工具都会造成缓存回退；Redis 分片后还要求两个键同槽。
- **首选值封装**：在缓存值前面加定长头 `0x00 | 版本 | epoch(8 字节大端) | seq(8 字节大端)`。合法的 protobuf 编码不会以 `0x00` 开头，新旧格式可以区分。一条脚本用 `GETRANGE` 取头比较后再写，单键、原子。代价是 login 要先能读两种格式，再让 go/db 写新格式，这是一次跨服务的格式升级。
- **与 Redis 分片方案（交接 §5 第 16 项、D12 / CZ-2 改写）的关系**：
  - 值封装是单键，不需要 hash tag。
  - §6.2 的脚本同时访问排序锁键和子表缓存键。今天主 Redis 是单实例，没有问题。如果以后按 `{player_id}` 分片，这两个键必须落在同一个 tag 下，排序锁键的改名还要与 `tools/merge_zone/relocate.go:367-373` 的锁键镜像同批改。请在裁决第 16 项时把这一条算进去。

---

## 7. C++：save_seq 发号

### 7.1 约束

- 同一玩家、同一 epoch 内，后发出的存盘必须拿到更大的 save_seq。
- **实体会在同一 epoch 内销毁重建**：同落点重连不铸造 epoch（`go/scene_manager/internal/logic/owner_epoch.go:41-46`），而实体可能已被清掉后重新加载（PL:3623-3628）。次数没有上界 [C]。
- **Redis 父键不是「已发出 DBTask」的上界**：DBTask 在 `Save()` 入队之后立刻发出，不等 Redis 应答（PL:3683-3741）；Redis 那次写可能还在排队、被后一次存盘顶替、或被 CAS 拒掉 [C]。
- **scene 进程重启后 epoch 不一定推进**：新进程可能复用同一个 node_id，而 location 仍指向它时走「不铸造」分支（`enterscenelogic.go:514-517`、:643）[C]。这条时序在真实部署下是否可达没有核实。
- 今天这条路径按 broker 分配的 offset 定序，与时钟无关。发号不能把「时钟回拨」变成新的静默丢盘（附录 B2）。

### 7.2 发号器

**进程级发号器，不设每玩家组件。**

```
Next(nowMs)      = last = max(last + 1, nowMs << 10)
Observe(floor)   = last = max(last, floor)
```

- `last` 是 scene 线程上的一个 `uint64`，整个进程共用。它对这个进程里的所有存盘严格递增，所以同一玩家的任意两次存盘（不管中间实体重建过几次）都有序。
- 发出的值在玩家之间共用一条数轴，每个玩家看到的是有空洞的递增序列。围栏只要求递增，不要求连续。
- 实例不用 static：挂在 scene 线程已有的全局上下文上，按仓库现有的系统对象惯例放置。纯函数 `NextSaveSeq(last, nowMs)` 单独放在 `cpp/libs/services/scene/player/system/save_seq.h`，时间显式传入，方便单测。
- `nowMs << 10` 约为 2^51，离 uint64 上限很远。

**播种**：`InitPlayerFromAllData`（PL:3495）加载玩家时调用一次

```
Observe(max(all.last_save_seq(),
            all.player_database_data().fence_save_seq(),
            all.player_database_1_data().fence_save_seq()))
```

两路来源互补：

| 加载路径 | `last_save_seq` | 子表里的 `fence_save_seq` |
|---|---|---|
| 父键存在（常态：C++ 存过一次之后父键没有 TTL）| 上一次落到 Redis 的存盘用的序号 | 0（C++ 存盘的 body 不带围栏）|
| 父键缺失，login 从子表缓存或 MySQL 拼出来（冷路径）| 0 | MySQL 行里已落库的序号 |

- 不看 epoch 是否相同：把下限抬高没有坏处，而且少一个分支。
- 现有的 `PlayerDatabaseMessageFieldsUnmarshal` 只读显式列出的字段（`player_database_loader.cpp:84-138`），读这两个围栏值要新增代码。C++ 只读不写。

**发号**：在 `SavePlayerToRedisImpl` 里，脏比较快路径之后（PL:3633-3642）、`Save()` 之前（PL:3683）：

```
saveSeq = Next(TimeSystem::NowMillisecondsUTC())
message->set_last_save_seq(saveSeq)        // Save() 会立即序列化，必须在它之前
…
dbTask.set_save_seq(saveSeq)               // 两张子表共用同一个值（PL:3721 附近）
```

- 被快路径跳过的存盘不消耗序号。
- 两条 DBTask 各自独立发送，可能一成一败（PL:3731-3741）；两张表各有自己的围栏，互不影响。

### 7.3 不让序号污染相等比较

`last_save_seq` 每次写盘都变。快路径比较的是「这次新构造的消息」对「上次落地的快照」（PL:3633-3635），快照是落地的那份对象（PL:2274-2275）。不处理的话，快照里带着序号、新消息里是 0，快路径永远不命中，49–60% 的存盘跳过率归零。

处理（与压测探针在退出收敛比较里的做法相同，PL:794-804）：

- `PlayerLastPersistedSnapshotComp::Replace`（`last_persisted_snapshot_comp.h:56-64`）是深拷贝，在拷贝之后清掉 `last_save_seq`。两个调用点（PL:2228、:2275）都经过它。
- `player_exit::IsPersistedPayloadCurrent`（PL:794-804）在两份副本上都清掉 `last_save_seq`。否则只要有两笔存盘在途，「落地的」和「当前的」序号不同，退出收敛永远判不相等。
- 单测必须覆盖：连续两次无改动的存盘，第二次命中快路径。

顺带记录一个现状 [I，未验证]：`STRESS_TEST_PROBE=1` 时探针字段进了快照但不在比较侧，快路径按阅读推断永不命中，与 PL:3604-3612 注释的意图不符。它影响压测口径，不在本设计范围内，请负责人决定是否另开任务。

### 7.4 残余风险

进程级发号器 + 两路播种之后，墙钟只在「新进程 + epoch 没推进 + 父键落后于已发出的序号」时才起作用。会出错的条件要同时满足四条：

1. scene 进程崩溃，崩溃前有存盘的 DBTask 已发出但 Redis 那次写没落地；
2. 同一个 node_id 的新进程接回这名玩家，且 scene_manager 走了不铸造分支；
3. 父键没丢（丢了会走冷路径，从 MySQL 行播种）；
4. 主机时钟回拨的幅度超过了停机时长。

后果：新进程的存盘被判 `older_seq`，ack 丢弃，直到序号追上。缓解与观测：

- `db_fence_write_total{outcome="older_seq"}` 建基线告警（§10）。
- 要求 scene 主机的时钟同步用 slew 不用 step（附录 M6，待运维确认）。
- 彻底关掉的办法是让 scene_manager 在进程换代时必定铸造 epoch（`PlayerLocation` 现在没有进程代际字段）。这是跨 zone 归属那条线的改动，列为待定项（§14 第 8 条）。

### 7.5 单测

放在 `cpp/tests/cross_zone_test/cross_zone_test.cpp`（单编译单元，只走 MSBuild，没有 CMakeLists）：

- `NextSaveSeq`：单调；时钟回拨仍单调；同一毫秒多次调用。
- `Observe` 之后的下一个值大于下限。
- 加载带 `last_save_seq` / `fence_save_seq` 的 `PlayerAllData` 后发号器的下限。
- 快照不保留 `last_save_seq`；连续两次无改动存盘第二次命中快路径；`IsPersistedPayloadCurrent` 不受序号影响。

测试宿主没有 Redis，`SavePlayerToRedisImpl` 的写盘段跑不到（现有用例只走快路径或直接调落地回调）。「DBTask 里带上了 save_seq」这一点靠本地端到端验证（§11 第 7 步）。

---

## 8. 上线分期与兼容

### 8.1 Stage 0：proto 与 DDL

1. 改三个 proto（§3），重新生成（§11 第 1 步），同一次改动里更新 §3.4 的七处清单。
2. 对每个 zone 库和每个落点库跑 `cmd/migrate -command plan`，核对只有 4 条 ADD COLUMN，再 `-command up`。
3. 这一步之后、新二进制上线之前，旧 go/db 照常运行：`Save` 只写它认识的列，围栏保持 0。旧二进制会对每个库持续打「多余列」的 `SCHEMA-DRIFT` ERROR 日志（`proto_sql/db.go:366-368`），是预期的噪声。
4. `tools/merge_zone` 按列名拷行，要求两库列集合完全相同（`player_rows.go:463-508`）。所有库迁完之前不能合服或搬库；它自己会拒绝，方向安全。

### 8.2 Stage 1a：新 go/db，`Mode=off`

- 新二进制带着新 proto，启动闸会要求库里已有围栏列，否则拒启。
- `off` 下唯一的行为变化是：SQL 从 `Save` 换成只带 `PreserveFields` 的 `SaveConditional`（语句相同，影响 0 行时多一条存在性查询）；缓存回写改为 §6.2 的写法。
- 这一步是为了把「换二进制」和「换语义」分开。

### 8.3 Stage 1b：逐 zone 切 `dual`

前置条件（缺一不可）：

- 该 zone 的 go/db 全部是新二进制。
- **死信长度的指标和告警已经生效**（L-04 设计第 1 批），并且 K8s 上 go/db 的指标端口抓得到（db ConfigMap 的 `MetricsListenAddr`，属另一会话的 G2b，目前没有）。否则 `stale_owner` / `legacy_after_seq` 进死信就是另一种静默。
- 一条金丝雀 zone 先切，观察 §10 的指标至少一个高峰。

切换：改配置后滚动重启。只改 ConfigMap 不会让 Pod 滚动，要显式 `rollout restart`，并用启动日志里的生效值核对。

### 8.4 Stage 1c：新 C++

新 C++ 开始发 `save_seq`，class-1 流量出现。它可以早于或晚于 Stage 1b：`off` 下 go/db 不看 save_seq。

**并存矩阵**：

| 并存情况 | 结果 |
|---|---|
| 旧 go/db + 已加列的库 | 旧实例只写旧列，围栏不动。安全性同今天 |
| 新 go/db（`dual`）+ 旧 go/db | 旧实例的无条件覆盖 MySQL 挡不住，互斥仍靠双方都遵守的排序锁、游标、epoch 键。旧实例不推进围栏，围栏只会落后，不会误拒新写。安全性同今天 |
| 新 C++ + 旧 go/db | 旧实例不认识 9 号字段，按未知字段原样保留并带进重试载荷。安全性同今天 |
| 旧 C++ + 新 go/db（`dual`）| 走 class-2 |
| 同一 zone 内 `off` 与 `dual` 实例并存 | 都保留 Redis 守卫；`off` 实例的写不带条件，等同上面「新旧 go/db 并存」|
| C++ 从新版回滚到旧版 | `dual` 下安全：之后的写是 class-2，走第二步，不退围栏 |

### 8.5 `sql` 模式的契约（预留，本批不实现）

将来要做时必须满足：

- **机器门禁**，不能只靠发布纪律：`sql` 实例不维护游标和已落库 epoch，任何还按它们判定的实例（旧二进制、`dual`）读到的是停更的值。
- 现有的 `db:capability:zone:{zone}` **不能复用**：它每 zone 一把键、所有副本用同一个常量覆盖，`tools/merge_zone` 按值精确相等判定（`placement_ops.go:424-459`），改值会让现有的合服门禁判为缺失 [C]。`kafka:retry:instances:{topic}` 按实例登记，但不带任何能力信息，过期成员也不自动摘除 [C]。需要新建一份按实例的能力登记：实例只有看到登记表里的所有成员都具备能力时才真正按 `sql` 执行，否则自动按 `dual` 运行并告警。
- 进入条件：class-2 / class-3 计数在观察窗内为 0；该 zone 的重试与死信队列清空过；缓存改用 §6.4 的值封装。
- 回退只能经过 `dual`。
- 立项判据见瓶颈文档 §6.3。

### 8.6 回退

| 回退 | 做法 |
|---|---|
| `dual` → `off` | 改配置滚动重启，随时安全。围栏列留着，`off` 不碰它们 |
| 新 go/db → 旧 go/db | 随时安全（Redis 守卫一直在维护）。旧代码忽略围栏列 |
| 新 C++ → 旧 C++ | `dual` / `off` 下安全 |
| DDL | 不回滚，不 DROP 列 |

`off` 运行一段时间后再切回 `dual`：`off` 期间的写不推进围栏，围栏落后于真实数据，只会放宽条件，不会误拒。

---

## 9. 运维流程

### 9.1 epoch 计数器倒退（B1）

**机制**：owner_epoch 只存在 Redis（`player:{id}:owner_epoch`，`go/shared/ownerepoch/ownerepoch.go:49-51`），缺键时从 0 起 INCR（`owner_epoch.go:104-107`）。围栏把「已落库的最大 epoch」持久在 MySQL。如果 Redis 里的计数器倒退而 MySQL 行还在，这名玩家之后的存盘都会被判 `stale_owner`。

**实际会触发的场景**（按 HEAD 取证）：

| 场景 | 是否触发 | 依据 |
|---|---|---|
| `redis-cli FLUSHALL`，不清 MySQL | 基本不触发 | 账号目录也只在 Redis（`loginlogic.go:319`），清空后同一账号会重新建角、得到新的 player_id，旧行成为孤儿，不会再被写到 |
| 在线玩家所在的 Redis 被清 | 不触发 | C++ 带守卫的存盘在守卫键缺失时会按节点缓存的值补种（`redis_client.h:42-48`）|
| Redis 从旧备份恢复、AOF 丢尾、主从切换丢尾 | **触发** | player_id 保留，计数器回到旧值。HA 清单 `redis-ha.yaml` 目前没有接进部署脚本，单实例 AOF 走默认的 everysec |
| 手工删除个别 `player:{id}:owner_epoch` 键 | **触发** | — |
| `k8s_zone_rollback.ps1` 带 `-ConfirmFlushSharedRedis` 的 FLUSHDB | 与 FLUSHALL 相同 | 它清的是共享实例的 DB 0，所有 zone 的账号目录一起没。这本身就是事故 |

**处置**：

1. `stale_owner` 进死信并告警（§4.4、§10），载荷不丢。
2. 人工确认是计数器倒退而不是真僵尸之后，把受影响玩家的围栏归零，再把死信里的载荷重新入队。
3. 归零必须在**该 zone 的 db_task 积压和重试队列排空之后**做。否则倒退之前发出的、带着更大 epoch 的在途任务会在归零之后落库，盖掉更新的数据。
4. 归零的入口建议放在 `go/db/cmd/migrate`，加一个子命令（例如 `-command fence-reset`，必须带确认开关，可按玩家或全表）：它已经有按 zone / 落点库定位目标、库名白名单和超时控制。脚本和 runbook 只调用这个入口，不各自内嵌 SQL。**这是建议，待评审**（§14 第 5 条）。

**要同步的文档**：

- `AGENTS.md` §6.2：是否加一句「只清 Redis、不清 MySQL 且会复用 player_id 时，先归零围栏」。按上表，现有压测流程不会触发，这一句是提醒性质；改 AGENTS.md 需要负责人同意（交接 §5 第 14 项）。另一会话也提了一条对 §6.2 的修改（清全局库路由表），建议合成一次修改。
- `docs/ops/merge-zone-runbook.md` §9.2「Redis 整体丢失」的恢复步骤、`docs/design/zone_data_rollback.md` 的回档步骤：各加一步围栏处置。
- 交接里提到的「Redis HA 故障 runbook」在 `docs/ops/` 下不存在；`dev_tools.ps1` 也没有独立的清 Redis 命令 [C]。

**长期**：让 epoch 的铸造有持久下限。交接写的是「并入 D1 映射落库」，但 D1 提案（`player-route-mapping-durability-proposal.md`）里没有对应条目 [C]，需要与那条线的负责会话确认是否接（§14 第 7 条）。

### 9.2 回档（PITR）

- MySQL 恢复到旧时间点后，围栏随数据一起回退。
- 回档流程如果同时清了 Redis，游标和已落库 epoch 也没了，比恢复点更新的在途任务会通过围栏落库，把回档结果冲掉。
- 所以回档 runbook 必须保证：重启 go/db 之前，该 zone 的 db_task 位点已经跳过恢复点之后的全部消息，`kafka:retry:*` 与死信里该 topic 的条目已清理，子表缓存已删除。
- 现有脚本和 runbook 都没有这一步 [C]：`k8s_zone_rollback.ps1` 的 Step 5 用 `--to-datetime` 重置位点，方向存疑；共享消费组下这一步通常直接失败。这与交接 §4.2 的「回档 Step 2」是同一件事，建议在那一项里一并改。

### 9.3 合服、搬库、导入导出

- `tools/merge_zone` 按列名拷行，围栏随行一起走；逐列相同的判据（`identicalRowCondition`）自动包含新列。不需要改代码。
- 搬库（relocate）之后落在旧库的写会重投到新库，在新库上按围栏裁决（`player-storage-placement.md` §6.3 的 LWW 语义不变，判据从游标换成围栏）。R2 等排序锁的逻辑在 `dual` 下不变。
- 缓存键没有新增，`playerCacheKeys` 不用改。
- `go/data_service/cmd/debug_import` 会把导出时的围栏值一起写回。从别的环境导入的行如果围栏比本环境的 epoch 大，之后的存盘会进死信，按 §9.1 归零。

### 9.4 死信里的 `stale_owner` / `legacy_after_seq`

死信目前没有消费者也没有重放工具 [C]。这两类条目需要人看、需要能重新入队，依赖 L-04 设计第 2 批的死信工具。在工具落地之前，处置只能靠手工 Redis 命令，写进 runbook。

---

## 10. 可观测

**指标**（低基数，不带 player_id）：

| 指标 | label | 用途 |
|---|---|---|
| `db_fence_write_total` | `class` ∈ seq / legacy_epoch / legacy_zero；`outcome` ∈ updated / inserted / unchanged / equal / older_seq / stale_owner / legacy_after_seq / race / error | 围栏结局分布 |
| `db_ordering_guard_total` | `outcome` ∈ apply / stale / incomparable / epoch_override | 游标守卫分布；`epoch_override` 是 §5.2 的新路径 |
| `db_fence_cache_publish_total` | `result` ∈ written / not_owner | §6.2 的令牌校验 |
| `db_fence_body_mismatch_total` | — | body 里带了与 DBTask 不一致的围栏（§4.2）|

现有的 `db_stale_owner_write_rejected_total`、`db_owner_epoch_guard_total` 保留；SQL 判出的 `stale_owner` 也计入前者，告警口径不变。

**告警**（追加到 `deploy/k8s/owner-epoch-alerts.yaml`，写法同现有两条）：

| 告警 | 条件 | 级别 |
|---|---|---|
| `DbFenceStaleOwner` | `outcome="stale_owner"` 有增长 | critical |
| `DbFenceLegacyAfterSeq` | `outcome="legacy_after_seq"` 大于 0 | critical |
| `DbFenceOlderSeqHigh` | `older_seq` 占 class-1 写的比例超过基线 | warning，阈值待压测确认 |
| `DbFenceBodyMismatch` | 大于 0 | warning |

该文件里记了一条经验：CounterVec 的序列从无到 1 时 `increase` 会漏第一次，所以关键告警用绝对值（:57-62）。新规则照此处理。

**日志**：`stale_owner` 沿用 `STALE-OWNER-WRITE` 前缀并写明来源是 SQL 围栏、行内围栏值；`legacy_after_seq` 用新前缀。

**前提**：仓库不部署 Prometheus，告警文件是手工 apply；K8s 上 go/db 的指标端口目前没开（§8.3）。

---

## 11. 测试与 Codex 验证

前提：Claude 没有编译、没有运行任何测试，下面全部由 Codex 执行。任一步失败就保留错误和日志摘要后停下，不连续重试、不改断言、不跳过测试。

**0. 前置**：proto2mysql `v0.3.0` 已发版；`go/db/go.mod` 的 require 与 replace 两侧都升到 v0.3.0（交接 §3.3）。

**1. proto 重新生成**（做法待负责人确认，§14 第 6 条）

- `go/build.bat` 第 3 行是 `pause`，而且它调用的 `build.ps1` 只跑 `goctl rpc protoc`、修 import、删副本，**不重新生成 `.pb.go`** [C]。真正的重新生成是 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run`（`proto/AGENTS.md:40-42` 列了三条命令）。AGENTS.md §4.1 只写了 `build.bat`，与实际不符。
- `proto-gen-run` 的默认配置开着 `enable_unity_client`，会写同级的客户端仓库（`proto_gen.yaml:243`、:29-31），与 AGENTS.md §9 的客户端边界冲突。
- 通过标准：`git diff` 只出现三个 proto 对应生成物里的新增字段（`cpp/generated/proto/…`、`go/proto/…`、`generated/proto/…` 与 `generated/robot/proto/…` 下的副本、`robot/vendor/proto/db/db_task.pb.go`），没有无关文件。加字段不产生新文件，CMakeLists 与 vcxproj 不用改 [C]。

**2. Go 单测**（不依赖外部服务；工作目录 `go/db`）

```sh
go vet ./...
go test ./internal/kafka/... ./internal/logic/pkg/proto_sql/... ./internal/config/... ./internal/metrics/... ./internal/locker/... -count=1
```

新增用例必须覆盖：

- 围栏结局归类全表（§4.4 每一行，含不可能分支报错）。
- 模式 × 类别的 `SaveOptions`（§4.3），含 class-2 的两步、`off` 不清围栏。
- `handleTask` 级别（miniredis）：`older_seq` 先复核、复核不过则延后；`stale_owner` 与 `legacy_after_seq` 进死信且不写缓存、不推进游标；`equal` 补发缓存。
- §5.2 的四种组合；`epoch_override` 成功后游标不倒退；`off` 下不放开。
- §5.3 批内合并按 save_seq。
- 缓存脚本：持锁写入、失锁返回 0 并延后且不耗预算；读路径 NX。
- 配置：缺省 `off`；非法值拒启。

需要的测试设施：`go/db/internal/dbtest/fakedriver.go` 的影响行数写死为 0（:116），要改成可配；现在没有任何测试调用真实的 `handleDBWriteOp`，本批补上。

**3. 真库集成**（新增，环境变量门控；工作目录 `go/db`）

- 用 **go/db 自己的连接配置**（`newMysqlConfig`）连库。proto2mysql 的真库用例强制开了 `InterpolateParams`，走的是客户端内插；go/db 不开，走服务端预处理。库的用例没有覆盖 go/db 实际的参数绑定路径 [C]。
- 场景：两个连接池、20 路并发乱序写同一玩家，最终值是最大围栏；同版本重放得到 `equal`；并发首写；`off` 写过之后围栏不变；class-2 两步；class-3 在有围栏的行上被拒。
- 后端：MySQL 8.4 与 TiDB 8.5 各一遍。拉镜像属于下载，先征得负责人同意。
- 通过标准：全绿，零 SKIP。

**4. 迁移**（工作目录 `go/db`）

```sh
go run ./cmd/migrate -f etc/db.yaml -command plan
go run ./cmd/migrate -f etc/db.yaml -command up
```

- 每个落点库加 `-storage-id <id>` 各跑一次。需要 `DB_ALLOWED_DATABASES` 放行目标库。
- 通过标准：plan 只有 4 条 `ADD COLUMN fence_*`；up 之后 `SHOW COLUMNS FROM player_database LIKE 'fence_%'` 与 `player_database_1` 各返回 2 行。

**5. merge_zone**（工作目录 `tools/merge_zone`）：`go test ./... -count=1`，原有用例保持绿。

**6. C++**

- 先 `pwsh -File tools/scripts/dev_tools.ps1 -Command cpp-node-stop`。
- 仓库根：`msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`。必须 `/m:1` 串行；生成的 proto 库变了，要全量编译。
- 运行 `cross_zone_test.exe --gtest_filter=*SaveSeq*:*DirtySave*:*OwnerEpoch*`。
- 新增的 `save_seq.h` 是头文件：`cpp/libs/services/scene/CMakeLists.txt` 只登记 `.cpp`，不用改；`scene.vcxproj`（ClInclude，:53-78）与 `scene.vcxproj.filters` 要登记。测试工程没有 CMakeLists。
- 通过标准：编译零错误，用例全绿。

**7. 本地端到端**

- 按 AGENTS.md §6.2 清环境后，`Fencing.Mode=dual` 起服，跑 robot 冒烟。
- 通过标准：`db_fence_write_total{class="seq"}` 的 updated / inserted 大于 0；`stale_owner`、`legacy_after_seq` 为 0；同一玩家两张表的 `fence_save_seq` 相同且非 0；`fence_owner_epoch` 等于该玩家的 owner_epoch。

**8. 一致性与混沌**

- `go/db/cmd/data_stress` 加 `-fenced` 开关（填 OwnerEpoch 与 SaveSeq），go/db 开两个副本，中途强杀一个并注入重试，之后跑 `cmd/verifier`。
- 再用一个 `dual` 副本与一个旧版副本混跑一轮。
- 通过标准：收敛到最大序号，0 处不一致。

**9. 压测**：按 AGENTS.md §6 全流程，与 R20 基线（交接 §4.1 第 4 项）对比。关注 `dataloader` 等 db 回调的耗时没有回归、`older_seq` 的比例。没有对比表不宣称性能结论。

---

## 12. 改动清单与规模

| 批 | 文件 | 说明 |
|---|---|---|
| A proto 与文档清单 | `proto/db/db_task.proto`、`proto/common/database/mysql_database_table.proto`、`proto/common/database/player_cache.proto`；§3.4 的 6 个文档 | 另有生成物约 10 个 |
| B go/db | `internal/kafka/key_ordered_consumer.go`、`placement_route.go`（哨兵与豁免）、`internal/locker/locker.go`、`internal/config/config.go`、`etc/db.yaml`、`internal/metrics/metrics.go`、`internal/dbtest/fakedriver.go`、`cmd/data_stress/main.go`、`cmd/migrate/main.go`（若采纳 fence-reset）、`go.mod` / `go.sum`；测试 4~5 个 | KOC 是多条修复线共用的文件，合入顺序见瓶颈文档 §5.2 |
| C C++ | `player_lifecycle.cpp`、`save_seq.h`（新）、`last_persisted_snapshot_comp.h`、`player_database_loader.cpp` 或加载处、`scene.vcxproj` 与 `.filters`、`cross_zone_test.cpp` | — |
| D 部署与运维 | `tools/scripts/k8s_deploy.ps1`（db ConfigMap）与契约测试、`deploy/k8s/owner-epoch-alerts.yaml`、回档与 Redis 丢失的两份 runbook | `k8s_deploy.ps1` 有另一会话在改 |

手写改动约 24 个文件，加生成物超过 30 个。建议按 A → B → C → D 拆成 4 个 PR，A 与 B 之间隔着 DDL。无论如何拆，本项都需要授权：字段号上线后不可回收，每个库要做 DDL，跨 C++。

---

## 13. Java 版（AGENTS.md §12）

**结论：不适用（Java 内部），另有一处待核。** 依据是对 Java 仓库 `D:\luyuan\xuanming-server-mmo`（`d942f74`）的只读核对：

- 带状态的写只由持有该 epoch 的 scene 进程自己执行，中间没有 go/db 这样的独立执行实例，也没有 Kafka 或 Redis 队列承载存盘。围栏就是 UPDATE 的 WHERE（`PlayerMapper.updateStateHeld`：`WHERE player_id = ? AND owner_epoch = ? AND owner_released = 0`），与数据写在同一个事务里。本设计要挡的「另一个执行实例拿着旧快照恢复」在 Java 版没有对应物。
- 交接里「同步写库」的说法要订正：Java 是进程内有界队列加重试的异步执行，快照到 SQL 之间有排队延迟；跨 epoch 靠围栏拒，同 epoch 靠「每个实例对象至多一笔在途的在线存盘」。
- **待核**：同节点收到同一 `(玩家, epoch)` 的再次进场时会换新实例并沿用旧内存（`SceneWorld.java:1403-1424`）。旧实例在途的在线存盘可能晚于新实例的存盘提交，等值围栏放行，库回到旧值。触发条件（gate / login 何时重发进场）没有核实。这是 Java 版自己的缺口，是否修由负责人定。
- 两版的差别：本仓库是单调高水位（异步通道必须接受老主在新 epoch 铸出之后才到的最后一笔），Java 是对权威列做等值比较，没有序号列。
- 新 proto 字段经 Java 仓库的 `tools/ContractSync.java` 进入 `game-proto`（它原样同步 `proto/**`，只排除 `proto/etcd/`）。Java 全仓没有引用 `DBTask` 或这两张表的 message，加字段兼容，不改 Java 代码。三个 proto 都不是客户端可见契约（`proto/README.md:27-31`）。
- PARITY.md 第 103 行记着 proto2mysql v0.2.0 与 `game-pbmysql` 逐方法对齐；proto2mysql 出 v0.3.0 后那一行要补状态。玩家存盘不经 `PbMysql`。

**登记**：功能落码后在 Java 仓库 `PARITY.md` 追加一行，状态「不适用（Java 内部）」，备注写上面的差别与待核项。设计阶段不登记。

---

## 14. 待拍板与未核实

**待拍板**：

| # | 事项 | 建议 |
|---|---|---|
| 1 | DBTask 9 号给 `save_seq` 还是 `reply_instance`（交接 §5 第 13 项）| 给 `save_seq`，另一个用 10。两个号都没用过，谁先落码谁在注释里预留另一个 |
| 2 | 是否改 AGENTS.md §6.2（交接 §5 第 14 项）| 条件写上线时加一句提醒，与另一会话对 §6.2 的修改合成一次 |
| 3 | Redis 分片的 hash tag（交接 §5 第 16 项）| 本批不受影响；裁决时把 §6.4 的两条约束算进去 |
| 4 | 缺省模式与切换方式（§8.2、§8.3）| 缺省 `off`，逐 zone 切 `dual` |
| 5 | 围栏归零的入口放哪（§9.1）| `cmd/migrate` 加子命令 |
| 6 | proto 重新生成的做法（§11 第 1 步）| 请负责人说明平时怎么跑 `proto-gen-run` 而不写客户端仓库；AGENTS.md §4.1 的描述是否订正 |
| 7 | epoch 铸造的持久下限由谁做（§9.1）| 与 D1 那条线确认；D1 提案里目前没有 |
| 8 | 是否让 scene_manager 在进程换代时必定铸造 epoch（§7.4）| 先核实那条时序是否可达，再定 |
| 9 | Stage 1b 的前置是否接受「L-04 第 1 批先上线」（§8.3）| 接受；两者都改 KOC，先 L-04 后本设计 |
| 10 | Java 版同 epoch 重复进场的待核项（§13）| 另开任务核实 |
| 11 | `STRESS_TEST_PROBE=1` 时快路径是否失效（§7.3）| 另开任务核实，影响压测口径 |

**未核实**：

- Drift 实际输出的 DDL 文本（只按源码推断）。
- go-sql-driver v1.9.3 的 `NewConfig()` 默认值；uint64 参数在服务端预处理路径上的绑定（§11 第 3 步用真库用例覆盖）。
- TiDB 上 autocommit 的乐观事务对 `SaveConditional` 的影响：9007 / 8002 / 1213 由调用方重试，建议玩家库所在集群用悲观事务。
- scene 主机的时钟同步方式与历史最大回拨量。
- `data_service` 回档写父键的路径是否只写 Redis。
- librdkafka 幂等生产者在本仓库版本下的保序行为（子模块未检出）。
- `SaveConditional` 本身尚未编译、未运行。

---

## 15. 考虑过而不采用的方案

| 方案 | 不采用的原因 |
|---|---|
| 把 save_seq 放进 body 的围栏字段，不加 DBTask 字段 | go/db 的守卫在解 body 之前就要用到序号；围栏列会多出一个写入方，body 与信封不一致时无从裁决 |
| 每玩家一个 `PlayerSaveSeqComp` | 实体重建后归零，只能靠父键播种，而父键不是已发出序号的上界 |
| 只用墙钟发号 | 时钟回拨即静默丢盘 |
| 用 Kafka offset 当序号 | 游标按分区记，换代后 offset 重置，读写本来就不在同一分区 |
| 多行合并写、`INSERT … ON DUPLICATE KEY UPDATE` 加 `IF` | 分不出哪一行被拒；两列围栏依赖 SET 的赋值顺序；proto2mysql 已提供等价且更清楚的原语 |
| 独立的 `:fence` 缓存键 | 两键必须同生同灭；分片后要求同槽 |
| 在 Lua 里解析 protobuf 取围栏 | 依赖字段编码顺序，脆弱 |
| 写后删缓存，由读路径重建 | 每次存盘后的第一次登录都变成冷读；读路径的回写同样会写入旧值 |
| `dual` 下对所有带 epoch 的写放开游标（§5.2）| class-2 没有可比的版本，会让旧写覆盖新写 |
| `stale_owner` 直接 ack 丢弃 | epoch 计数器倒退时会把全部存盘静默丢掉 |
| 复用 `db:capability:zone` 做 `sql` 模式门禁 | 每 zone 一把键，merge_zone 按精确相等判定 |

---

## 16. 交接要求对照

交接 §4.1「条件写设计必须覆盖」逐条对应的章节：

| 要求 | 章节 |
|---|---|
| DBTask 加 `save_seq`，字段号冲突 | §3.1、§14 第 1 条 |
| 两张玩家表加围栏列、声明在末尾、确认未被占用 | §3.2 |
| 帮会二期放行清单 | §3.4 |
| B1：stale_owner 进死信并告警；只清 Redis 的流程；AGENTS.md；长期方案 | §4.4、§9.1、§10、§14 第 2、7 条 |
| B2：save_seq 初值不能只靠墙钟 | §7 |
| B3：class-3 只在围栏为 `(0,0)` 时写 | §4.3 |
| 判 older 之后先复核再 ack | §4.4、§4.5 |
| `off` 模式不能调 Save | §4.3 |
| Stage 2 要有机器门禁 | §8.5 |
| PITR 回档后先清积压 | §9.2 |
| go/db 映射与 `SaveConditional` 语义；先判 epoch 再判 seq（G3）| §4.3、§4.4、§5.2 |
| 围栏缓存：不另开键、优先值封装、与分片方案的关系 | §6 |
| MySQL 读写超时 | §4.6 |
| 构建：proto 重生、C++ 串行、工程文件登记、不手改生成物 | §11、§12 |
| Java | §13 |

---

## 17. 修订历史

- v0.1（2026-10-13）：初稿。
- v0.2（2026-10-13）：对抗评审后的修订，见下。

评审记录待补（本稿提交评审后填写）。
