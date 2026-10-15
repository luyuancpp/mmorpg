# 玩家存盘 MySQL 条件写（围栏）：go/db、C++、proto、schema 详细设计

> **状态**：设计稿 v0.3（2026-10-14，Claude）。已并入对抗评审结论（§17：24 条意见逐条核实后的处置表）；**未落码、未编译、未测试**（AGENTS.md §10.1）。v0.2 文首写的「已过一轮对抗评审」不准确，当时评审结论尚未并入。
> **实现归属**：由「玩家消息投递与位置管理」会话落码，用户已于 2026-10-14 授权。落码前置：① L-04 第 1 批（[db-task-store-unavailable-deferral.md](db-task-store-unavailable-deferral.md) v0.3）合入，合入顺序 L-04 在前；② proto2mysql 的 `SaveConditional` 经 Codex 验证并合入 main。
> **范围**：C++ scene → Kafka `db_task_zone_*` → go/db → MySQL 这条玩家存盘链。给两张玩家表加围栏列 `(fence_owner_epoch, fence_save_seq)`，go/db 改为条件写。
> **上游材料**：[db-task-pipeline-bottlenecks.md](db-task-pipeline-bottlenecks.md)（下称「瓶颈文档」）§4、§6.4、§6.6；[交接](../handoff/godb-pipeline-handoff-20261013.md) §4.1 第 3 项；[附录](../handoff/godb-pipeline-handoff-20261013-appendix.md) B「mysql-fencing」（其中已作废的条目见交接 §4.0，本文不采用）；[L-04 设计](db-task-store-unavailable-deferral.md) v0.3（下称「L-04」）。
> **库原语**：proto2mysql 分支 `feat/conditional-save`（commit `e81a1e7`）的 `SaveConditional`。它未合入 main、未经 Codex 验证。负责人已定：全绿后只合入 main、不打 tag；go/db 用伪版本钉 main 上的那个提交（`docs/PROGRESS.md` 2026-10-14 go/db 条目，§5 拍板第 1 条）。
> **证据口径**：[C] 读代码确认；[D] 文档记载；[I] 推断。v0.2 的引用在 HEAD `0aa1027bc` 上取证；v0.3 新增或改写的引用以 2026-10-14 的工作区（含未提交的 L-04 第 1 批）为准。L-04 让 KOC 整体后移约 4～51 行，PL 被另一会话插入约 286 行，所以正文以函数名为主；仍保留的带行号引用一律指 `0aa1027bc`。
> **缩写**：KOC = `go/db/internal/kafka/key_ordered_consumer.go`；PR = `go/db/internal/kafka/placement_route.go`；PL = `cpp/libs/services/scene/player/system/player_lifecycle.cpp`；P2M = proto2mysql 的 `conditional_save.go`。

---

## 0. 结论速览

1. **围栏是行内两列，按字典序比较。** go/db 把 DBTask 的 `(owner_epoch, save_seq)` 盖到要落库的消息上，用 `SaveConditional` 写：库内围栏更旧才写，判断与写入是同一条 UPDATE。旧的执行实例恢复后发出的写会被 MySQL 自己拒掉，不再依赖 Redis 排序锁的租约。
2. **首批只实现 `legacy` 与 `dual` 两种模式。** `dual` = 条件写 + 现有全套 Redis 守卫；`legacy`（v0.2 叫 `off`，改名原因见 §4.7）是紧急开关，写入语义等同今天。删 Redis 守卫的 `sql` 模式（附录的 Stage 2）属于性能轨，本文只定契约和门禁（§8.5），不在首批实现。条件写只作用于「围栏表」（§4.1），其余表照旧 `Save`。
3. **被拒的写按拒绝位置分两路，没有任何一种是「不复核就丢」：**
   - 到达 SQL 的：库里已有同版本或更新版本（`equal` / `older_seq`）→ 复核后 ack；归属更老（`stale_owner`）、计数器倒退（`owner_epoch_regressed`）或无法判定（`legacy_after_seq`）→ 进死信并告警，保住载荷。
   - 在 Redis epoch 守卫被判 stale 的：`dual` 下先探测 owner_epoch 计数器。没有倒退 → ack 丢弃（同今天）；发现倒退 → 进死信（§5.1）。
   - 残余：计数器重新追过已落库 epoch 之后，倒退期间发出的写与僵尸写无法区分（§9.1）。
4. **游标判过期时让 epoch 说话（G3）只在 `dual` + `Fencing.EpochOverride` + class-1 + 围栏表上放开。** 放行的安全性来自新二进制把「游标、已落库 epoch」用一条脚本原子推进（§5.5），override 之后游标原子改写到本任务（§5.2）。旧二进制做不到原子推进，所以这个开关有门禁（§8.3）。
5. **save_seq 由 C++ 进程级发号器发出**，播种来自 Redis 父键和 MySQL 行两路，墙钟只作下限；`Observe` 有上界，`Next` 饱和时显式失败。不设每玩家组件（§7）。
6. **子表缓存不加键、不改值格式**：写路径改成「持有排序锁令牌才写」的单条脚本，读路径改成 `SET NX`（§6）。缓存只前进不后退，但可能落后于 MySQL；缓存里的围栏只是行内围栏的下限（§6.5）。
7. **规模**：生成物 22 个，手写约 24 个，合计约 46 个文件；A 批自身 3 + 6 + 22 = 31 个，已超过 AGENTS.md §10.2 的 30 文件门槛。跨 C++ / Go / proto / schema，每个库要做一次 DDL。用户已于 2026-10-14 授权实现；A 批超门槛这一点在落码交付说明里仍要单独报告（§12）。

### 0.1 相对交接与附录的变化

| # | 交接 / 附录的说法 | 本文的做法 | 原因 |
|---|---|---|---|
| 1 | C++ 加 `PlayerSaveSeqComp`，实体内计数 | 进程级发号器，实体不存序号 | 实体在同一 epoch 内会销毁重建（PL:3623-3628）；Redis 父键不是「已发出 DBTask」的上界（§7.1） |
| 2 | 播种取 `max(PlayerAllData.last_save_seq, 同 epoch 的 fence_save_seq)` | 两路都取，不看 epoch；`Observe` 加上界 | 父键存在时 login 不读子表，C++ 拿不到 MySQL 行里的围栏（§7.2）。不看 epoch 有代价：进程级下限会被任意一名玩家带来的值抬高，并经父键和行内围栏在进程之间传播，所以要加上界（§7.2） |
| 3 | 缓存回写带版本，建议放进值封装 | 用排序锁令牌做原子校验（新二进制，`legacy` / `dual` 都生效）；值封装留给 `sql` 模式 | `dual` 还持有排序锁，令牌校验已经够用，且不动 login（§6） |
| 4 | 三种模式一起实现 | 首批只做 `legacy` / `dual` | `sql` 是性能优化，立项判据未满足（瓶颈文档 §6.3） |
| 5 | G3「先判 epoch 再判 seq」 | **部分修复**：只在 `dual` + `EpochOverride` + class-1 + 围栏表上放开；override 后原子改写游标；新二进制原子推进游标与已落库 epoch | 不带 save_seq 的写在同一 epoch 内没有可比的版本；v0.2 的放行在 override 后不改游标、且依赖游标与 epoch 的非原子推进，两头都不成立（§5.2、§5.5，§17 状态机 F1 / F2） |
| 6 | B1：`redis-cli FLUSHALL` 会让全部存盘被判 stale_owner | 触发面比预想小；处置改为「定真源、抬计数器、不归零、不回灌」 | FLUSHALL 同时清掉账号目录，旧 player_id 不会再被用到；归零加回灌会用旧数据覆盖新数据（§9.1） |
| 7 | 能力心跳 `db:capability:zone` 可复用为机器门禁 | 不能复用，`sql` 模式要另建按实例的登记 | 该键每 zone 一把、值是常量，merge_zone 按精确相等判定（§8.5） |
| 8 | 缺省模式 `dual` | 缺省 `legacy`，逐 zone 改配置切 `dual` | 二进制发布与行为切换分开，可以单 zone 金丝雀（§8.3） |

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
| F-1 | 同一玩家、同一张围栏表、同一个库里，行内围栏只进不退；带 save_seq 的写里，行内容永远是已到达的写中围栏最大的那一笔 | 条件与写入在同一条 UPDATE 里，在主键行锁下完成；§5.2 放行的写落库后原子改写游标（§5.2、§5.5）。v0.2 在 override 后不改游标，同 epoch 的较新重试会在游标处被丢，本条当时不成立（§17 状态机 F1） |
| F-2 | 被拒的写不留下任何副作用：不写缓存，不推进游标和已落库 epoch | 结局处理表（§4.4） |
| F-3 | 不静默丢盘。到达 SQL 的写：只有库里已有不低于本版本的数据、且复核确认该库仍是真源时才 ack，其余进死信并告警。在 Redis epoch 守卫被判 stale 的写：`dual` 下先探测计数器，没有发现倒退才 ack，发现倒退进死信 | §4.4、§5.1、§10。残余：计数器重新追过已落库 epoch 之后，倒退期间发出的写与僵尸写无法区分（§9.1） |
| F-4 | 子表缓存只前进、不后退，不会被比它旧的版本覆盖；SQL 已落而缓存未发布的窗口靠重试补发收敛 | §6.2、§6.5。缓存不保证等于 MySQL（失租、发布失败、跨进程撞号时见 §4.4、§6.5） |
| F-5 | 新旧 go/db、新旧 C++ 并存期间，安全性不低于今天 | 成立条件：§5.5 的原子推进，加 `EpochOverride` 门禁（该 zone 没有旧版 go/db，且旧实例下线后重试排空过一次，§8.3）。新二进制的 `legacy` 与 `dual` 混跑在 §5.5 之下是安全的（§8.4） |

### 1.3 不在范围内

- 删 Redis 守卫、合并 Redis 往返（`sql` 模式与瓶颈文档 §6.3 的 3a / 3b）。
- 读写分区对齐（①）、重试只交给分区属主（④）、消费组按 zone 拆（③）。
- 连接不可用时延后（L-04）与死信工具：见 [L-04](db-task-store-unavailable-deferral.md)。第 1 批已落码（工作区，未提交、未验证），本文在它的接口上实现（§4.1、§4.4）；第 2 批死信工具是 Stage 1b 的前置条件（§8.3），目前未授权。
- 两张子表之间的原子性：仍然是两条独立的 DBTask、两次独立的写。
- 「scene 发 DBTask 失败后脏比较快照不作废」（`SavePlayerToRedisImpl` 的发送失败分支）：负责人已定归存盘管线交接（交接 §5 第 17 项），不在本设计。
- **冷登录与在途写的竞态**（§6.2 残余窗口 3）：父键缺失、而该玩家仍有未落库的写时，login 拼出的父键比在途写旧。NX 和条件写都挡不住，今天同样存在。修法方向（按 key 维护「有待落写」标记）记在 §14，不在本批。

---

## 2. 术语

**围栏**：行内的 `(fence_owner_epoch, fence_save_seq)`，按字典序比较，epoch 是高位。

**任务类别**（按 DBTask 判定，只对 `op == "write"`）：

| 类别 | 条件 | 来源 | 指标 label |
|---|---|---|---|
| class-1 | `owner_epoch > 0` 且 `save_seq > 0` | 新版 C++ | `seq` |
| class-2 | `owner_epoch > 0` 且 `save_seq == 0` | 没有 save_seq 的旧版 C++（滚动升级窗口） | `legacy_epoch` |
| class-3 | `owner_epoch == 0`（不看 save_seq） | 实体没有拿到 epoch 的兼容路径（`SavePlayerToRedisImpl` 里 epoch 为 0 时走无守卫 Save 并计 `owner_epoch_unknown`）；`go/db/cmd/data_stress`（不填 OwnerEpoch）| `legacy_zero` |

当前 write 的生产者只有 C++ scene（`SavePlayerToRedisImpl`，只写 `player_database` / `player_database_1`）和 data_stress（只写 `PlayerDatabase`）[C]。

class-3 在现行代码里很难走到：scene_manager 的路由一律带观察到的或新铸的 epoch，键丢失时同节点按 location 补种、location 也没了就铸造（≥1），gate 原样透传 [C]。对已经落过 class-1 的玩家，只有 CZ-4 之前的旧 gate / scene_manager 还在跑才会产生 epoch 0；另一类是还没铸过 epoch 的存量玩家同落点重连，它写的是 `(0,0)` 行，切 `dual` 后无害。data_stress 的号段从 1,000,000 起，只影响测试环境。

**围栏表**：描述符里有两列围栏字段的表（判定规则见 §4.1）。今天只有 `player_database`、`player_database_1`。§4.2–§4.4、§5.2、§5.3 只作用于围栏表。

**模式**（配置 `Fencing.Mode`）：

| 模式 | 含义 |
|---|---|
| `legacy` | 紧急开关（v0.2 叫 `off`，§4.7）。写入语义等同今天：Redis 守卫照旧，SQL 无条件覆盖业务列，不碰已有行的围栏列 |
| `dual` | 条件写 + 现有全套 Redis 守卫 |
| `sql` | 预留。首批二进制读到这个值时拒绝启动 |

子开关 `Fencing.EpochOverride`（缺省 `false`，只在 `dual` 下生效）控制 §5.2 的放行，开启条件见 §8.3。模式名与类别的指标 label（`legacy_epoch`、`legacy_zero`）互不相关。

---

## 3. 数据契约

### 3.1 DBTask（`proto/db/db_task.proto`）

- 新增 `uint64 save_seq = 9;`。**负责人已定**：9 号给 `save_seq`，10 号给全局单点交接的 `reply_instance`（交接 §5 第 13 项）。9、10 号都从未用过 [C]，不存在复用问题；先落码的一方在注释里写明另一个号已预留。
- 语义：生产者按玩家发出的存盘序号，同一玩家只增不减；0 表示生产者没有发号。
- 顺带订正 8 号字段的注释（:18-19）：现在比对的是「已落库的最大 epoch」（`consumer:applied_epoch:*`，KOC 的 `appliedEpochKey` / `checkOwnerEpoch`），不是 `player:{key}:owner_epoch`。

### 3.2 玩家表（`proto/common/database/mysql_database_table.proto`）

| message | 新字段 | 现末尾 |
|---|---|---|
| `player_database` | `uint64 fence_owner_epoch = 18;`、`uint64 fence_save_seq = 19;` | `settlement_ledger = 17`（:154）|
| `player_database_1` | `uint64 fence_owner_epoch = 3;`、`uint64 fence_save_seq = 4;` | `stress_test_probe = 2`（:170）|

- 都声明在 message 末尾（加列纪律，:139-145）。18 / 19 目前没有被占用 [C]，落码前再 grep 一次。
- 注释写明：存储版本元数据，只由 go/db 按 DBTask 写入；C++ 只在加载时读来播种（§7.2），存盘时不写。
- **为什么必须是 proto 字段**：`SaveConditional` 的 `FenceFields` 只认该表 message 的顶层 uint32 / uint64 字段，栅栏值从消息字段读，没有「外部传入栅栏值」的入口（P2M:200-208、:264）[C]。另外三个好处是附带的：`cmd/migrate` 的 Drift 自动生成 ADD COLUMN；启动闸 `assertSchemaUpToDate` 缺列拒启；读路径会把围栏一起读回。
- **「先 DDL 后代码」由机器保证只限本 zone 库**：启动闸只核对配置里的 `DBName`。其它落点库在第一次被指向时才按需打开，打开时过同一道只读 schema 闸，失败包成 `ErrStoreUnavailable`（`store_registry.go`）[C]。所以漏迁的落点库不会拒启，表现为写不耗预算地延后（`db_task_deferred_total{reason="store_open_failed"}`，被 L-04 的 critical 告警 `DbTaskDeferredByStore` 覆盖）和冷读立即失败。补完 DDL 后按需打开在 10 秒退避后自动成功，不用重启；期间写只在 ready 里有一份，压在 noeviction 的 Redis 上。兜底靠 §8.1 的库清单穷举与 §8.2 的观察项。
- `player_centre_database` 也以 player_id 为主键，但没有任何 write 生产者 [C]，不加围栏。
- **纪律**：任何新表成为 DBTask write 目标时，必须在同一次改动里加这两列围栏，并进入 §4.1 的启动断言列表。否则它在 `legacy` / `dual` 下都按非围栏表走 `Save`（§4.1），条件写对它不生效。

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
- 每个 zone 库跑一次；每个落点库加 `-storage-id <id>` 再各跑一次（`-storage-id` 接受 zone 编号）。库清单按 §8.1 第 2 步穷举，不能只靠记忆或 `tools/merge_zone/storage_audit.go`（它只审计给定 `-storage` 的一个库）。
- 存量行的默认值 `(0,0)` 表示「还没见过带版本的写」，不做回填。

---

## 4. go/db：条件写

### 4.1 入口

- **签名**：L-04 第 1 批已把 `dbOpHandler` 改成 `func(ctx context.Context, store *proto2mysql.DB, task *db_proto.DBTask, msg proto.Message) error`。本设计在这个 error 签名上把写 handler 扩成 `(writeOutcome, error)`，同步改测试替身 `recordingHarness`（`key_ordered_consumer_test.go`，按现签名整张替换 `dbOpHandlers`）。
- **被拒的结局不编码成 error**：`runDBOp` 在 handler 返回后先调 `route.reportStoreResult(handlerErr)`，收到非 nil 时既不会 `ReportHealthy`，连接类错误还会触发探活（`store_failure.go`）[C]。所以 SQL 已正常应答的拒绝（`equal` / `older_seq` / `stale_owner` / `owner_epoch_regressed` / `legacy_after_seq`）一律以 `(outcome, nil)` 返回，`reportStoreResult` 只拿 SQL 这一步自身的错误上报。
- **错误标记**：`handleDBWriteOp` 对 `SaveConditional` 返回的错误继续套 `storeerr.Tag`。P2M 用 `%w` 保留了驱动错误，所以 1205 / 1213 / 9007 / 8002 与连接类错误照常按 L-04 分类；条件写自己的哨兵（`ErrConditionalSaveRace` 等）不是驱动错误，归 `KindNone`，按 §4.4 错误表处理。`storeerr` 标记只用于 SQL 调用点（L-04 §3.2），Redis 侧的新错误不得复用它。
- `handleDBWriteOp` 不再丢弃 ctx：用 `store.WithContext(ctx)`（proto2mysql v0.2.0 就有）把截止时间带到每条语句（§4.6）。
- 顺序：大字段闸（不变）→ 围栏表判定（见下）→ 按类别给消息盖围栏（§4.2）→ `SaveConditional`（§4.3）→ 把库的结局归类成 `writeOutcome`（§4.4）。
- **「是否走了 §5.2 的 override」**在 SQL 之前由 `orderingForTask` / `guardOwnerEpoch` 决定，记在 `workerTask` 上（新增字段），由 `handleTask` 尾部的推进脚本（§5.5）读取。不塞进 handler 返回值。
- `route.store` 的类型仍是 `*proto2mysql.DB`，不需要改成 `*GameDB`：状态查询由库自己做，go/db 不再手写 SELECT。

**围栏表判定**（只算一次；§4.2、§4.3、§4.4、§5.2、§5.3 都引用这一个结果，不各自判断）：

- 启动时遍历 go/db 已注册的表（`go/db/data/mysql_database_table_list.json`，9 张），描述符同时含顶层、非 optional 的 uint64 字段 `fence_owner_epoch` 与 `fence_save_seq` 的才算围栏表，结果按 MsgType 缓存。
- 启动时断言 `player_database`、`player_database_1` 在这个集合里，不在就拒启，避免已知表静默退化成无围栏。
- 非围栏表在 `legacy` 和 `dual` 下一律沿用 `store.Save`：不盖章、不分类、不放开游标（§5.2），计 `db_fence_write_total{class="unfenced"}`，每个 MsgType 首次出现时打一行 ERROR。
- 理由：写任务不按表区分，`processTaskWithoutLock` 按 `task.MsgType` 在全局注册表里找描述符，9 张表都能走到 `handleDBWriteOp`，其中 7 张没有围栏列。P2M 在拼计划阶段对不在表里的保留列 / 栅栏列返回 `ErrInvalidSaveOptions`（P2M:214-218、:263-267），不加判定时 `legacy` 的 `PreserveFields` 与 `dual` 的 `FenceFields` 在这 7 张表上必然失败，按 `deferReason=other` 耗完预算后以 `budget_exhausted` 进死信 [C]。目前没有第三张表的写生产者（§2），所以这是潜在问题。
- 盖章只对围栏表做，所以 dynamicpb 按字段名取描述符不会拿到 nil，不存在对 nil 描述符 `Set` 而 panic 的路径。

### 4.2 盖围栏

go/db 在调用前把围栏写进 dynamicpb 消息的两个字段（只对围栏表）：

| 模式 / 类别 | `fence_owner_epoch` | `fence_save_seq` |
|---|---|---|
| `dual` class-1 | `task.OwnerEpoch` | `task.SaveSeq` |
| `dual` class-2 | `task.OwnerEpoch` | 0 |
| `dual` class-3 | 0 | 0 |
| `legacy` | 0 | 0 |

C++ 每次都从组件全新构造消息（PL:3587-3595），body 里这两个字段恒为 0 [C]。如果 body 里出现非 0 且与 DBTask 不一致的值，说明有别的生产者或生产端有缺陷：打 ERROR、计数，以 DBTask 为准。

### 4.3 模式 × 类别 → `SaveOptions`

记 `fo = "fence_owner_epoch"`，`fs = "fence_save_seq"`。下表只对围栏表；非围栏表照旧 `Save`（§4.1）。

| 模式 | 类别 | 调用 | 写入条件 |
|---|---|---|---|
| `legacy` | 任意 | `SaveOptions{PreserveFields: [fo, fs]}` | 无条件。已有行的围栏列不动；新行的围栏为 0 |
| `dual` | class-1 | `SaveOptions{FenceFields: [fo, fs]}` | 库内 `(fo, fs)` < `(E, s)` |
| `dual` | class-2 | 第一步 `{FenceFields: [fo, fs], AllowEqual: true}`；被 `(E, s>0)` 拒绝时第二步 `{FenceFields: [fo], AllowEqual: true, PreserveFields: [fs]}` | 第一步：库内 ≤ `(E, 0)`。第二步：库内 `fo` ≤ `E`，不动 `fs` |
| `dual` | class-3 | `{FenceFields: [fo, fs], AllowEqual: true}` | 库内围栏等于 `(0, 0)` |

说明：

- **`legacy` 不能调 `Save`（对围栏表）。** 新二进制的消息里带着值为 0 的围栏字段，`Save` 会把已有行的围栏清零。`PreserveFields` 的语义是「更新已有行时不写这些列，插入新行时照常写」（P2M:62-65）[C]。只带 `PreserveFields` 时没有任何保护，这正是「等同今天」。
- **class-1 用严格模式。** 同版本重放的结局是 `SaveRejected` 且 `Stored` 等于消息围栏，调用方据此判为重复（P2M:57-59）。
- **class-2 的第二步**解决附录复核指出的问题：`<= E` 的条件写 `(E, 0)` 会把同一 epoch 内已有的 `(E, s>0)` 退回 `(E, 0)`。第二步只比 epoch，并保留库内的 seq，围栏不退。class-2 在同一 epoch 内的先后仍然靠 Redis 的排序锁与游标（§5.1）。
- **class-2 第二步的缓存围栏（可选）**：第二步之后缓存里的消息围栏是 `(E, 0)`，行内 `fs` 被保留，缓存围栏低于行内（§6.3）。可选的修补：第一步 `SaveRejected` 时 `Stored` 必定存在且含 `[fo, fs]`（P2M:100-104），第二步把第一步读到的 `fs` 回填进消息再发布缓存。两步之间同一 `fo` 下行内 `fs` 只增不减，所以回填值是下限，安全。收益很小（class-2 只出现在 C++ 滚动或回退窗口，还要叠加冷加载和时钟回拨才有用），本批默认不做，只改文档；做的话在 §11 第 2 步加用例（§14 第 18 条）。
- **class-3 只在行上没有围栏时写**（修 B3）。行上已有围栏说明带版本的写已经落过，这时一笔没有任何版本的写无法判断新旧，不能覆盖。严格模式下全 0 围栏会被库判为非法选项（P2M:209-212），所以 class-3 必须带 `AllowEqual`。
- 不可能出现的组合（例如严格模式被拒但 `Stored` 比消息旧）返回错误、不 ack。

### 4.4 结局处理

| `writeOutcome` | 来自 | 落点复核 `route.confirm()` | 缓存回写 | 游标 / 已落库 epoch | 终态 |
|---|---|---|---|---|---|
| `applied` | `SaveUpdated` / `SaveInserted` / `SaveUnchanged` | 做 | 写 | §5.5 的脚本推进。正常路径：游标只进不退；override 路径（§5.2）：游标改写为本任务的 (partition, seq)，允许后退。两条路径都对已落库 epoch 取 max | ack |
| `equal` | class-1 被拒，`Stored == (E, s)` | 做 | 写（补发上次可能没发成的缓存；跨进程撞号时见下）| 同 `applied`（失租后重试得到 `equal` 也走同一脚本，§5.4）| ack |
| `older_seq` | class-1 被拒，`Stored.fo == E` 且 `Stored.fs > s` | **做** | 不写 | 不推进 | 复核通过 → ack 丢弃并计数；复核不过 → 按 `errPlacementMoved` 延后（不耗预算） |
| `stale_owner` | class-1 / class-2 被拒，`Stored.fo > E`，且计数器探测没有发现倒退（`dual`，见下）| 不做 | 不写 | 不推进 | **进死信**（label `fence_stale_owner`），计数，打 `STALE-OWNER-WRITE` |
| `owner_epoch_regressed` | 同上，但 `player:{id}:owner_epoch` 计数器 < `Stored.fo`（含键缺失）| 不做 | 不写 | 不推进 | **进死信**（label `owner_epoch_regressed`），计数，打 `OWNER-EPOCH-REGRESSED` |
| `legacy_after_seq` | class-3 被拒（行上围栏不是 `(0,0)`）| 不做 | 不写 | 不推进 | **进死信**（label `fence_legacy_after_seq`），计数 |

错误的去向（按 L-04 的词表落定三件事：`deferReason` 的取值、是否进 `retryBudgetExempt`、死信 label）：

| 错误 | 含义 | 去向 |
|---|---|---|
| `ErrConditionalSaveRace` | 严格模式下 UPDATE 影响 0 行、读到的状态又满足条件，两轮都这样（P2M:504-507、:521-522）| 延后，**耗预算**（不进 `retryBudgetExempt`），`deferReason=fence_race`（新增，启动时以 0 建出序列）；用完以 `budget_exhausted` 进死信。理由：`dual` 持有排序锁时，严格模式下的 Race 只可能来自围栏倒退（人工改库、PITR、`debug_import` 并发执行）或 SQL 谓词与 Go 侧 `conditionHolds` 判定不一致，不是瞬时现象；不设上限会无界空转。TiDB 乐观事务的写冲突以 9007 / 8002 / 1213 返回，已由 L-04 归入 `store_busy`，不表现为 Race |
| `ErrSaveMatchMismatch` | 整行比较与 MySQL 的「值是否变化」不一致，多半是表结构问题 | 延后，耗预算（`deferReason=other`），最终进死信 |
| `ErrInvalidSaveOptions`、`ErrSchemaDrift`、`ErrMultipleRowsFound` | 配置或表结构错误 | 同上 |
| 带 `storeerr` 标记的驱动错误（连接数满、连接断开、只读、1205 / 1213 / 9007 / 8002）| 存储侧环境问题 | 按 L-04：写任务延后、不耗预算 |
| 语句已发出后的超时、连接断开 | 结局未知，那条语句可能已经生效 | 按 L-04 的分类延后；重试时 class-1 会得到 `equal` 或 `applied`，天然幂等 |
| 计数器探测读失败（§5.1 与本表 `stale_owner` 行的探测）| Redis 侧 | `deferFailedTask`，耗预算，`deferReason=redis_unavailable`（L-04 §4.3 口径）；用完以 `budget_exhausted` 进死信，载荷不丢。有意 fail-closed，不进 `retryBudgetExempt` |
| §5.5 推进脚本返回 0 / 出错 | 已失租 / Redis 侧 | 返回 0 包 `errOrderingLeaseLost`（`lease_lost`），Redis 错误归 `redis_unavailable`；都耗预算（§5.4） |

死信入口统一用 L-04 第 1 批带 label 的 `quarantineOrderingConflict(task, dbTask, label, reason)`：Kafka 来源先 LPUSH 到死信再 ack；重试来源把收据原子搬进死信。`go/db/internal/metrics/metrics.go` 新增三个常量 `DeadLetterFenceStaleOwner = "fence_stale_owner"`、`DeadLetterFenceLegacyAfterSeq = "fence_legacy_after_seq"`、`DeadLetterOwnerEpochRegressed = "owner_epoch_regressed"`，追加进 `deadLetterReasons` 让启动时预建序列，同步改 `db_task_dead_letter_total` 的 Help（那里的注释要求 Help 与常量一致）。这样 L-04 的 `DbTaskDeadLetterAdded` 自动覆盖它们（与 §10 告警的重复见 §10）。

要点：

- **`older_seq` 先复核再 ack**（附录复核的修正）。判 `older_seq` 只证明「被选中的那个库」里有更新的版本，不证明那个库仍是真源。复核不过时按 `errPlacementMoved` 延后，重投到新库。
- **`stale_owner` 不 ack 丢弃**（修 B1）。正常情况下真正的僵尸写在前面的 Redis epoch 守卫就被处理了（§5.1）；能走到 SQL 才被判 stale，说明 Redis 里的已落库 epoch 记录缺失或落后。`dual` 下这里再探测一次计数器（`GET player:{id}:owner_epoch`，判据同 §5.1）：计数器 < `Stored.fo` 归为 `owner_epoch_regressed`，否则记 `stale_owner`，两者都进死信保住载荷。handler 不碰 Redis：它只返回 `stale_owner` 与 `Stored`，探测与重新归类在 `handleTask` 拿到结局之后做；探测读失败按上表延后。计数器倒退而已落库 epoch 没有倒退时，写根本到不了 SQL，由 §5.1 的探测接住（§9.1）。
- **`equal` 视同「同一份内容」，但这个前提只在单个 scene 进程内由 §7 保证。** 跨进程、同一 epoch 时可能撞号：dev 旁路 `AllowUnsafeCrossNodeHandoff=true` 下新旧两个进程同持一个 epoch（§7.1），以及 §7.4 的 node_id 复用；时钟模式下每个进程某一毫秒的首次发号都是 `nowMs << 10`，计数模式（`last + 1`）下新旧进程从同一个下限起步，撞号更容易。撞号时后到的一笔判 `equal`，把自己的 body 补发进缓存，缓存与 MySQL 内容不同。v0.3 维持补发：失租恢复依赖 `equal` 补发缓存（§5.4、§6.2），不补发时缓存里会留着上一版的值，父键缺失时 login 直接拿它拼数据。这是 dev 旁路与 §7.4 残余风险下的已知代价，由 `cmd/verifier` 事后核对。另一方案（在令牌脚本里删除子表缓存键、让读路径从 MySQL 重建，撞号与否都正确，代价是每次 `equal` 多一次冷读）记为 §14 第 12 条待拍板。不采用「`equal` 不发布缓存」（§15）。

### 4.5 `runDBOp` 的顺序

「选库 → SQL → 上报 SQL 结果（L-04 的 `reportStoreResult`）→ 复核 → 缓存 / 读结果发布」的顺序不变。变化只有两处：

- handler 返回 `stale_owner` / `owner_epoch_regressed` / `legacy_after_seq` 时，`runDBOp` 直接把结局交回 `handleTask`，不复核、不发布。
- handler 返回 `older_seq` 时，复核照做，但不发布。

被拒的结局都以 `(outcome, nil)` 返回（§4.1），`reportStoreResult` 收到 nil，按「SQL 正常应答」处理。

复核 MGET 不能因为有了围栏而省掉：围栏是按库内的行算的，看不出「这个库已经不是真源」（`player-storage-placement.md` §6.3）。

### 4.6 超时与取消

- **连接级超时已由 L-04 落地，直接复用。** 业务连接池 `newServiceMysqlConfig`（`proto_sql/db.go`）在 `newMysqlConfig` 之上设了五项：拨号 5 秒、读 30 秒、写 30 秒，以及会话级 `lock_wait_timeout`、`innodb_lock_wait_timeout` 10 秒 [C]。本设计不新增超时配置键。迁移入口 `NewMysqlConfig` 不带这些，不受影响。
- **ctx 仍是本设计的工作。** `handleDBWriteOp` 仍丢弃 ctx（签名里是 `_ context.Context`）[C]。改为经 `WithContext` 把处理上下文带到每条语句：Stop 时取消；失租时取消属瓶颈文档 §4.4 G1 治标第 ② 条，该条未落地时本批一起引入。
- 语句被取消或超时后，驱动会关掉那条连接，服务端可能仍把语句执行完。这类错误一律按「结局未知」处理：延后重试，靠围栏幂等。**治标措施撤不回已经进入 MySQL 的语句，挡住旧写的是围栏。**

### 4.7 配置

```yaml
Fencing:
  Mode: legacy          # legacy | dual；缺省 legacy
  EpochOverride: false  # 只在 dual 下生效；开启条件见 §8.3
```

- **模式名不用 `off`**：go/db 依赖的 go-zero v1.10.0（`go/db/go.mod`）解析 YAML 用 `gopkg.in/yaml.v2`（YAML 1.1），裸写的 `off` 会被解析成布尔 `false`；`core/mapping` 对 string 字段收到 bool 返回类型不匹配，go/db 的三个入口（`db.go`、`cmd/migrate`、`cmd/verifier`）都用 `conf.MustLoad`，直接退出 [C，读本机 module cache 中 go-zero v1.10.0 的源码确认，未运行]。带引号的 `"off"` 能用，但只能靠纪律保证，而且如果模板在 Stage 1a 省略这个键，问题会拖到紧急回退时才暴露（滚动卡住或 CrashLoop）。`legacy`、`dual`、`sql` 都不撞 YAML 1.1 的布尔字面量。
- go/db 的配置没有独立的校验函数，默认值统一在 `Normalize()` 里补（`internal/config/config.go`）[C]。`Mode` 用 go-zero 的 `options=|legacy|dual` 约束（同 `AllowlistEnforcement` 的 `options=|strict|warn` 写法，允许显式空值），空值在 `Normalize()` 里补成 `legacy`。写了 `sql` 或其它值即拒绝启动。
- `EpochOverride` 是 bool，缺省 `false`；`Mode=legacy` 时忽略（打一行 WARN）。启动日志打印 `Mode` 与 `EpochOverride` 的生效值，切换后用它核对。
- K8s 上 db 的 ConfigMap 模板（`tools/scripts/k8s_deploy.ps1` 的 `New-GoSvcConfigMapYaml` 里 `"db"` 块）不会自动带上 `db.yaml` 的新键 [C]，必须手改模板，并在 `tools/scripts/tests/k8s_deploy_contract.tests.ps1` 加断言：模板输出的 `Mode` 取值、`EpochOverride` 取值都在约定集合内。该文件目前有另一个会话在改，落码前先对齐。
- 配置用例见 §11 第 2 步。

---

## 5. go/db：与 Redis 守卫的关系（以 `dual` 为主；§5.5 与模式无关）

### 5.1 保留什么

排序锁、游标（`consumer:applied:*`）、已落库 epoch（`consumer:applied_epoch:*`）全部保留，键名和值格式不变。原因：

- 旧版 go/db 只认这三样，新旧实例并存时靠它们互斥和判新旧。
- class-2 / class-3 在同一 epoch 内没有可比的版本，先后只能靠锁和游标。
- 对 class-1，它们是便宜的去重：重复投递在游标处就被丢掉，不用把整行发到 MySQL。

**Redis epoch 守卫判 stale 时：`legacy` 不变（ack 丢弃）；`dual` 下先做计数器探测。**

现状：`guardOwnerEpoch` 的 stale 分支 ack 重试收据和 Kafka 消息，不进死信，排在选库与 SQL 之前 [C]。计数器倒退（`player:{id}:owner_epoch` 回到旧值）而已落库 epoch 没有倒退时，倒退之后铸出的 epoch 全部小于已落库 epoch，这名玩家之后的写都在这里被丢掉，到不了 SQL，死信里也没有载荷。丢弃不是无声的（`STALE-OWNER-WRITE` 日志、`db_stale_owner_write_rejected_total`、warning 级告警 `DbStaleOwnerWriteRejected`），但告警描述把它解释为「可能是 K1 被正确拦下」，值班大概率当成良性，而载荷确实丢了（§17 状态机 F3、事实 F1）。

`dual` 下的探测：

- 判 stale 时多读一次 `GET player:{id}:owner_epoch`（键名与解析用 `go/shared/ownerepoch` 的 `OwnerEpochKey` / `ParseEpoch`）。
- **计数器 ≥ 已落库 epoch**：判为真旧主，ack 丢弃（同今天）。
- **计数器 < 已落库 epoch（含键缺失）**：判为计数器倒退。经 `quarantineOrderingConflict(label=DeadLetterOwnerEpochRegressed)` 进死信，`db_owner_epoch_guard_total{outcome="regressed"}` 计数，打 `OWNER-EPOCH-REGRESSED` 日志。
- **探测读失败**：按 owner epoch guard unavailable 走 `deferFailedTask`，耗预算（`redis_unavailable`，L-04 §4.3 口径），预算用完以 `budget_exhausted` 进死信，载荷不丢。不进 `retryBudgetExempt`。
- `legacy` 不探测。
- **判据为什么正确**：已落库 epoch 只记录被铸造过、并且有写带着它落了库的值；计数器只升不降（铸造与回滚都用 INCR，`ownerepoch.go` 包注释）。正常运行时任何被铸造的 epoch 都 ≤ 计数器，所以无论两次读的先后顺序如何，都不可能出现「计数器 < 已落库 epoch」。出现就说明计数器倒退了。
- **残余**：计数器重新追过已落库 epoch 之后，探测失效，倒退期间发出的写与僵尸写无法区分。探测只是检测手段，处置与根治见 §9.1。
- **依赖**：探测要求 go/db 能读到 owner_epoch 所在的 Redis。今天 go/db 的 Redis 客户端与 scene_manager 是同一个核心实例（本地 `db.yaml` 与 `scene_manager_service.yaml` 都是 127.0.0.1:6379，K8s 的 db RedisClient 也是核心实例）[C]。G4a 拆分第 2 批计划把 `consumer:applied_epoch:*` 迁到 redis-mapping、owner_epoch 留在核心实例（`shared-redis-split-by-purpose.md`），拆分后 go/db 要单独配一个指向核心实例的客户端。两份设计都要登记这条依赖（§14 第 19 条）。
- 不采用「`dual` 下 Redis 守卫判 stale 一律进死信」：真僵尸与迟到的退出存盘（K1）本来就是这道守卫的设计对象，全部进死信会把整行载荷堆进 noeviction 的共享 Redis，告警一直是噪声，而且仍然分不出哪条是计数器倒退（§15）。

### 5.2 游标判过期时让 epoch 说话（G3）

现状：`handleTask` 里 `orderingForTask` 判 `orderingStale` 即 ack 丢弃，之后才轮到 `guardOwnerEpoch` [C]。新主的写在重试队列里、老主的迟到写先落库并推进游标时，新主的写回来会被判过期丢掉。

放开条件（同时满足）：`Mode=dual`、`Fencing.EpochOverride=true`、class-1、MsgType 是围栏表（§4.1）。不加「围栏表」时，无围栏表上游标已判过期的旧写会被放过，送到无条件 `Save`，覆盖更新的数据，比今天差。

| 游标 | 已落库 epoch 的判定 | 处理 |
|---|---|---|
| apply | — | 照旧 |
| stale | `match`（相等）或 `stale`（更小）| 照旧丢弃。理由：同一 epoch、同一分区内 offset 更小就是更早发出的写；override 落库后游标被改写到那笔写（见下），这个前提重新成立 |
| stale | `advance`（任务 epoch 更大）| **不丢**，继续走到 SQL，由围栏裁决（`epoch_override`） |
| stale | `first`（游标在、epoch 记录不在）| 照旧丢弃，计 `db_ordering_guard_total{outcome="stale_no_epoch"}`。游标存在而 epoch 记录不存在什么也证明不了：class-3 的写不推进 epoch，另外还可能是键丢失 |
| incomparable | — | 照旧进死信 |

override 落库之后：

- SQL 结局为 `applied` 或 `equal` 时，用 §5.5 的同一条脚本原子完成三件事：核对排序锁令牌（锁与游标在同一个 Redis）；把游标**改写**为本任务的 (partition, seq)，允许后退；把已落库 epoch 取 max 推进到任务 epoch。脚本失败或返回 0 就延后，不能只记日志。
- **为什么必须改写**：v0.2 写的是「成功落库后不推进游标」。那样游标停在老主迟到写的位置、已落库 epoch 已是新 epoch，同 epoch 的另一笔较新重试（offset 同样小于游标）会落到上表第 2 行被丢。重试是 FIFO（`pushPayloadDurably` 的 LPUSH 加认领脚本的 RPOPLPUSH），两笔同 epoch 的重试排在老主迟到写之后时，最终落库的可能是较旧那笔（§17 状态机 F1）。这比今天好（今天两笔都在游标处被丢），但 F-1 不成立。
- **为什么后退是安全的**：被让出来的 offset 区间里只有两类写。老 epoch 的写，由同一脚本已推进的 epoch 守卫挡住；同 epoch 的更新写，按 apply 进 SQL，由围栏裁决。
- 失租后重试得到 `equal` 时走同一脚本（§5.4）。
- **残余**：游标后退之后，epoch=0 的写（class-3）重投到 `legacy` 实例时会越过游标。切 `dual` 的前置要求 `legacy_zero` 不增长（§8.3），与 §8.5 的进入条件相同。

放行为什么安全：

- **安全性来自 §5.5 的不变量，不依赖围栏精度。** 不变量是「游标被某笔写推进 ⇒ 已落库 epoch ≥ 该写的 epoch」。在它之下，stale + advance 意味着推进过游标的写的 epoch 都小于任务 epoch，任务在 epoch 次序上更新。v0.2 写的「class-1 的围栏是精确的，放过去最多多花两条语句」删除：围栏可能落后（`legacy` 写不推进围栏），落后时放行就是覆盖。
- **旧二进制做不到原子推进**：它先 SET 游标、再单独 EVAL 推进 epoch，后者失败只记日志。它在场时会出现「游标已推进而 epoch 未推进」：一笔写由旧实例落库（不推进围栏），游标 SET 成功、epoch EVAL 失败（v0.2 的新二进制 `off` 也是这两步，v0.3 起新二进制一律走 §5.5）；之后任何实例（重试队列各实例共用）认领一笔更旧的同 epoch 重试，游标判 stale、epoch 判 advance，被落后的围栏放过，覆盖更新的数据。今天这笔会在游标处被丢掉（§17 状态机 F2、事实 F5）。所以 `EpochOverride` 有门禁（§8.3），旧二进制在场时不得开。
- **只对 class-1 放开**：在 §5.5 之下，class-2 的 advance 放行同样成立；本批只开 class-1，理由是控制范围（§15 同步）。class-2 第二步不移动 `fs`，但只有同一 epoch 内先后出现 class-1 与 class-2 两种生产者（同一 epoch 里 C++ 版本发生变化），再叠加一次 epoch 推进失败，才可能被放行路径利用；这跟随 §14 第 8 条的结论，本批不另做处理。
- `legacy` 不放开：没有围栏兜底。
- 新增计数 `db_ordering_guard_total{outcome=apply|stale|stale_no_epoch|incomparable|epoch_override}`，现在 stale 只有一行 INFO 日志。
- 不采用「stale + match 也交给 SQL」（评审改法 b）：它依赖围栏精确（全 zone 都是 `dual`），并且会把重平衡时成批的 stale + match 重投都送进 MySQL；如果将来要做，只对 `fromRetry` 的任务开放（§15）。

### 5.3 批内合并跟着围栏走（只在 `dual`、只对围栏表）

`processTaskBatch` 对同一 `(key, msgType, originPartition)` 保留 offset 最大的一条，其余标记为被取代并 ack [C]。

改法（只在 `dual`、只对围栏表）：两条都是 class-1 且 epoch 相同时，保留 `save_seq` 更大的那条（相同则保留后到的）；其它情况照旧。

- 正常情况下两种比法结果相同：C++ 生产者开了 `enable.idempotence`（`kafka_producer.cpp:59-76`），同一分区内 offset 顺序就是发送顺序。
- 这条改动是为了让合并与围栏用同一把尺子。否则一旦两者不一致，被保留的那条会被围栏判 `older_seq`，被取代的那条已经 ack，两条都没落库（附录 B2 指出的叠加）。
- `legacy` 下不生效：`legacy` 没有围栏，若按 save_seq 保留，被取代那条的 offset 更大，一旦重投会被无条件写入，与「`legacy` 等同今天」矛盾。

### 5.4 失租

现状：写完之后核锁发现失租，按普通失败耗重试预算。哨兵 `errOrderingLeaseLost` 已由 L-04 第 1 批引入（`store_failure.go`），只用于计数（`deferReason=lease_lost`），注释写明「是否豁免预算，条件写设计另议」，`retryBudgetExempt` 里没有它 [C]。

v0.3 的做法：**复用 L-04 的哨兵，暂不并入豁免**（与 v0.2 的「新增哨兵并入豁免」相反）。§6.2 令牌脚本返回 0、§5.5 推进脚本返回 0 时也包这个哨兵。

- 不豁免的理由：豁免同样会无界空转（失租反复出现说明 Redis 或进程有系统性问题）；失租的写已经在 MySQL 里，重试时 SQL 得到 `equal`，再在新的锁下补发缓存、走 §5.5 脚本推进；预算用完进死信也不丢 MySQL 里的数据。
- 不豁免的代价：同一笔写连续 3 次失租会以 `budget_exhausted` 进死信。这时 MySQL 已有该版本，但缓存可能停在旧值直到 TTL（≤3600 秒）、游标与已落库 epoch 没推进，放大 §6.5 所说的「缓存落后于 MySQL」（父键缺失时 login 会拿旧缓存拼数据）与 §6.3 的围栏落后。
- 这一点核实各组意见有分歧，列为 §14 第 14 条待拍板。若负责人选择豁免：在 `retryBudgetExempt` 里对 `errOrderingLeaseLost` 加一个分支（只对 write），并接受无界重试，靠 `db_task_deferred_total{reason="lease_lost"}` 观察。

### 5.5 游标与已落库 epoch 的原子推进（新二进制，与模式无关）

现状（工作区）：`handleTask` 尾部核锁之后先 `markAppliedSeq`（普通 SET 游标，不设过期），再 `markAppliedOwnerEpoch`（单独 EVAL `luaMarkAppliedEpoch` 取 max）。后者失败只打 ERROR，不延后也不回滚；用的是 worker 的 ctx，而 `Stop` 先 cancel 再等待，cancel 恰好落在 SET 返回之后、EVAL 之前就会留下「游标已推进、epoch 未推进」。这个窗口只有一次 Redis 往返，Redis 抖动同理，概率低但真实存在 [C]。L-04 第 1 批没有改动这两处。

改法：新二进制无论 `legacy` 还是 `dual`，都用一条脚本取代这两次调用：

```lua
-- KEYS[1] 排序锁键   KEYS[2] 游标键（appliedSeqKey）   KEYS[3] 已落库 epoch 键（appliedEpochKey）
-- ARGV[1] 锁令牌     ARGV[2] 游标新值（encodeAppliedCursor，格式不变；空串 = 不写游标）   ARGV[3] 任务 epoch（0 = 不动 epoch）
if redis.call('GET', KEYS[1]) ~= ARGV[1] then
  return 0
end
if ARGV[2] ~= '' then
  redis.call('SET', KEYS[2], ARGV[2])
end
local e = tonumber(ARGV[3])
if e > 0 then
  local cur = tonumber(redis.call('GET', KEYS[3]) or '0')
  if cur == nil then
    return redis.error_reply('applied owner epoch is not a decimal integer')
  end
  if e > cur then
    redis.call('SET', KEYS[3], ARGV[3])
  end
end
return 1
```

- **游标写入**：正常路径（`orderingApply`）在持锁下判定、脚本里再核令牌，所以只进不退，不需要在 Lua 里比较游标（跨分区不可比）；override 路径（§5.2）写入本任务的 (partition, seq)，允许后退。沿用 `markAppliedSeq` 现有的跳过条件：`task.seq == 0` 或非 write 时不写游标（传空串），epoch 照常取 max。
- **不变量**：「游标被某笔写推进 ⇒ 已落库 epoch ≥ 该写的 epoch」。§5.2 的放行只依赖它。
- **失败即延后**：脚本返回 0 包 `errOrderingLeaseLost`，Redis 错误归 `redis_unavailable`，都走 `deferFailedTask`、耗预算（§5.4）。删掉 `markAppliedOwnerEpoch` 注释里「失败只记日志」的口径。重试时 SQL 得到 `equal`，再走同一脚本。
- 键名、值格式、不设过期都不变，旧二进制照常读写。epoch 是从 1 起的小计数器，Lua 的 double 比较是精确的（同现有 `luaMarkAppliedEpoch` 的注释）；已落库 epoch 键出现非整数时报错（→ 延后 → 死信），不当缺键处理，与 `ReadAppliedEpoch` 的口径一致。
- 推进脚本放在 `handleTask` 尾部，缓存发布仍在 `runDBOp` 里（§6.2），两条脚本各自核令牌，本批不合并。
- **最小替代**（若实现上先求最小改动）：调换顺序，先推进 epoch、再 SET 游标，任一步失败都延后。epoch 只升不降、SQL 已经落库，提前推进 epoch 没有害处，这同样保住不变量。**不能**只把 `markAppliedOwnerEpoch` 失败改成延后：失败时游标已经 SET 到本任务的 seq，延后之后的重试会被游标判 stale 丢掉，epoch 永远补不上。最小替代不核令牌、做不了 override 的游标改写，所以开 `EpochOverride` 之前必须换成脚本。
- 三把键（排序锁、游标、已落库 epoch）今天都在同一个 Redis 客户端上 [C]，单脚本可行。负责人已定 Redis 按 player_id 分片（§6.4），分片后三把键必须同一个 hash tag，并入 §14 第 3 条。

### 5.6 G5 的处置

G5（瓶颈文档 §4 的缺口表）：游标与已落库 epoch 记录缺键时放行。

- **class-1、围栏表、`dual`**：由围栏兜住。Redis 记录缺失只会让写多走到 SQL，由围栏裁决。§5.2 对「游标在、epoch 记录不在」照旧丢弃（`stale_no_epoch`），不扩大放行。
- **class-2 / class-3**：`dual` 下仍开着，与今天相同。
- **游标不做 Lua 取最大值**：class-1 的正确性不依赖游标；跨分区不可比；正常路径的单调由持锁判定加 §5.5 的令牌核对保证。
- 这条残余写进 §8.5 的 `sql` 进入条件。瓶颈文档把条件写登记为覆盖 G1 / G3 / G5，要同步订正为「G1；G3 部分；G5 只关 class-1」（§16）。
- 两张子表不原子见 §1.3。

---

## 6. 子表缓存

### 6.1 现状

- 键 `{MsgType}:{Key}`，TTL 3600 秒，普通 `SET`，没有版本也没有条件 [C]。
- 写入点只有 go/db 两处：读路径 `publishDBReadResult`、写路径 `writeBackDBCache`。读取方是 login（`ensure_player_all_data_async.go`；`sync_loader.go` 的 `TriggerPlayerDataPreload` 逻辑相同，但全仓无调用方）和 `cmd/verifier`。删除方只有 `tools/merge_zone/player_rows.go` 的 `invalidatePlayerCaches` [C]。
- 两个缺口：写路径的缓存回写在核锁之前（旧实例恢复后会把缓存写回旧值）；读路径不拿锁、无条件覆盖子表缓存（瓶颈文档 G4，后果是缓存被旧值覆盖）。本节解决这两个缺口，不解决「冷登录拼出旧父键」，后者见 §6.2 残余窗口 3。
- 父键缺失时 login 用子表缓存或读任务的结果拼 `PlayerAllData`，所以缓存变旧会被固化成回档；读任务的结果不经缓存（§6.5）。

### 6.2 新做法（与模式无关，`legacy` 下也生效）

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

- 返回 0 = 已经不持有锁：不写缓存，包 `errOrderingLeaseLost` 延后（§5.4，耗预算）。重试时拿到新锁，SQL 得到 `equal`，再补发缓存。
- 脚本用 `redis.NewScript(...).Run`（EVALSHA，NOSCRIPT 时自动回落 EVAL）。往返数不变（一次 SET 换成一次 EVALSHA）。
- 令牌在 `locker.TryLockResult` 里是私有字段（`go/db/internal/locker/locker.go`），给它加一个方法（例如 `SetIfOwned`），不把令牌暴露出去；§5.5 的推进脚本用同一个方式拿令牌。
- 为什么这是真栅栏而不是又一层租约：校验与写入在 Redis 里是同一个原子步骤，而缓存本身就在这台 Redis 上。MySQL 做不到这一点（它看不到 Redis 的令牌），所以 MySQL 侧才需要围栏列。
- 被拒的写本来就不发布缓存（§4.4），所以能让缓存变旧的只剩「SQL 已落、缓存迟发」这一种，令牌校验正好挡住它。

**读路径**：`SET` 换成 `SET NX`。

- login 在子表缓存缺失、`GET` 出错或值解析失败时发读任务（`ensure_player_all_data_async.go:79-89`；`sync_loader.go` 的 `TriggerPlayerDataPreload` 逻辑相同，但全仓无调用方）[C]。常态下 NX 成功。v0.2 写的「只在缺失时才发」不准确：`GET` 超时时键可能是好的，值解析失败时键里是坏值，这两种情况 NX 都不写。
- **NX 只决定缓存键。** 读任务回给 login 的 `TaskResult.Data` 始终是本次 SELECT 的行（`publishDBReadResult` 用同一份数据做缓存写和结果发布）。父键缺失时 login 用它或子表缓存拼父键（`ensure_player_all_data_async.go:79-89`、:145、:178-186）[C]。
- 读与写并发时，读到的旧行写不进已有的新缓存；即使读先写进去，写路径随后的发布会覆盖它。

**残余窗口**：

1. 读到旧行之后、NX 之前，缓存键恰好过期或被合服工具删掉。这个窗口换成带版本的写法也存在（键不在时无从比较），可以接受。
2. **缓存里是解析不了的值时 NX 不覆盖。** 本次登录仍拿到 MySQL 的行；坏值留到 TTL（≤3600 秒）或下一笔写的令牌脚本覆盖为止。login 随即写回父键，后续登录走快路径，所以代价只是父键再次缺失时多一次读和一行 ERROR。坏值来源很少：值只由 go/db 对同一描述符做 `proto.Marshal` 写入，现实来源是人工 SET，或 §6.4 的值封装在 login 升级之前就被写出（所以 §6.4 规定了上线顺序）。自愈方案记在 §15。
3. **冷登录与在途写的竞态**（今天同样存在，NX 与条件写都挡不住）：
   - 现象：父键缺失、而该玩家仍有未落库的写（在 Kafka 未消费，或在 ready 里延后）时，login 拼出的父键比在途写旧。子表缓存里还是旧版本时 login 连读任务都不发，直接用缓存拼父键；R 先于 W 完成时 NX 也不参与。C++ 加载后用更大的围栏覆盖，在途写的内容丢失（在途写后到时会被游标或 epoch 守卫丢掉）。
   - 入口：父键在 scene 首次存盘后不带 TTL，主 Redis 是 noeviction + AOF；Redis 整体丢失时 ready 一起丢；合服有 P3 / P4 排空门禁。现实入口只有手工删父键，或 Redis 丢失时写还在 Kafka。
   - 窗口：L-04 之后写的存储类失败不耗预算，存储不可用期间写一直留在 ready；冷却期内读立即失败，恢复后读从 Kafka 立即执行，而 ready 每实例每拍至多认领 min(LLEN, 200) 条。窗口约为「停机时长 + 排空时长」（L-04 之前约 3 拍就进死信，那本身就是丢）。
   - 修法方向不是读写分区对齐（①）：W 在 ready 里时不受分区顺序约束；④ 同样不会让读去等同 key 的待重试写。方向是 go/db 在写延后进 ready 时按 key 维护「有待落写」标记、ack 或进死信时清除，读任务看到标记就回失败或延后让 login 重试（或者 login 冷组装前先查标记）。批次与归属待定（§14 第 15 条）。

这两处改动与 `Fencing.Mode` 无关，`legacy` 下也生效（只会更严）。它同时就是瓶颈文档 G1 治标第 ③ 条「缓存回写挪到核锁之后」的原子版本；G1 小 PR 若先落地，这里直接复用。

计数：`db_fence_cache_publish_total{path, result}`，`path=write` 时 `result` ∈ written / not_owner，`path=read` 时 ∈ written / exists（§10）。计数加在 `writeBackDBCache` 与 `publishDBReadResult`，不涉及 L-04 已改的 handler 签名。

### 6.3 值里自带围栏（下限，不是精确值）

go/db 盖过围栏的消息原样进缓存，所以缓存值（序列化的 `player_database`）里带着 18 / 19 号字段。login 不需要改：它把整条子表消息原样拼进 `PlayerAllData`（`ensure_player_all_data_async.go:178-186`）。C++ 冷加载时据此播种（§7.2）。

**缓存值里的围栏是行内围栏的下限，不保证相等**：

- `legacy` 写出 `(0, 0)`（`PreserveFields` 只保留行内值，不修改消息；`SaveConditional` 不改 message）[C]；
- class-2 第二步写出 `(E, 0)`，而行内 `fs` 被保留（可选的回填见 §4.3）；
- SQL 已落而缓存未发布期间（令牌脚本返回 0、`writeBackDBCache` 失败），整条缓存落后，围栏也落后。

只有读路径的发布，以及 class-1 `applied` / `equal` 的发布，与行内相等。**任何地方不得把缓存里的围栏当精确值用。** 方向安全：只会让播种下限偏低，不会误抬。

### 6.4 留给 `sql` 模式的问题（本批不做）

`sql` 模式下 class-1 不再取排序锁，令牌校验用不了，缓存必须自带可比较的版本。方向与交接要求一致：

- **不另开 `{MsgType}:{Key}:fence` 键。** 两个键必须同生同灭，任何只删或只写其中一个的工具都会造成缓存回退；Redis 分片后还要求两个键同槽。
- **首选值封装**：在缓存值前面加定长头 `0x00 | 版本 | epoch(8 字节大端) | seq(8 字节大端)`。合法的 protobuf 编码不会以 `0x00` 开头，新旧格式可以区分。一条脚本用 `GETRANGE` 取头比较后再写，单键、原子。代价是 login 要先能读两种格式，再让 go/db 写新格式，这是一次跨服务的格式升级。
- **上线顺序**：login 能读新格式之前，go/db 不得写新格式。否则每次冷登录都会因解析失败回落读库，而读路径的 NX 修不了坏值（§6.2 残余窗口 2）。
- **与 Redis 分片方案的关系**（负责人已定：以按 player_id 分片为准，交接 §5 第 16 项）：
  - 值封装是单键，不需要 hash tag。
  - §6.2 的脚本同时访问排序锁键和子表缓存键；§5.5 的推进脚本同时访问排序锁键、游标键和已落库 epoch 键。今天主 Redis 是单实例，没有问题。按 `{player_id}` 分片后，这些键必须落在同一个 tag 下；排序锁键的改名还要与 `tools/merge_zone/relocate.go` 里的锁键镜像同批改。瓶颈文档 §6.5 的分片修订要把这两条约束算进去。

### 6.5 缓存与读结果各自保证什么

| 对象 | 保证 | 不保证 |
|---|---|---|
| 子表缓存（写路径令牌脚本发布）| 只前进不后退：只有持锁者能写，被拒的写不发布 | 不保证等于 MySQL：SQL 已落而缓存未发布（失租、发布失败）期间整条落后，靠重试补发收敛，失租耗尽预算时停到 TTL（§5.4）；跨进程撞号时 `equal` 补发的 body 可能与 MySQL 不同（§4.4） |
| 子表缓存（读路径 NX）| 不覆盖已有值 | 不修坏值（§6.2 残余窗口 2）|
| 缓存值里的围栏 | 行内围栏的下限 | 不精确（§6.3）|
| 读任务结果 `TaskResult.Data` | 本次 SELECT 的行 | 不经缓存、不受 NX 影响；可能比在途写旧（§6.2 残余窗口 3）|

F-4（§1.2）按这张表理解：缓存单调，但可能落后。不采用「NX 失败后改回缓存现值」：SQL 已落而缓存未发布的窗口里，SELECT 读到 v3、缓存是 v2，NX 失败后回 v2 等于新增一条回档路径，而今天回的是 v3（§15）。

---

## 7. C++：save_seq 发号

### 7.1 约束

- 同一玩家、同一 epoch 内，后发出的存盘必须拿到更大的 save_seq。
- **实体会在同一 epoch 内销毁重建**：同落点重连不铸造 epoch（`go/scene_manager/internal/logic/owner_epoch.go:41-46`），而实体可能已被清掉后重新加载（PL:3623-3628）。次数没有上界 [C]。
- **Redis 父键不是「已发出 DBTask」的上界**：DBTask 在 `Save()` 入队之后立刻发出，不等 Redis 应答（PL:3683-3741）；Redis 那次写可能还在排队、被后一次存盘顶替、或被 CAS 拒掉 [C]。
- **scene 进程重启后 epoch 不一定推进**：新进程可能复用同一个 node_id，而 location 仍指向它时走「不铸造」分支（`enterscenelogic.go:514-517`、:643）[C]。这条时序在真实部署下是否可达没有核实。node_id 取最小空闲号、不绑主机，新进程可以落在另一台主机；但崩溃后旧的 node_id 占位键要等 etcd 租约过期（`NodeTTLSeconds`，权威值 180 秒，`bin/etc/base_deploy_config.yaml`）才能被复用，所以「同一个 node_id 的新进程」与旧进程之间至少隔着分钟级的停机 [C]。
- **dev 旁路**：`AllowUnsafeCrossNodeHandoff=true` 时跨节点交接无标记放行、不铸造，`ReleasePlayer` 异步派发，新旧两个进程同持一个 epoch 并各自发号，序号先后只靠两台主机的墙钟（`enterscenelogic.go` 的旁路分支）[C]。本地 `scene_manager_service.yaml` 开着它，代码默认 `false`。单机 dev 下两个进程读同一个墙钟，序号先后就是发号先后；只有多主机误开旁路才依赖主机间偏差。只允许单机 dev。
- 今天这条路径按 broker 分配的 offset 定序，与时钟无关。发号不能把「时钟回拨」变成新的静默丢盘（附录 B2）。

### 7.2 发号器

**进程级发号器，不设每玩家组件。**

```
Next(nowMs)           = 若 last == UINT64_MAX：显式失败（拒绝本次存盘并打 ERROR，不回绕）
                        否则 last = max(last + 1, nowMs << 10)
Observe(floor, nowMs) = 若 floor > (nowMs + 容差) << 10：不采纳（不抬高 last），打 ERROR 并计数
                        否则 last = max(last, floor)
```

- `last` 是 scene 线程上的一个 `uint64`，整个进程共用。它对这个进程里的所有存盘严格递增，所以同一玩家的任意两次存盘（不管中间实体重建过几次）都有序。
- 发出的值在玩家之间共用一条数轴，每个玩家看到的是有空洞的递增序列。围栏只要求递增，不要求连续。
- **类型与放置**：发号器是普通值类型 `SaveSeqAllocator`（`Next` / `Observe` 两个方法加一个读当前值的 `Current`）。它与纯函数 `NextSaveSeq(last, nowMs)`、`SaveSeqFloorFrom(const PlayerAllData&)`（返回三路的 max，见下）一起放在 `cpp/libs/services/scene/player/system/save_seq.h`，时间显式传入。生产实例作为 `ThreadLocalIdGeneratorManager`（`cpp/libs/services/scene/core/system/id_generator.h`，`thread_local tlsIdGeneratorManager`，与 buff / skill 发号器同处）的一个成员；它不在 `tlsEcs`（`EcsContext`）里，`tlsEcs.Clear()` 不触及它 [I，落码时核对]。单测各自构造局部实例，不碰全局实例，所以不需要复位入口。不用 static。
- `nowMs << 10` 约为 2^51，离 uint64 上限很远；有了 `Observe` 的上界，`last` 正常情况下到不了 `UINT64_MAX`，饱和检查只防坏数据。
- **为什么要上界**：`last` 全进程共用，`Observe` 无条件接受时有两种坏法。其一，回绕：一行来自 `debug_import` 或手工改库的 `fence_save_seq = 2^64-3` 被冷加载进来，之后两笔发号得到 2^64-2、2^64-1，第三笔 `last + 1` 回绕成 0 退回墙钟；前两名互不相干的玩家行内围栏停在近 2^64，同 epoch 内之后的存盘全被判 `older_seq`。其二，一台时钟错到几十年后的主机发出的序号写进父键和 MySQL 行，任何加载到这些玩家的进程都被抬到同一量级、长期走 `last + 1`，墙钟下限失效，新旧进程从同一个下限各自 `+1` 还会撞成 `equal`（§4.4）。污染单独存在时不丢数据，丢盘仍要 §7.4 的条件或 dev 旁路。data_stress 写的合成 player_id（从 1,000,000 起）不经 login 与 scene 加载，进不了 `Observe`。
- **容差**按可接受的主机间时钟偏差取值（建议分钟级），写进 scene 配置，具体值待定。
- **超界玩家的处置（二选一，§14 第 13 条）**：
  1. 不采纳：该玩家在本 epoch 内的每次存盘都会被判 `older_seq` 丢弃，直到下一次铸造 epoch；靠 `DbFenceOlderSeqHigh` 告警与 §9.1 的抬计数器流程处置。这是把静默扩散换成该玩家的显式丢盘，采纳时必须接受。
  2. 个人下限：只把超界值挂在该玩家实体上（该玩家发号取 `max(全局 Next, 个人下限 + 1)`），不污染其他玩家，也不丢盘。代价是只为超界玩家多一个小组件；实体重建时加载会重新得到它，不受 §15「每玩家组件归零」的问题影响。建议 2。

**播种**：`InitPlayerFromAllData` 加载玩家时调用一次

```
Observe(SaveSeqFloorFrom(all), nowMs)
// SaveSeqFloorFrom = max(all.last_save_seq(),
//                        all.player_database_data().fence_save_seq(),
//                        all.player_database_1_data().fence_save_seq())
```

`InitPlayerFromAllData` 里只剩这一行调用。它会连带触发初始化事件、`EnterScene` 与 `SnapshotSystem::CaptureAndSend`，`cross_zone_test` 宿主里没有先例能跑通，所以这一行没有单测覆盖，由代码评审与 §11 第 7 步覆盖；单测只测纯函数 `SaveSeqFloorFrom`（§7.5）。

两路来源互补：

| 加载路径 | `last_save_seq` | 子表里的 `fence_save_seq` |
|---|---|---|
| 父键存在（常态：C++ 存过一次之后父键没有 TTL）| 上一次落到 Redis 的存盘用的序号 | 0（C++ 存盘的 body 不带围栏）|
| 父键缺失，login 从子表缓存或 MySQL 拼出来（冷路径）| 0 | ≤ 已落库的序号。子表缓存命中时取缓存（§6.3 的下限）；缓存缺失、login 读 MySQL 时才等于行值 |

- 不看 epoch 是否相同：少一个分支。代价是进程级下限会被任意一名玩家带来的值抬高，并经父键和行内围栏在进程之间传播；上界就是为此加的。v0.2 写的「没有坏处」删除。
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

进程级发号器 + 两路播种之后，墙钟只在「新进程 + epoch 没推进 + 种子落后于已发出的序号」时才起作用。会出错的条件要同时满足四条：

1. scene 进程崩溃，崩溃前有存盘的 DBTask 已发出但 Redis 那次写没落地；
2. 同一个 node_id 的新进程接回这名玩家，且 scene_manager 走了不铸造分支；
3. 播种的两路来源都看不到崩溃前已发出的最大序号：父键没丢（父键是旧快照）；或父键丢了，但那笔 DBTask 还没落库（在 Kafka 或 ready 里；L-04 之后存储不可用期间它会一直留在 ready）；或子表缓存里的围栏偏低（§6.3）。v0.2 把「父键丢了走冷路径」写成安全侧，不成立；
4. 新进程发号时的墙钟早于旧进程最后一次发号的时刻：同机时钟回拨，或新进程落在另一台时钟更慢的主机。量级：崩溃后 node_id 要等 etcd 租约（180 秒）过期才能复用（§7.1），停机时长的下限是分钟级，所以需要的回拨或主机间偏差也在分钟级，属于运维故障，不是几秒偏差就能触发。

另有一条 dev 旁路：`AllowUnsafeCrossNodeHandoff=true` 时新旧两个进程同持一个 epoch 并各自发号，先后只靠墙钟（§7.1）。只允许单机 dev。

后果：新进程的存盘被判 `older_seq`，ack 丢弃，直到序号追上。被丢的若是下线前、或状态不再变化前的最后一笔，快路径不再发 DBTask（`SavePlayerToRedisImpl` 的脏比较快路径），MySQL 停在旧值，直到该实体下一次真正写盘（通常是下次上线后的首存）；go/db 的 ack 丢弃对 C++ 不可见。期间父键丢失即回档。

缓解与观测：

- `db_fence_write_total{outcome="older_seq"}` 建基线告警（§10）。
- 要求 scene 主机的时钟同步用 slew 不用 step（附录 M6，待运维确认），并补 **scene 主机间时钟偏差的监控**：slew 管不到主机间偏差。
- 彻底关掉的办法是让 scene_manager 在进程换代时必定铸造 epoch（`PlayerLocation` 现在没有进程代际字段）。这是跨 zone 归属那条线的改动，列为待定项（§14 第 8 条）：先核实条件 2 是否可达；可达时它是多主机环境切 `dual` 的前置。
- 「用 `AllowUnsafeCrossNodeHandoff=false` 再跑一遍端到端」不写成 §11 第 7 步的硬性要求：`scene_manager_service.yaml` 注明标记链路尚未编译、未跑过测试，它应作为单独的验收项。

### 7.5 单测

放在 `cpp/tests/cross_zone_test/cross_zone_test.cpp`（单编译单元，只走 MSBuild，没有 CMakeLists）。全部在局部 `SaveSeqAllocator` 实例或纯函数上测，不碰 `tlsIdGeneratorManager`。用例名统一带 `SaveSeq` 前缀，§11 第 6 步按名逐条核对：

- `SaveSeq.NextMonotonic`：单调。
- `SaveSeq.NextMonotonicUnderClockRollback`：时钟回拨仍单调。
- `SaveSeq.NextSameMillisecond`：同一毫秒多次调用。
- `SaveSeq.ObserveRaisesFloor`：`Observe` 之后的下一个值大于下限。
- `SaveSeq.ObserveRejectsBeyondBound`：超界下限不抬高进程级 `last`。
- `SaveSeq.NextSaturatesWithoutWrap`：`last == UINT64_MAX` 时显式失败，不回绕。
- `SaveSeq.FloorFromAllData`：纯函数 `SaveSeqFloorFrom` 对带 `last_save_seq` / `fence_save_seq` 的 `PlayerAllData` 取三路 max（替代 v0.2「加载后发号器的下限」那条：`InitPlayerFromAllData` 在宿主里跑不通，见 §7.2）。
- `SaveSeq.SnapshotStripsLastSaveSeq`：快照不保留 `last_save_seq`。
- `SaveSeq.SecondUnchangedSaveHitsFastPath`：连续两次无改动存盘，第二次命中快路径。做法：用 `HandlePlayerAsyncSaved` 喂一份带 `last_save_seq` 的落地消息；先断言快照里 `last_save_seq` 为 0 且 `dirty_save::IsEqual(新 marshal, 快照)` 为真，再调 `SavePlayerToRedis` 断言返回 false。宿主里 `GetPlayerDataRedis()` 为空，实现出错时先在断言处失败，不会在写盘段对空指针解引用、连带后面的用例不跑。
- `SaveSeq.PersistedPayloadCurrentIgnoresSeq`：`IsPersistedPayloadCurrent` 不受序号影响。
- 若采纳 §7.2 超界处置方案 2，加 `SaveSeq.PersonalFloorBeyondBound`。

测试宿主没有 Redis，`SavePlayerToRedisImpl` 的写盘段跑不到（现有用例只走快路径或直接调落地回调）。「DBTask 里带上了 save_seq」这一点靠本地端到端验证（§11 第 7 步）。

`Replace` 是头文件内联实现，`bag_test`（`player_battle_settlement_test.cpp`）与 `currency_test`（`asset_op_system_test.cpp`）直接调用它，改它会直接进这两个测试二进制，所以 §11 第 6 步要跑这两个工程。

---

## 8. 上线分期与兼容

**分期顺序**：Stage 0 → 1a（新 go/db，`legacy`）→ 1c（新 C++）→ 1b（逐 zone 切 `dual`）→（满足门禁后）开 `EpochOverride`。至少金丝雀 zone 必须先上新 C++ 再切 `dual`，金丝雀才覆盖 class-1（`older_seq` / `equal` / `epoch_override`）这条风险最大的路径；class-2 只在 C++ 回滚时出现，由单测和真库用例覆盖。`legacy` 下 go/db 不看 save_seq，先上 C++ 不改变 go/db 的行为。小节编号沿用 v0.2。

### 8.1 Stage 0：proto 与 DDL

1. 改三个 proto（§3），重新生成（§11 第 1 步），同一次改动里更新 §3.4 的七处清单。
2. **库清单穷举**（Stage 1a 的门禁）：
   - 在每个 MySQL 实例上 `SHOW DATABASES`，取 `zone_%_db`、`player_store_%_db` 两个家族，加上各 zone 的 `AllowedDatabases`。按需打开从不建库，`AdmitStore` 也只放行白名单与这两个家族，所以这个集合覆盖了 go/db 可能打开的全部库。可选：用 `player:placement` 记录里出现过的落点编号交叉核对。
   - 每个库用 `cmd/migrate`（落点库加 `-storage-id`）跑 `-command plan`，核对只有 4 条 ADD COLUMN，再 `-command up`；之后 `SHOW COLUMNS ... LIKE 'fence_%'` 核对两张表各返回 2 行。全部通过才进 Stage 1a。
   - 原因：启动闸只拦本 zone 库，漏迁的落点库不会拒启，只会让这批玩家的存盘一直延后（§3.2）。合服 runbook §4.4 有同类前置，可作先例。
3. 这一步之后、新二进制上线之前，旧 go/db 照常运行：`Save` 只写它认识的列，围栏保持 0。旧二进制会对每个库持续打「多余列」的 `SCHEMA-DRIFT` ERROR 日志，是预期的噪声。
4. `tools/merge_zone` 按列名拷行，要求两库列集合完全相同（`player_rows.go`）。所有库迁完之前不能合服或搬库；它自己会拒绝，方向安全。

### 8.2 Stage 1a：新 go/db，`Mode=legacy`

- 新二进制带着新 proto，启动闸会要求本 zone 库已有围栏列，否则拒启；围栏表判定（§4.1）要求两张玩家表在集合里，否则拒启。
- **`legacy` 下生效的变化**（与模式无关）：
  - 围栏表的 SQL 从 `Save` 换成只带 `PreserveFields` 的 `SaveConditional`（语句相同，影响 0 行时多一条存在性查询）；非围栏表仍是 `Save`；
  - §6.2 的缓存写法（写路径令牌脚本、读路径 NX）；
  - §5.5 的推进脚本（游标与已落库 epoch 原子推进，失败即延后）；
  - §4.6 的 `WithContext` 截止时间（连接级超时 L-04 已有）。
- **只在 `dual` 生效**：§4.2 盖章、§4.4 的结局分类与死信、§5.1 的计数器探测、§5.2（另需 `EpochOverride`）、§5.3。
- 观察项：
  - `db_task_deferred_total{reason="store_open_failed"}` 保持 0，日志里没有 `failed the read-only schema gate`（落点库漏迁，§3.2）。
  - **class-3 观察窗**（Redis 守卫在 `legacy` 下照跑、照计数）：该 zone 各 go/db Pod 的 `db_owner_epoch_guard_total{outcome="legacy_zero"}` 不增长；同期 scene 日志里 `[OwnerEpoch]` 行的 `owner_epoch_unknown` 不增长（它是进程累计值、每 30 秒打一行 WARN，不是 Prometheus 指标，所以写「不增长」，不写「为 0」）。
- 这一步是为了把「换二进制」和「换语义」分开。

### 8.3 Stage 1b：逐 zone 切 `dual`

前置条件（缺一不可）：

1. 该 zone 的 go/db 全部是新二进制。
2. **L-04 第 2 批的死信工具已落地并经 Codex 演练**：至少能按原因、按玩家列出和查看；重新入队时 RetryCount 清零；默认只演练不执行。`dual` 会让 `stale_owner` / `owner_epoch_regressed` / `legacy_after_seq` 进死信，没有工具就没有处置手段；L-04 §3.8 明确禁止用 `redis-cli` 把条目搬回 ready（预算耗尽的条目重试次数已满，下一拍会被立刻送回死信；`redis-cli` 也改不了二进制载荷里的 RetryCount）。第 2 批目前未授权（§14 第 9 条）。
3. **条件写进死信的条目带原因**：放进同一个 `kafka:dead:queue:{topic}` 列表，载荷外面套带原因的新信封（原因、时间、行内围栏、任务的 `(E, s)`；L-04 §4.1 已规划「新条目带原因和时间」）。不另开独立的死信键：合服预检 `tools/merge_zone/preflight.go` 和 `db_retry_queue_length{queue="dead"}` 都只看这个列表，独立键会绕过两者。
4. **告警已生效，留证据**：HEAD 的 db ConfigMap 已配 `MetricsListenAddr: ":9160"`（v0.2 写的「目前没有」已过时）；还要确认 ServiceMonitor / PodMonitor，并手工 apply `deploy/k8s/owner-epoch-alerts.yaml` 与 `deploy/k8s/db-task-retry-alerts.yaml`（`docs/ops/godb-pipeline-launch-todo.md` 2.5.1、2.5.2 仍是待确认 / 待做）。配置证明不了告警生效，所以要做一次演练：在金丝雀 zone 用 data_stress 对测试号段制造一条 `legacy_after_seq`，确认 `DbTaskDeadLetterAdded` 与 `DbFenceLegacyAfterSeq` 都触发，并留证据。
5. **class-3 前置**：Stage 1a 观察窗内 `legacy_zero` 与 `owner_epoch_unknown` 都不增长（§8.2）；gate / scene_manager 全部是 CZ-4 之后的版本。非 0 时先归因：还没铸过 epoch 的存量玩家在同落点重连时写的是 `(0,0)` 行，切 `dual` 后无害；由负责人确认之后可以放行。不写成硬性「恒 0」，否则可能永远卡住。理由：`dual` 把 class-3 从「总能落库」改成「行上有围栏就进死信」。
6. 该 zone 已上新 C++（Stage 1c）。
7. 多主机环境：§14 第 8 条（进程换代必铸造 epoch）的可达性已核实；可达时先做它（§7.4）。
8. 一条金丝雀 zone 先切，观察 §10 的指标至少一个高峰。金丝雀通过标准：`db_fence_write_total{class="seq",outcome=~"updated|inserted"} > 0`；记录 `older_seq` 比例与 `epoch_override` 次数（`EpochOverride` 开启之后才有）；`stale_owner` / `owner_epoch_regressed` / `legacy_after_seq` 为 0（第 4 条演练在测试号段上制造的那一条除外，演练前后各记一次计数）。

切换：改配置后滚动重启。只改 ConfigMap 不会让 Pod 滚动，要显式 `rollout restart`，并用启动日志里的生效值核对。

**开 `EpochOverride` 的条件**（在 `dual` 稳定之后，逐 zone）：

- 该 zone 已经没有旧版 go/db（没有 §5.5 原子推进的二进制）。新二进制的 `legacy` 与 `dual` 混跑不影响开启。
- 最后一个旧实例下线后，重试 ready 与各实例 processing 至少**同时为 0** 过一次（看 L-04 的 `db_retry_queue_length`）：旧实例留在重试里的任务可能已经造成「游标推进、epoch 未推进」。
- 将来可以挂到 §8.5 的按实例能力登记上，做成机器门禁；本批靠发布纪律加上述观察。
- L-04 第 2 批的死信回灌受同一门禁约束（§9.4）。

### 8.4 Stage 1c：新 C++

新 C++ 开始发 `save_seq`，class-1 流量出现。按分期顺序排在 Stage 1b 之前（v0.2 写的「可以早于或晚于」改掉：晚于时金丝雀看不到 class-1）；`legacy` 下 go/db 不看 save_seq。

**并存矩阵**：

| 并存情况 | 结果 |
|---|---|
| 旧 go/db + 已加列的库 | 旧实例只写旧列，围栏不动。安全性同今天 |
| 新 go/db（`dual`）+ 旧 go/db | 旧实例的无条件覆盖 MySQL 挡不住，互斥仍靠双方都遵守的排序锁、游标、epoch 键。旧实例不推进围栏，围栏只会落后，不会误拒新写。**旧 go/db 在场时不得开 `EpochOverride`**：旧实例「游标已推进而 epoch 未推进」的窗口会让更旧的重试越过游标、被落后的围栏放行（§5.2，§17 状态机 F2）。不开时安全性同今天 |
| 新 C++ + 旧 go/db | 旧实例不认识 9 号字段，按未知字段原样保留并带进重试载荷。安全性同今天 |
| 旧 C++ + 新 go/db（`dual`）| 走 class-2 |
| 同一 zone 内新二进制的 `legacy` 与 `dual` 实例并存 | 安全：都保留 Redis 守卫，都用 §5.5 原子推进游标与已落库 epoch，§5.2 的放行只依赖原子推进的 epoch 记录，不依赖围栏精度。`legacy` 实例的写不带条件、不推进围栏 |
| epoch=0 的存盘（class-3）+ `dual` | 行上已有围栏时判 `legacy_after_seq` 进死信；今天会落库。之后同一玩家第一笔 class-1 存盘会整行追平。由 §8.3 第 5 条的前置排除 |
| C++ 从新版回滚到旧版 | `dual` 下安全：之后的写是 class-2，走第二步，不退围栏 |

F-5（§1.2）的成立条件就是上表第 2、5 行：没有旧二进制，或者旧二进制在场时 `EpochOverride` 关着。

### 8.5 `sql` 模式的契约（预留，本批不实现）

将来要做时必须满足：

- **机器门禁**，不能只靠发布纪律：`sql` 实例不维护游标和已落库 epoch，任何还按它们判定的实例（旧二进制、`dual`）读到的是停更的值。
- 现有的 `db:capability:zone:{zone}` **不能复用**：它每 zone 一把键、所有副本用同一个常量覆盖，`tools/merge_zone` 按值精确相等判定（`placement_ops.go:424-459`），改值会让现有的合服门禁判为缺失 [C]。
- `kafka:retry:instances:{topic}` 按实例登记重试租约（score 是 Redis TIME 算出的到期毫秒），不带能力信息。成员在正常停机归还收据后、或租约过期被存活实例搬空收据后，由同一段 Lua 摘除（`retry_ownership.go` 的 `retryReclaimScript`，最迟约 lease + lease/3）[C]。v0.2 写的「过期成员也不自动摘除」与代码相反。它仍不能直接作 `sql` 门禁：成员资格表示「最近持有重试租约」，不表示「正在落库」；活着的实例续租中断一次就会被别的实例回收并摘除，直到下次认领才重新登记；「登记表里所有成员都具备能力」这种判法会把缺席当成不存在，门禁 fail-open。
- **门禁契约**：无论新建一份按实例的能力登记，还是在这张 ZSET 旁边挂一个按 instanceID 的能力 hash、由同一组续租和回收脚本维护，都必须保证「实例的登记失效期间不消费、不落库」，与认领脚本「先续租再认领」是同一类不变量。实例只有看到登记表里的所有成员都具备能力时才真正按 `sql` 执行，否则自动按 `dual` 运行并告警。早于重试归属改造、不登记的旧二进制，靠下面的进入条件与发布门槛排除。
- 进入条件：class-2 / class-3 计数在观察窗内为 0；该 zone 的重试与死信队列清空过；缓存改用 §6.4 的值封装；G5 在 class-2 / class-3 上仍开着的残余（§5.6）已有处置；§5.2 游标后退后 class-3 越过游标的残余同样以「class-3 为 0」排除。
- 回退只能经过 `dual`。
- 立项判据见瓶颈文档 §6.3。

### 8.6 回退

| 回退 | 做法 |
|---|---|
| `dual` → `legacy` | 改配置滚动重启，随时安全。围栏列留着，`legacy` 不碰它们。切 `dual` 后若发现 class-3 进死信，先回退 `legacy` 恢复落库，再查来源 |
| 关 `EpochOverride` | 改配置滚动重启，随时安全 |
| 新 go/db → 旧 go/db | **先关 `EpochOverride`（或全部切 `legacy`），再换二进制**。旧代码忽略围栏列，Redis 守卫一直在维护。重新开启 `EpochOverride` 前要再满足 §8.3 的排空条件 |
| 新 C++ → 旧 C++ | `dual` / `legacy` 下安全 |
| DDL | 不回滚，不 DROP 列 |

`legacy` 运行一段时间后再切回 `dual`：`legacy` 期间的写不推进围栏，围栏落后于真实数据。围栏落后期间，这些行的保护退回到今天的水平，直到下一笔 `dual` 写把围栏推上来；§5.2 的放行不受影响（它依赖 §5.5 的 epoch 记录，不依赖围栏，§5.5 在 `legacy` 下也照做）。另外，`legacy` 期间行内围栏和缓存围栏都落后于已发出的序号，切回 `dual` 后冷加载的播种只剩墙钟（§7.4 条件 3）。

---

## 9. 运维流程

### 9.1 epoch 计数器倒退（B1）

**机制**：owner_epoch 只存在 Redis（`player:{id}:owner_epoch`，`go/shared/ownerepoch`），缺键时铸造脚本把缺键当 `"0"` 后 INCR（`owner_epoch.go`）。围栏把「已落库的最大 epoch」持久在 MySQL，`consumer:applied_epoch:*` 把它记在 Redis。如果计数器倒退，之后的存盘会在 **Redis 守卫或 SQL 处**被拒，取决于已落库 epoch（`applied_epoch`）是否同步倒退：

- `applied_epoch` 同步倒退：写通过 Redis 守卫，到 SQL 被围栏拒（`dual` 下归 `owner_epoch_regressed` 或 `stale_owner`，进死信）。
- `applied_epoch` 没有倒退：写在 Redis 守卫判 stale。今天 ack 丢弃、死信里没有载荷；`dual` 下由 §5.1 的探测进死信。v0.2 的处置只覆盖了前一半（§17 状态机 F3）。

**实际会触发的场景**（按 HEAD 与工作区取证）：

| 场景 | 计数器倒退 | `applied_epoch` 同步倒退 | 在哪里被拒 | 载荷去向（`dual`）| 依据 |
|---|---|---|---|---|---|
| `redis-cli FLUSHALL`，不清 MySQL | 清空 | 清空 | 基本不触发 | — | 账号目录也只在 Redis（`loginlogic.go`），清空后同一账号会重新建角、得到新的 player_id，旧行成为孤儿，不会再被写到 |
| 在线玩家所在的 Redis 被清 | — | — | 不触发 | — | C++ 带守卫的存盘在守卫键缺失时会按节点缓存的值补种（`redis_client.h`）|
| Redis 从旧备份恢复、AOF 丢尾、主从切换丢尾，**恢复时玩家离线** | 是 | 是（同一恢复点）| SQL（围栏）| 死信（`owner_epoch_regressed`）| 父键存在时 login 走快路径载入备份里的旧快照 S0；之后的写判 advance 通过 Redis 守卫，在 SQL 被行内更大的围栏拒。今天这些写会直接落库、静默覆盖 MySQL 里更新的 S1；`dual` 下 S1 被保住，造成丢数据的是 v0.2「归零后回灌」的处置（§17 事实 F1 序列 B）。HA 清单 `redis-ha.yaml` 目前没有接进部署脚本，单实例 AOF 走默认的 everysec |
| 同上，**恢复时玩家在线** | 是 | 先倒退，又被在途的高 epoch 写推回高位 | Redis 守卫 | 死信（§5.1 探测；今天 ack 丢弃）| C++ 先异步写 Redis、再无条件发 DBTask；go/db 收到高 epoch 判 advance 落库并把 `applied_epoch` 推回；C++ 的 CAS 被拒后销毁实体；玩家重进拿到倒退后的 epoch，之后的写在 Redis 守卫判 stale（§17 事实 F1 序列 A）|
| 手工删除个别 `player:{id}:owner_epoch` 键 | 是（从 0 起）| 否 | Redis 守卫 | 死信（§5.1 探测）| 玩家离线时 location 已删，同节点补种走不到，铸造得到 1 |
| G4a 拆分后核心实例单独回档（**未来**）| 是 | 否（`applied_epoch` 已迁到 redis-mapping）| Redis 守卫 | 死信（§5.1 探测）| `shared-redis-split-by-purpose.md` 的拆分第 2 批；今天两者在同一实例，不成立 |
| `k8s_zone_rollback.ps1` 带 `-ConfirmFlushSharedRedis` 的 FLUSHDB | 与 FLUSHALL 相同 | | | | 它清的是共享实例的 DB 0，所有 zone 的账号目录一起没。这本身就是事故 |

备份恢复、AOF 丢尾、主从切换丢尾都只丢末尾一段，计数器与 `applied_epoch` 在恢复点上一致；真正让 `applied_epoch` 高于计数器的，是恢复后仍在流动的高 epoch 在途写，所以「在 Redis 守卫被拒」这一行只在恢复时玩家在线时出现。

**处置：定真源、抬计数器、不归零、不回灌**（替换 v0.2 的「归零加回灌」与「排空前置」）：

- a. **定真源**：人工比较 Redis 父键（§3.3 的 `last_save_seq` 与业务字段）和各落点库行内的围栏与业务字段，确定以哪边为准，并记录被放弃一侧的进度（是否补偿由负责人定）。
- b. **踢下线**并等退出存盘落地，确认 `player:{id}:location` 不存在。在线时抬计数器等于废黜当前持有者：缓存值 CAS 失败、实体被销毁，这次存盘的内容丢失。
- c. **抬计数器**：用只升不降的 Lua 把计数器抬到 `C' = max(当前计数器, 各表各落点库的 fence_owner_epoch, 各表各 topic 世代的 applied_epoch) + 余量 K`。K 覆盖恢复点之后铸过、但还没落库的 epoch（仍在 Kafka 积压或重试队列里），否则抬升后新铸的 epoch 会与它们撞号。二选一写进 runbook：加余量 K（建议值待定），或者先排空该 zone 的积压与重试队列，再取最大值、不加余量。
- d. **缓存**：真源是 MySQL 时，删除 `PlayerAllData:{id}` 和各 `{MsgType}:{id}` 子表缓存，下次登录冷读。真源是父键时不删：进场后快照为空，第一次存盘必然写，而且每次都是两张子表整行，以 `C'+1` 自然落库。
- e. **死信**：两种情况都把该玩家倒退期间的死信载荷标记作废，不回灌（§9.4）。
- f. **不归零围栏，不动 `applied_epoch`。** 归零加回灌会用旧快照派生的数据覆盖更新的数据（事实 F1 序列 B）。若仍保留归零分支（例如 §9.3 `debug_import` 导入的围栏过大），必须同时 `DEL` 该玩家所有 topic 世代下的 `consumer:applied_epoch:{topic}:{id}:{表}`，并先排空；建议同样改用抬计数器。

**入口**：从 v0.2 的 `fence-reset` 改为 `epoch-raise`：按玩家执行，必须带确认开关，输出抬升前后的值以及取 max 的各个来源。它要同时读 MySQL（各落点库的围栏）和 Redis（计数器、`applied_epoch`、location），放在 go/db 的命令行工具里（`cmd/migrate` 加子命令或独立的 `cmd`，§14 第 5 条）。脚本和 runbook 只调用这个入口，不各自内嵌 Lua 或 SQL。它让计数器多了一个只升不降的运维写入方，与 `go/shared/ownerepoch/ownerepoch.go` 包注释「只经 scene_manager 的 INCR 前进」以及 `scene-owner-reentry-barrier.md` 里「唯一写入方」的表述冲突，落码时同步改这两处契约。

**seq 超前**：玩家行内的 `fence_save_seq` 超出 §7.2 上界时（坏数据、导入），抬计数器同样是处置：下一次进场铸出的新 epoch 在高位胜出，任何 seq 都不再起作用。

**死信增长**：倒退期间该玩家每次存盘产生两条整行载荷，进 noeviction 的共享 Redis，所以 `DbOwnerEpochRegressed` 是 critical，处置时限按小时计（增长上限的算法见 §9.4）。

**要同步的文档**：

- `AGENTS.md` §6.2：负责人已定「条件写落码时再加」（交接 §5 第 14 项）。内容：「只清 Redis、不清 MySQL 且会复用 player_id 时，先按条件写设计 §9.1 抬计数器」；data_stress `-fenced` 按 §11 第 8 步的规则每轮换 E 时不会触发 `stale_owner`，固定 E 又只清 Redis 时结论无效。另一会话对 §6.2 的修改（清全局库路由表）建议合成一次修改。
- `docs/ops/merge-zone-runbook.md` §9.2「Redis 整体丢失」的恢复步骤、`docs/design/zone_data_rollback.md` 的回档步骤：各加一步「定真源、抬计数器」。G4a 拆分稿的回档 runbook 引用本节流程。
- `go/shared/ownerepoch/ownerepoch.go` 包注释、`docs/design/scene-owner-reentry-barrier.md`：写入方契约（见上）。
- `deploy/k8s/owner-epoch-alerts.yaml` 里 `DbStaleOwnerWriteRejected` 的描述：注明倒退已由 `regressed` 单独区分，这条告警只剩僵尸与 K1（§10）。
- 交接里提到的「Redis HA 故障 runbook」在 `docs/ops/` 下不存在；`dev_tools.ps1` 也没有独立的清 Redis 命令 [C]。

**长期**：§5.1 的探测只是检测手段，计数器追平之后就失效。根治仍靠给 epoch 的铸造加持久下限（例如铸造时按库内 `fence_owner_epoch` 取 max）。交接写的是「并入 D1 映射落库」，但 D1 提案（`player-route-mapping-durability-proposal.md`）里没有对应条目 [C]，需要与那条线的负责会话确认是否接（§14 第 7 条）。

### 9.2 回档（PITR）

- MySQL 恢复到旧时间点后，围栏随数据一起回退。
- 回档流程如果同时清了 Redis，游标和已落库 epoch 也没了，比恢复点更新的在途任务会通过围栏落库，把回档结果冲掉。
- 所以回档 runbook 必须保证：重启 go/db 之前，该 zone 的 db_task 位点已经跳过恢复点之后的全部消息，`kafka:retry:*` 与死信里该 topic 的条目已清理，子表缓存已删除。
- 现有脚本和 runbook 都没有这一步 [C]：`k8s_zone_rollback.ps1` 的 Step 5 用 `--to-datetime` 重置位点，方向存疑；共享消费组下这一步通常直接失败。这与交接 §4.2 的「回档 Step 2」是同一件事，建议在那一项里一并改。

### 9.3 合服、搬库、导入导出

- `tools/merge_zone` 按列名拷行，围栏随行一起走；逐列相同的判据（`identicalRowCondition`）自动包含新列。不需要改代码。
- 搬库（relocate）之后落在旧库的写会重投到新库，在新库上按围栏裁决（`player-storage-placement.md` §6.3 的 LWW 语义不变，判据从游标换成围栏）。R2 等排序锁的逻辑在 `dual` 下不变。
- 缓存键没有新增，`playerCacheKeys` 不用改。
- `go/data_service/cmd/debug_import` 会把导出时的围栏值一起写回。从别的环境导入的行如果围栏比本环境的 epoch 大，之后的存盘会进死信；`fence_save_seq` 超出 §7.2 上界时，冷加载会触发 `Observe` 的超界处置（§7.2）。两种情况都按 §9.1 抬计数器处置，不建议归零。

### 9.4 死信里的条件写条目（`fence_stale_owner` / `owner_epoch_regressed` / `fence_legacy_after_seq`）

- 死信目前没有消费者也没有重放工具 [C]。这三类条目需要人看、需要能按原因和按玩家查看，依赖 L-04 第 2 批的死信工具；工具落地并演练是 Stage 1b 的前置（§8.3 第 2 条）。
- **不用手工 Redis 命令处置**（删除 v0.2 的「工具落地之前靠手工 Redis 命令」）：L-04 §3.8 与 `deploy/k8s/db-task-retry-alerts.yaml` 的告警描述都写明「不要用 redis-cli 把条目搬回 ready」，预算耗尽的条目 RetryCount 已满，下一拍认领时会被立刻送回死信，而 `redis-cli` 改不了二进制载荷里的 RetryCount。
- 条目格式：同一个 `kafka:dead:queue:{topic}` 列表，载荷外套带原因的信封（§8.3 第 3 条）。在第 2 批工具落地之前，`owner_epoch_regressed` 只能靠日志前缀与计数区分。
- **`owner_epoch_regressed` 只作证据、不回灌**：按 §9.1 的处置，真源确定之后倒退期的载荷一律作废。这也绕开了「RetryCount 已满、回灌后立刻再进死信」的问题。
- `fence_stale_owner`、`fence_legacy_after_seq` 的回灌，以及 L-04 第 2 批的回灌工具（重试次数清零后搬回 ready）本身，都会经过 §5.2，受 `EpochOverride` 门禁约束：门禁开启之前入死信的写，回灌前要人工核对。
- **增长上限**：速率约等于「受影响的在线玩家数 × 2 ÷ 存盘间隔 × 单行字节数」，单行字节取 `db_blob_row_bytes` 的 p99。noeviction 的共享 Redis 上会同时堆积三类唯一副本：条件写死信、落点库缺列时不耗预算的延后（§3.2）、L-04 的存储类延后。告警统一看 `DbTaskRetryQueueGrowing`、死信长度与 Redis `used_memory`；达到阈值时先止血（定位受影响玩家、按 §9.1 处置倒退，或回退 `legacy`），再清理。
- 可选（§14 第 17 条）：class-1 每笔都是整行快照，同一 (玩家, 表) 只保留围栏最大的一条，内存就按玩家数封顶、不再按存盘次数增长。代价是换数据结构，并同步改合服预检和死信长度的采样，由负责人取舍。

---

## 10. 可观测

**指标**（低基数，不带 player_id）：

| 指标 | label | 用途 |
|---|---|---|
| `db_fence_write_total` | `class` ∈ seq / legacy_epoch / legacy_zero / unfenced；`outcome` ∈ updated / inserted / unchanged / equal / older_seq / stale_owner / owner_epoch_regressed / legacy_after_seq / race / error | 围栏结局分布；`unfenced` 是非围栏表的写（§4.1），只记 updated / inserted / unchanged / error |
| `db_ordering_guard_total` | `outcome` ∈ apply / stale / stale_no_epoch / incomparable / epoch_override | 游标守卫分布；`epoch_override`、`stale_no_epoch` 是 §5.2 的新路径 |
| `db_fence_cache_publish_total` | `path` ∈ write / read；`result`：write 路径 ∈ written / not_owner，read 路径 ∈ written / exists | §6.2 的令牌校验与读路径 NX |
| `db_fence_body_mismatch_total` | — | body 里带了与 DBTask 不一致的围栏（§4.2）|
| `db_fence_rejected_bytes_total` | `outcome` ∈ equal / older_seq / stale_owner / owner_epoch_regressed / legacy_after_seq / epoch_override | 到达 SQL 后被拒的写所下发的行字节数（UPDATE 的 SET 带全部列）；`epoch_override` 另起一个 label，统计 §5.2 放行后被围栏拒掉的那部分。§11 第 9 步作为对比项（附录 M2）|

现有指标的变化：

- `db_owner_epoch_guard_total` 的 `outcome` 增加 `regressed`（§5.1），同步改 `metrics.go` 里该指标的 Help（注释要求 Help 与 `OwnerEpoch*` 常量一一对应）。
- `db_task_deferred_total` 的 `reason` 增加 `fence_race`（§4.4），加进 `deferReasons` 预建。
- `db_task_dead_letter_total` 的 `reason` 增加 `fence_stale_owner`、`fence_legacy_after_seq`、`owner_epoch_regressed`，加进 `deadLetterReasons` 预建，同步改 Help（§4.4）。
- `db_stale_owner_write_rejected_total` 保留；SQL 判出的 `stale_owner` 也计入它；`owner_epoch_regressed` 不计入，单独由 `regressed` 计数。
- 启动日志打印 `Fencing.Mode` 与 `Fencing.EpochOverride` 的生效值（§4.7）。

**告警**（追加到 `deploy/k8s/owner-epoch-alerts.yaml`，写法同现有两条）：

| 告警 | 条件 | 级别 |
|---|---|---|
| `DbFenceStaleOwner` | `outcome="stale_owner"` 有增长 | critical |
| `DbOwnerEpochRegressed` | `db_owner_epoch_guard_total{outcome="regressed"}` 或 `db_fence_write_total{outcome="owner_epoch_regressed"}` 大于 0（绝对值）| critical，处置时限按小时计（§9.1）|
| `DbFenceLegacyAfterSeq` | `outcome="legacy_after_seq"` 大于 0 | critical |
| `DbFenceRace` | `db_fence_write_total{outcome="race"}` 大于 0（绝对值）| warning |
| `DbFenceOlderSeqHigh` | `older_seq` 占 class-1 写的比例超过基线 | warning，阈值待压测确认 |
| `DbFenceBodyMismatch` | 大于 0 | warning |

- 该文件里记了一条经验：CounterVec 的序列从无到 1 时 `increase` 会漏第一次，所以关键告警用绝对值。新规则照此处理。
- 进死信的三类条目也会触发 L-04 的 `DbTaskDeadLetterAdded`（warning），与上面的 critical 告警同时响。二选一：接受重复告警；或者在 L-04 那条规则的表达式里排除 `fence_*` 与 `owner_epoch_regressed` 原因。建议后者，落码时与 L-04 的维护者对齐（§14 第 16 条）。
- `DbStaleOwnerWriteRejected` 的描述里注明：计数器倒退已由 `regressed` 单独区分，这条告警只剩僵尸与 K1。
- L-04 的 `DbTaskDeferredByStore` 只统计存储类原因，`DbTaskRetryQueueGrowing` 要求 ready 超过 1000，单条任务因 Race 空转触发不了它们，所以 `DbFenceRace` 必须单独加。

**日志**：`stale_owner` 沿用 `STALE-OWNER-WRITE` 前缀并写明来源是 SQL 围栏、行内围栏值；计数器倒退用 `OWNER-EPOCH-REGRESSED`（写明计数器值、已落库 epoch 或行内围栏、拒绝位置）；`legacy_after_seq` 用新前缀；非围栏表每个 MsgType 首次出现打一行 ERROR（§4.1）。

**前提**：仓库不部署 Prometheus，告警文件是手工 apply。HEAD 的 db ConfigMap 已配 `MetricsListenAddr: ":9160"`；ServiceMonitor / PodMonitor 与告警文件的 apply 仍待确认，生效与否以 §8.3 第 4 条的演练为准。

---

## 11. 测试与 Codex 验证

前提：Claude 没有编译、没有运行任何测试，下面全部由 Codex 执行。任一步失败就保留错误和日志摘要后停下，不连续重试、不改断言、不跳过测试。

**0. 前置**：L-04 第 1 批已合入。proto2mysql 的 `SaveConditional` 经 Codex 验证全绿后合入 main（不打 tag）；`go/db/go.mod` 的 require 与 replace 两侧都改成钉 main 上那个提交的伪版本（负责人已定，见文首）。

**1. proto 重新生成**（做法待负责人确认，§14 第 6 条）

- `go/build.bat` 第 3 行是 `pause`，而且它调用的 `build.ps1` 只跑 `goctl rpc protoc`、修 import、删副本，**不重新生成 `.pb.go`** [C]。真正的重新生成是 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run`（`proto/AGENTS.md` 列了三条命令）。AGENTS.md §4.1 只写了 `build.bat`，与实际不符。
- `proto-gen-run` 的默认配置开着 `enable_unity_client`，会写同级的客户端仓库（`proto_gen.yaml`），与 AGENTS.md §9 的客户端边界冲突。
- 通过标准：`git diff` 只出现下面 22 个已入库生成物里的新增字段，没有无关文件。加字段不产生新文件，CMakeLists 与 vcxproj 不用改 [C]。
  - `cpp/generated/proto/common/database/mysql_database_table.pb.{cc,h}`、`cpp/generated/proto/common/database/player_cache.pb.{cc,h}`、`cpp/generated/proto/db/db_task.pb.{cc,h}`（6 个）；
  - `go/proto/common/database/mysql_database_table.pb.go`、`go/proto/common/database/player_cache.pb.go`、`go/proto/db/db_task.pb.go`（3 个）；
  - `robot/vendor/proto/db/db_task.pb.go`（1 个）；
  - `generated/proto/_unified/proto/`、`generated/proto/db/proto/`、`generated/proto/login/proto/`、`generated/robot/proto/` 四处各自的 `common/database/mysql_database_table.proto`、`common/database/player_cache.proto`、`db/db_task.proto` 副本（12 个）。
- `cpp/generated/proto/db/db_task.grpc.pb.{cc,h}` 也已入库，但给 message 加字段不会改动它们；出现变化即视为异常停下。
- 生成器路径上还有三份 `mysql_database_table.sql`（`go/db/model/`、`go/login/model/`、`go/player_locator/`），是停在 2026-09-09 的陈旧快照（缺 2026-09-25 加入的 `settlement_ledger`）。如果重生刷新了它们，就还原、不随本批提交，或者经负责人同意另行提交；这一条交给 §14 第 6 条一并确认。

**2. Go 单测**（不依赖外部服务；工作目录 `go/db`）

```sh
go vet ./...
go test ./internal/kafka/... ./internal/logic/pkg/proto_sql/... ./internal/config/... ./internal/metrics/... ./internal/locker/... ./internal/storeerr/... -count=1
```

新增用例必须覆盖：

- 围栏结局归类全表（§4.4 每一行，含 `owner_epoch_regressed`，含不可能分支报错）；被拒结局以 `(outcome, nil)` 返回，`reportStoreResult` 收到 nil。
- 模式 × 类别的 `SaveOptions`（§4.3），含 class-2 的两步、`legacy` 不清围栏。若采纳 §4.3 的可选回填：class-2 第二步之后缓存里的 `fs` 等于第一步读到的行内 `fs`。
- 围栏表判定（§4.1）：非围栏表在 `legacy` 和 `dual` 下都照常 `Save` 落库并计 `unfenced`；非围栏表不触发 `epoch_override`；启动断言列表里的表缺围栏字段时拒启。
- 错误去向（§4.4）：`ErrConditionalSaveRace` 延后、耗预算、`deferReason=fence_race`；`SaveConditional` 返回的驱动错误仍带 `storeerr` 标记（1205 / 1213 / 9007 / 8002 归 `store_busy`、不耗预算）；条件写哨兵不带标记。
- `handleTask` 级别（miniredis）：`older_seq` 先复核、复核不过则延后；`stale_owner` / `owner_epoch_regressed` / `legacy_after_seq` 以对应 label 进死信，且不写缓存、不推进游标；`equal` 补发缓存。
- §5.1 计数器探测：`applied_epoch=7`、计数器缺失或为 1、任务 epoch=1 → 进死信、不 ack、计 `regressed`；`applied_epoch=8`、计数器=8、任务 epoch=7 → ack（行为同今天）；探测读失败 → 延后且耗预算；`legacy` 不探测。
- §5.2：表里每一行；`EpochOverride=false` 或 `legacy` 下不放开；stale + first 丢弃并计 `stale_no_epoch`；**两笔同 epoch 的重试排在老主迟到写之后，最终落库的是较新的一笔**；override 之后老 epoch 的重投被 epoch 挡住；override 路径上 `equal` 的游标改写；推进脚本失败后能自愈（延后 → 重试 → `equal` → 脚本成功）。
- §5.5 推进脚本：令牌不符返回 0 → 延后（`lease_lost`）；正常路径游标前进、epoch 取 max；Redis 错误 → 延后，不只记日志。**混跑对照**（用测试替身模拟旧二进制的两步推进并让 epoch 那一步失败）：旧实例 + 放行 → 复现回退，证明门禁必要；新二进制 `legacy` + `dual` → 不回退。
- §5.3 批内合并按 save_seq；`legacy` 下照旧按 offset。
- 缓存脚本：持锁写入；失锁返回 0、包 `errOrderingLeaseLost` 延后且按 §5.4 的默认耗预算（若负责人选豁免，改为不耗预算）；读路径 NX；`db_fence_cache_publish_total{path,result}` 计数正确。
- 配置（§4.7）：用 `conf.LoadFromYamlBytes` 分别加载裸写的 `off`、带引号的 `"off"`、`legacy`、`dual`、`sql`、空值，把行为钉住（裸 `off` 类型不匹配报错；空值补成 `legacy`；`sql` 与 `"off"` 拒启）；`EpochOverride` 缺省 false。
- 指标：启动后新增的 `deferReason` 与死信 reason 序列都以 0 存在。

需要的测试设施：`go/db/internal/dbtest/fakedriver.go` 的影响行数写死为 0，要改成可配；现在没有任何测试调用真实的 `handleDBWriteOp`，本批补上。

**3. 真库集成**（新增，环境变量门控；工作目录 `go/db`）

- 用 **go/db 业务池自己的连接配置**（`newServiceMysqlConfig`，含 L-04 的超时与会话级锁等待）连库。proto2mysql 的真库用例强制开了 `InterpolateParams`，走的是客户端内插；go/db 不开，走服务端预处理。库的用例没有覆盖 go/db 实际的参数绑定路径 [C]。
- 场景：两个连接池、20 路并发乱序写同一玩家，最终值是最大围栏；同版本重放得到 `equal`；并发首写；`legacy` 写过之后围栏不变；class-2 两步；class-3 在有围栏的行上被拒；非围栏表在两种模式下照常落库；`db_fence_rejected_bytes_total` 按 outcome 计数。
- 后端：MySQL 8.4 与 TiDB 8.5 各一遍。Codex 任务单已记负责人同意拉这两个镜像；任务单之外的下载仍先请示。
- 通过标准：全绿，零 SKIP（含子用例：按 `--- SKIP` 计数为 0）。

**4. 迁移**（工作目录 `go/db`）

```sh
go run ./cmd/migrate -f etc/db.yaml -command plan
go run ./cmd/migrate -f etc/db.yaml -command up
```

- 库清单按 §8.1 第 2 步穷举；每个落点库加 `-storage-id <id>` 各跑一次。需要 `DB_ALLOWED_DATABASES` 放行目标库。
- 通过标准：每个库 plan 只有 4 条 `ADD COLUMN fence_*`；up 之后 `SHOW COLUMNS FROM player_database LIKE 'fence_%'` 与 `player_database_1` 各返回 2 行。

**5. merge_zone**（工作目录 `tools/merge_zone`）：`go test ./... -count=1`，原有用例保持绿。

**6. C++**

- 先 `pwsh -File tools/scripts/dev_tools.ps1 -Command cpp-node-stop`。
- 仓库根：`msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`。必须 `/m:1` 串行；生成的 proto 库变了，要全量编译。`game.sln` 的 Debug|x64 配置已包含 `cross_zone_test`、`bag_test`、`currency_test`（排在 scene 等库之后），不需要也不建议再单独编 vcxproj。
- 全量运行：`pwsh tools/scripts/run_cpp_tests.ps1 -Filter cross_zone_test`、`-Filter bag_test`、`-Filter currency_test` 各一次。脚本会先同步 `zlibd.dll` / `rdkafka*.dll` 等运行时 DLL（直接运行 exe 可能因缺 DLL 以 0xC0000135 静默退出），并解析 Running / PASSED / FAILED。
- 过滤运行：`build\cpp\tests\cross_zone_test.exe --gtest_filter=<§7.5 列出的用例名，逐个写出，以冒号分隔>`。
- 新增的 `save_seq.h` 是头文件：`cpp/libs/services/scene/CMakeLists.txt` 只登记 `.cpp`，不用改；`scene.vcxproj`（ClInclude）与 `scene.vcxproj.filters` 要登记。测试工程没有 CMakeLists。
- 通过标准：
  - 编译零错误；
  - 三个工程全量运行时 PASSED 等于 Running，`cross_zone_test` 的原有用例数不减（现 166 个）；
  - 过滤运行的「Running N tests」中 N 不小于 §7.5 新增用例数，且逐条列出的用例名都出现在 PASSED 里（gtest 的过滤器一条都没匹配到时退出码仍为 0，所以不能只看退出码）；
  - 可选：运行前核对 exe 时间戳晚于本次编译开始时间。

**7. 本地端到端**

- 按 AGENTS.md §6.2 清环境后，`Fencing.Mode=dual`、`EpochOverride=true`（本地只有新二进制）起服，跑 robot 冒烟。
- 本地 `scene_manager_service.yaml` 开着 `AllowUnsafeCrossNodeHandoff`（dev 旁路，§7.1）；单机下两个进程读同一个墙钟，不会因此出 `older_seq`。用 `false` 再跑一遍是单独的验收项，不是本步的硬性要求（§7.4）。
- 通过标准：`db_fence_write_total{class="seq"}` 的 updated / inserted 大于 0；`stale_owner`、`owner_epoch_regressed`、`legacy_after_seq` 为 0；同一玩家两张表的 `fence_save_seq` 相同且非 0；`fence_owner_epoch` 等于该玩家的 owner_epoch。

**8. 一致性与混沌**

- `go/db/cmd/data_stress` 加 `-fenced` 开关，定义：
  - `OwnerEpoch` 每轮取一个只增不减的值，例如本轮开始时的 Unix 秒，允许用参数覆盖；`SaveSeq = seq`（每玩家 1..N，不需要与生产公式一致：合成号不会被 scene 加载，§7.2）；
  - 额外写一个期望键 `verify:expected_fence:{msgType}:{pid} = E:N`；
  - 每轮必须换 E 或换 `-player-start`，禁止跨轮复用相同或更小的 E。固定 E 又只清 Redis 时，第二轮的写全部判 `older_seq` / `equal`，verifier 读到的仍是第一轮的值，结论无效。
  - data_stress 不铸造计数器（键缺失时按 0 处理），E 必须单调递增，否则 `dual` 下旧 E 的写会被 §5.1 的探测判成倒退、误报 `DbOwnerEpochRegressed`；或者先为测试号段铸好计数器。
- `cmd/verifier` 增加围栏断言，分两种轮次：
  - 全 `dual` 轮：探针 == N，且围栏 == (E_run, N)；
  - `dual` 与旧版混跑轮：探针 == N，且围栏 ≤ (E_run, N)（旧版副本不写围栏）；同时要求至少有一部分玩家的围栏 == (E_run, N)，证明 `dual` 副本真的参与了裁决。
- go/db 开两个副本，中途强杀一个并注入重试，之后跑 `cmd/verifier`。
- 再用一个 `dual` 副本与一个旧版副本混跑一轮，**`EpochOverride=false`**（旧二进制在场，§8.3）。
- 切 `dual` 之后，data_stress 必须带 `-fenced`，或者只用围栏仍为 `(0,0)` 的独立号段；否则 class-3 写会判 `legacy_after_seq` 进死信。
- 通过标准：收敛到最大序号，0 处不一致，围栏断言按轮次通过。

**9. 压测**：按 AGENTS.md §6 全流程，与 R20 基线（交接 §4.1 第 4 项）对比。关注 `dataloader` 等 db 回调的耗时没有回归、`older_seq` 的比例、`db_fence_rejected_bytes_total`（被拒写白白下发的字节数）。没有对比表不宣称性能结论。

---

## 12. 改动清单与规模

| 批 | 文件 | 说明 |
|---|---|---|
| A proto 与文档清单 | `proto/db/db_task.proto`、`proto/common/database/mysql_database_table.proto`、`proto/common/database/player_cache.proto`；§3.4 的 6 个文档 | 另有生成物 22 个（§11 第 1 步逐一列出）。A 批自身 3 + 6 + 22 = 31 个 |
| B go/db | `internal/kafka/key_ordered_consumer.go`、`placement_route.go`、`store_failure.go`（`deferReason` 新分支）、`internal/locker/locker.go`、`internal/config/config.go`、`etc/db.yaml`、`internal/metrics/metrics.go`、`internal/dbtest/fakedriver.go`、`cmd/data_stress/main.go`、`cmd/verifier/main.go`（围栏断言）、`cmd/migrate/main.go` 或独立 `cmd`（`epoch-raise`，§9.1）、`go.mod` / `go.sum`；测试 4~5 个 | KOC 是多条修复线共用的文件，L-04 先合入，本批在其上改；合入顺序见瓶颈文档 §5.2 |
| C C++ | `player_lifecycle.cpp`、`save_seq.h`（新）、`core/system/id_generator.h`（发号器实例）、`last_persisted_snapshot_comp.h`、`player_database_loader.cpp` 或加载处、`scene.vcxproj` 与 `.filters`、`cross_zone_test.cpp` | — |
| D 部署与运维 | `tools/scripts/k8s_deploy.ps1`（db ConfigMap）与契约测试、`deploy/k8s/owner-epoch-alerts.yaml`（新告警与 `DbStaleOwnerWriteRejected` 描述）、`deploy/k8s/db-task-retry-alerts.yaml`（若排除 `fence_*` 原因）、回档与 Redis 丢失的两份 runbook | `k8s_deploy.ps1` 有另一会话在改 |
| E 契约与文档同步 | `go/shared/ownerepoch/ownerepoch.go` 包注释、`scene-owner-reentry-barrier.md`（计数器写入方，§9.1）、瓶颈文档（G1 / G3 / G5 覆盖口径与 §6.5 分片约束）、`shared-redis-split-by-purpose.md`（探测依赖）、`AGENTS.md` §6.2（负责人已定落码时加）| 随对应批次提交 |

规模：生成物 22 个，手写约 24 个，合计约 46 个文件；A 批自身 31 个，已超过 AGENTS.md §10.2 的 30 文件门槛。v0.3 新增的改动点（E 行、`epoch-raise`、verifier、`id_generator.h`）会让手写数再多几个，落码前按实际清单重数。建议按 A → B → C → D 拆 PR，A 与 B 之间隔着 DDL；无论如何拆都要授权（字段号上线后不可回收，每个库要做 DDL，跨 C++）。用户已于 2026-10-14 授权实现，A 批超门槛仍要在交付说明里单独报告。

---

## 13. Java 版（AGENTS.md §12）

**结论：不适用（Java 内部），另有一处待核。** 依据是对 Java 仓库 `D:\luyuan\xuanming-server-mmo`（`d942f74`）的只读核对：

- 带状态的写只由持有该 epoch 的 scene 进程自己执行，中间没有 go/db 这样的独立执行实例，也没有 Kafka 或 Redis 队列承载存盘。围栏就是 UPDATE 的 WHERE（`PlayerMapper.updateStateHeld`：`WHERE player_id = ? AND owner_epoch = ? AND owner_released = 0`），与数据写在同一个事务里。本设计要挡的「另一个执行实例拿着旧快照恢复」在 Java 版没有对应物。
- 交接里「同步写库」的说法要订正：Java 是进程内有界队列加重试的异步执行，快照到 SQL 之间有排队延迟；跨 epoch 靠围栏拒，同 epoch 靠「每个实例对象至多一笔在途的在线存盘」。
- **待核**：同节点收到同一 `(玩家, epoch)` 的再次进场时会换新实例并沿用旧内存（`SceneWorld.java:1403-1424`）。旧实例在途的在线存盘可能晚于新实例的存盘提交，等值围栏放行，库回到旧值。触发条件（gate / login 何时重发进场）没有核实。这是 Java 版自己的缺口，是否修由负责人定。
- 两版的差别：本仓库是单调高水位（异步通道必须接受老主在新 epoch 铸出之后才到的最后一笔），Java 是对权威列做等值比较，没有序号列。
- 新 proto 字段经 Java 仓库的 `tools/ContractSync.java` 进入 `game-proto`（它原样同步 `proto/**`，只排除 `proto/etcd/`）。Java 全仓没有引用 `DBTask` 或这两张表的 message，加字段兼容，不改 Java 代码。三个 proto 都不是客户端可见契约（`proto/README.md:27-31`）。
- PARITY.md 第 103 行记着 proto2mysql v0.2.0 与 `game-pbmysql` 逐方法对齐；`SaveConditional` 合入 proto2mysql 的 main 之后（负责人已定不打 tag），那一行要补状态。玩家存盘不经 `PbMysql`。
- v0.3 的改动（计数器探测、原子推进、`EpochOverride`、`epoch-raise`）都在 Go 内部与运维流程，不涉及客户端契约，结论不变。

**登记**：功能落码后在 Java 仓库 `PARITY.md` 追加一行，状态「不适用（Java 内部）」，备注写上面的差别与待核项。设计阶段不登记。

---

## 14. 待拍板与未核实

**待拍板**：

| # | 事项 | 建议 |
|---|---|---|
| 1 | DBTask 9 号给 `save_seq` 还是 `reply_instance`（交接 §5 第 13 项）| **已定**：9 给 `save_seq`，10 给 `reply_instance`（请负责人通知全局单点那个会话）|
| 2 | 是否改 AGENTS.md §6.2（交接 §5 第 14 项）| **已定**：条件写落码时再加；内容按 §9.1（抬计数器，不是归零），与另一会话对 §6.2 的修改合成一次 |
| 3 | Redis 分片的 hash tag（交接 §5 第 16 项）| **已定**：以按 player_id 分片为准。本批不受影响；瓶颈文档 §6.5 修订时要把 §6.4 的约束（令牌脚本两键同 tag）与 §5.5 的约束（排序锁、游标、已落库 epoch 三键同 tag）算进去 |
| 4 | 缺省模式与切换方式（§8.2、§8.3）| 缺省 `legacy`，按 1a → 1c → 1b 逐 zone 切 `dual`，再按门禁开 `EpochOverride` |
| 5 | 计数器抬升入口放哪（§9.1）| `epoch-raise`：`cmd/migrate` 加子命令或独立 `cmd`，需要同时访问 MySQL 与 Redis；同步改 ownerepoch 的写入方契约 |
| 6 | proto 重新生成的做法（§11 第 1 步）| 请负责人说明平时怎么跑 `proto-gen-run` 而不写客户端仓库；AGENTS.md §4.1 的描述是否订正；三份陈旧的 `mysql_database_table.sql` 是否随本批刷新 |
| 7 | epoch 铸造的持久下限由谁做（§9.1）| 与 D1 那条线确认；D1 提案里目前没有 |
| 8 | 是否让 scene_manager 在进程换代时必定铸造 epoch（§7.4）| 先核实那条时序是否可达；可达时它是多主机环境切 `dual` 的前置。class-2 第二步被放行路径利用的条件（同 epoch 内生产者版本变化，§5.2）也跟随本条 |
| 9 | Stage 1b 的前置（§8.3）| L-04 第 1 批先合入（**已定**，合入顺序 L-04 在前）；**1b 还要等 L-04 第 2 批死信工具落地并演练**，第 2 批目前未授权，需要负责人授权 |
| 10 | Java 版同 epoch 重复进场的待核项（§13）| 另开任务核实 |
| 11 | `STRESS_TEST_PROBE=1` 时快路径是否失效（§7.3）| 另开任务核实，影响压测口径 |
| 12 | `equal` 的缓存处理（§4.4，CPP-3）| 方案 1（v0.3 的写法）：维持补发，跨进程撞号时缓存与 MySQL 可能不同，由 verifier 事后核对；方案 2：令牌脚本里删除子表缓存键，读路径从 MySQL 重建，撞号与否都正确，代价是每次 `equal` 多一次冷读，verifier 的缓存核对要容忍缺键。请负责人选 |
| 13 | `Observe` 超界玩家的处置（§7.2，CPP-2）| 方案 1：不采纳，该玩家本 epoch 内被判 `older_seq`，靠告警与 §9.1 处置；方案 2：个人下限挂实体，不丢盘。建议方案 2；容差取值也待定 |
| 14 | 失租是否豁免重试预算（§5.4）| v0.3 默认不豁免（避免无界空转；MySQL 已有该版本）。代价是连续 3 次失租时进死信、缓存停到 TTL。核实意见有分歧，请负责人选；选豁免就在 `retryBudgetExempt` 加分支 |
| 15 | 冷登录与在途写的竞态（§6.2 残余窗口 3）| 不在本批。修法方向：go/db 按 key 维护「有待落写」标记，读任务看到就回失败或延后（或 login 冷组装前先查）。批次与归属待定 |
| 16 | `DbTaskDeadLetterAdded` 与条件写 critical 告警重复（§10）| 建议在 L-04 那条规则里排除 `fence_*` 与 `owner_epoch_regressed`，需与 L-04 维护者对齐 |
| 17 | 死信按 (玩家, 表) 只保留围栏最大的一条（§9.4）| 可选；代价是换数据结构并同步改合服预检与死信长度采样 |
| 18 | class-2 第二步回填缓存围栏（§4.3）| 可选，默认不做 |
| 19 | 计数器探测在 G4a 拆分后的 Redis 客户端（§5.1）| 拆分第 2 批之前要给 go/db 单独配一个指向核心实例的客户端；请在拆分稿登记这条依赖，并让拆分稿的回档 runbook 引用 §9.1 |
| 20 | `epoch-raise` 的余量 K 与「先排空」二选一（§9.1 第 c 步）| 待运维确认 |

**未核实**：

- Drift 实际输出的 DDL 文本（只按源码推断）。
- go-sql-driver v1.9.3 的 `NewConfig()` 默认值；uint64 参数在服务端预处理路径上的绑定（§11 第 3 步用真库用例覆盖）。
- TiDB 上 autocommit 的乐观事务对 `SaveConditional` 的影响：9007 / 8002 / 1213 已由 L-04 归入 `store_busy`（写不耗预算，延后重试）；仍建议玩家库所在集群用悲观事务。
- scene 主机的时钟同步方式、历史最大回拨量与主机间偏差。
- `data_service` 回档写父键的路径是否只写 Redis。
- librdkafka 幂等生产者在本仓库版本下的保序行为（子模块未检出）。
- 「同一 node_id 的新进程走不铸造分支」在真实部署下是否可达（§7.4 条件 2）。
- `AllowUnsafeCrossNodeHandoff=false` 下的标记链路：yaml 注明尚未编译、未跑过测试。
- 发号器实例放在 `tlsIdGeneratorManager` 上、`tlsEcs.Clear()` 不触及它（§7.2，按阅读推断）。
- go-zero 对裸 `off` 的解析行为只读了 module cache 里的源码，没有运行（§4.7）。
- `SaveConditional` 本身尚未编译、未运行；本设计的全部改动未落码、未编译、未测试。

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
| `dual` 下对所有带 epoch 的写放开游标（§5.2）| v0.2 的理由是「class-2 没有可比的版本」；v0.3 起放行的安全性来自 §5.5 的原子推进，class-2 的 advance 放行在它之下同样成立。本批仍只开 class-1，理由改为控制范围 |
| `stale_owner` 直接 ack 丢弃 | epoch 计数器倒退时会把全部存盘静默丢掉 |
| 复用 `db:capability:zone` 做 `sql` 模式门禁 | 每 zone 一把键，merge_zone 按精确相等判定 |
| §5.2 把 stale + match 也交给 SQL（状态机 F1 评审改法 b）| 依赖围栏精确（必须全 zone 都是 `dual`），并且会把重平衡时成批的 stale + match 重投都送进 MySQL。将来若要做，只对 `fromRetry` 的任务开放 |
| 只把 `markAppliedOwnerEpoch` 失败改成延后（事实 F5 评审改法）| 失败时游标已经 SET 到本任务的 seq，延后之后的重试会被游标判 stale 丢掉，epoch 永远补不上。改用 §5.5 的原子脚本（或「先 epoch 后游标」的过渡写法）|
| `dual` 下 Redis 守卫判 stale 一律进死信（状态机 F3 评审改法 1）| 真僵尸与迟到的退出存盘（K1）是这道守卫的设计对象，全部进死信会把整行载荷堆进 noeviction 的 Redis，告警成为噪声，而且仍区分不出倒退。改用计数器探测（§5.1）|
| 计数器倒退后「围栏归零 + 回灌死信」（v0.2 §9.1 处置 2、3）| 恢复时玩家离线的情形下，归零后回灌会用备份快照派生的数据覆盖 MySQL 里更新的数据；回灌的条目 RetryCount 已满会立刻再进死信。改为定真源、抬计数器、不归零、不回灌 |
| `equal` 不发布缓存（CPP-3 评审改法二）| 失租恢复依赖 `equal` 补发缓存；SQL 已落而缓存没发成时缓存键不是缺失而是留着上一版的值（TTL 3600 秒），读路径的 NX 补不了，违反 F-4 |
| 发号低位放 node_id 防跨进程撞号（CPP-3 评审改法一）| 计数模式下 `last + 1` 会改掉低位的进程身份；C++ 的 node_id 是 17 位，毫秒 41 位加计数 10 位后只剩 13 位；§7.4 的场景恰恰是同一个 node_id 的两代进程，用 node_id 区分不开。若确实要降低撞号，低位放启动时取的随机数、增量按计数字段步进，也只能把概率降到 2^-k，不是保证 |
| data_stress `-fenced` 的 SaveSeq 用与生产相同的公式（CPP-2 评审改法）| 合成 player_id 不经 scene 加载，进不了 `Observe`；每玩家 1..N 就能满足「收敛到最大序号」（§11 第 8 步）|
| 读路径 NX 失败后改回缓存现值（状态机 F4 评审改法 a）| SQL 已落而缓存未发布的窗口里，SELECT 读到 v3、缓存是 v2，回 v2 等于新增一条回档路径。若一定要收窄那个毫秒级交错，只能在 NX 失败后 GET 缓存、按围栏字典序取较大者，相等或解析失败时取 SELECT（缓存围栏只是下限，§6.3）；收益太小，不做 |
| 读路径「先 GET、解析失败再覆盖」修坏值（状态机 F6 评审改法）| 写成 GET → 判断 → SET 三步时，其间写路径发布的新值会被读到的旧行覆盖，等于把 G4 放回来；「长度为 0」也不算解析失败（空字节能解出空消息）。可选的自愈写法是 CAS：NX 失败后 go/db 尝试解析现值，解析失败才 EVAL「`GET(k) == 观察到的字节` 时 SET」，默认不做 |
| 同一任务连续 N 次 Race 后转普通失败（状态机 F8 评审改法）| 需要跨重试持久的计数，而豁免路径会把 RetryCount 减回去；新增计数就要改载荷或 proto，L-04 已否掉改载荷格式。改为 Race 直接耗预算（§4.4）|
| 条件写死信另开独立键（事实 F2 讨论）| 合服预检 `tools/merge_zone/preflight.go` 和 `db_retry_queue_length{queue="dead"}` 都只看 `kafka:dead:queue:{topic}`，独立键会绕过两者。改为同一列表、外套带原因的信封（§8.3）|
| 两轮压测之间清 MySQL 或归零围栏（事实 F4 评审改法 3）| 可作备选，但不如每轮换 E 简单（§11 第 8 步）|
| 单独编 `cross_zone_test.vcxproj`（事实 F3 评审改法 1）| `game.sln` 的 Debug / x64 构建配置已包含它，排在所有库之后；评审「旧产物上假绿」的前提不成立 |
| 模式名沿用 `off`，靠纪律写成 `"off"`（事实 F6）| go-zero 的 YAML 1.1 解析会把裸 `off` 变成 bool，只能靠纪律防住；改名 `legacy` 由机器防住（§4.7）|

---

## 16. 交接要求对照

交接 §4.1「条件写设计必须覆盖」逐条对应的章节：

| 要求 | 章节 |
|---|---|
| DBTask 加 `save_seq`，字段号冲突 | §3.1、§14 第 1 条 |
| 两张玩家表加围栏列、声明在末尾、确认未被占用 | §3.2 |
| 帮会二期放行清单 | §3.4 |
| B1：stale_owner 进死信并告警；只清 Redis 的流程；AGENTS.md；长期方案 | §4.4、§5.1（计数器探测）、§9.1（定真源、抬计数器）、§10、§14 第 2、7 条 |
| B2：save_seq 初值不能只靠墙钟 | §7 |
| B3：class-3 只在围栏为 `(0,0)` 时写 | §4.3；切 `dual` 的 class-3 前置见 §8.3 第 5 条 |
| 判 older 之后先复核再 ack | §4.4、§4.5 |
| `off`（v0.3 改名 `legacy`）模式不能调 Save | §4.3、§4.7 |
| Stage 2 要有机器门禁 | §8.5 |
| PITR 回档后先清积压 | §9.2 |
| go/db 映射与 `SaveConditional` 语义；先判 epoch 再判 seq（G3）| §4.3、§4.4、§5.2、§5.5（G3 部分修复，成立条件见 §1.2 F-1、F-5）|
| 围栏缓存：不另开键、优先值封装、与分片方案的关系 | §6 |
| MySQL 读写超时 | §4.6（连接级超时已由 L-04 落地，本设计只补 ctx）|
| 构建：proto 重生、C++ 串行、工程文件登记、不手改生成物 | §11、§12 |
| Java | §13 |
| 注释与文档同步（附录「复核要求的修正」中的注释同步清单）| 本批订正 `placement_route.go` 里「整行 REPLACE，重复落地幂等」那句注释（它本来就不准：Save 是 UPDATE → INSERT），改成「`dual` 下按围栏条件写裁决，重复落地得到 equal / older_seq」；`relocate.go` 文件头「按 per-key 游标全序落到 T」补「`dual` 下另有围栏裁决」。claim drain、`subShardIndex`、`placement_route.go`「选库结果必须在持锁期间用掉」、relocate R2、merge-zone-runbook R2 段在 `dual` 下仍成立（排序锁保留），标为随 `sql` 模式那批改 |
| 被拒写的字节数（附录 M2）| §10 `db_fence_rejected_bytes_total{outcome}`；§11 第 9 步列为对比项 |
| G5：缺键放行 | §5.6。class-1 由围栏兜住；class-2 / class-3 在 `dual` 下仍开着；残余写进 §8.5。瓶颈文档把条件写登记为覆盖「G1 / G3 / G5」，要同步订正为「G1；G3 部分；G5 只关 class-1」 |

---

## 17. 修订历史

- v0.1（2026-10-13）：初稿。
- v0.2（2026-10-13）：按附录复核意见修订。文首当时写的「已过一轮对抗评审」不准确：评审结论在 v0.2 里没有并入。
- v0.3（2026-10-14）：并入对抗评审结论。三组评审（条件写-状态机正确性 9 条、条件写-事实与完整性 10 条、条件写-C++ 发号 5 条）共 24 条，逐条按工作区代码（含未提交的 L-04 第 1 批）核实后处置，见下表；同时并入负责人 2026-10-14 已定的事项（DBTask 9 / 10 号、Redis 按 player_id 分片、AGENTS.md §6.2 落码时再加、proto2mysql 只合 main 用伪版本钉提交）与实现归属。主要变化：模式 `off` 改名 `legacy`；新增 `Fencing.EpochOverride` 与 §5.5 原子推进、§5.6 G5 处置、§6.5；§5.1 计数器探测；§9.1 处置改为抬计数器；分期改为 1a → 1c → 1b。**仍未落码、未编译、未测试。**

### 17.1 评审意见处置表

核实结论与严重度以核实结果为准（不是原评审的定级）。「不采纳」的理由同时登记在 §15。

| 意见编号 | 核实结论 | 严重度 | v0.3 的处理 |
|---|---|---|---|
| 状态机 F1（override 后游标不动，同 epoch 较新重试被丢）| 成立 | 重要 | §5.2 采用评审改法 (a) 并补全：stale + first 照旧丢弃、计 `stale_no_epoch`；override 落库后用 §5.5 的脚本原子改写游标（允许后退）并推进 epoch，脚本失败即延后；写明后退安全的理由与 class-3 残余；第 2 行理由改写。§4.4 游标列分正常 / override 两路；§5.4 失租后 `equal` 走同一脚本；§11 第 2 步加四个用例；§1.2 F-1、§0.1 第 5 行改为「部分修复」并写成立条件。改法 (b) 不采纳（§15）|
| 状态机 F2（游标与 epoch 非原子推进，放行越过落后的围栏）| 成立 | 重要 | 新增 §5.5：新二进制不分模式用一条脚本核令牌、推进游标、epoch 取 max，失败即延后，写明不变量。§5.2 删除「class-1 围栏精确」的论证，放行安全性改由 §5.5 支撑。新增子开关 `Fencing.EpochOverride`（§2、§4.7），开启门禁写在 §8.3（无旧版 go/db、旧实例下线后重试排空过一次）。§8.4 第 2、5 行与 §8.6 回退改写；§9.4 回灌受同一门禁；§11 第 2 步加混跑对照，第 8 步混跑轮 `EpochOverride=false`；§1.2 F-5 补成立条件。三键同 hash tag 并入 §14 第 3 条 |
| 状态机 F3（计数器倒退而 applied_epoch 未倒退时，写在 Redis 守卫被 ack 丢弃）| 成立 | 重要 | 与事实 F1 合成一处：§5.1 `dual` 下计数器探测（判据与正确性理由、读失败延后耗预算、`legacy` 不探测、对 G4a 拆分的依赖）；§4.4 SQL 处同样探测，新增结局 `owner_epoch_regressed`；§9.1 机制段与触发表改写（加「是否同步倒退 / 在哪被拒 / 载荷去向」三列，拆行，加 G4a 行）；§10 加 `regressed`、`DbOwnerEpochRegressed`、三个死信 reason；§0 第 3 条、§1.2 F-3 按两种拒绝位置分写；§11 加三条用例。评审改法 1 不采纳（§15）。修正评审两处：丢弃有日志、计数与 warning 告警，并非无声；「两个 Redis 不是同一实例」今天不成立，G4a 拆分后成立 |
| 状态机 F4（读路径 NX 后回给 login 的仍是 SELECT 的行）| 部分成立 | 次要 | §6.1 G4 措辞改为「缓存被旧值覆盖」，删去暗示本节解决回档固化的表述；§6.2 加「NX 只决定缓存键，`TaskResult.Data` 是 SELECT 的行」；§6.2 残余窗口 3 与 §1.3 登记「冷登录与在途写的竞态」（现象、入口、L-04 之后的窗口）；§14 第 15 条登记修法方向（按 key 的待落写标记，不是 ①）；§10 publish 计数加 `path`；§6.5 汇总。改法 (a) 不采纳（会新增回档路径，§15）。修正评审：父键缺失且 W 在 ready 现实中只能来自手工删父键；「G4 没有关掉」说重了 |
| 状态机 F5（切 `dual` 改变 class-3 语义却没有门禁）| 成立（触发面比评审窄）| 次要 | §8.3 第 5 条加 class-3 前置（Stage 1a 观察窗内 `legacy_zero` 与 `owner_epoch_unknown` 不增长，gate / scene_manager 为 CZ-4 之后版本，非 0 先归因、不写成硬性恒 0）；§8.2 加观察项；§8.4 矩阵加一行；§8.6 写明先回退 `legacy`；§2 补 class-3 的实际来源；§11 第 8 步要求切 `dual` 后 data_stress 带 `-fenced` 或用独立号段 |
| 状态机 F6（缓存坏值时 NX 修不了）| 成立 | 次要 | §6.2 读路径第 1 条改写（缺失、GET 出错、解析失败都会发读任务；`TriggerPlayerDataPreload` 无调用方）；残余窗口 2 写明 NX 不修坏值及代价；§6.4 补值封装的上线顺序；§10 read 路径计 `exists`。评审给的 GET → 判断 → SET 改法不采纳，CAS 自愈记为可选（§15）。修正评审：「每次登录都重复」偏重，login 随即写回父键 |
| 状态机 F7（非围栏表上 `PreserveFields` / `FenceFields` 必然失败）| 成立 | 次要 | §4.1 新增围栏表判定（启动时按描述符判定并缓存、两张玩家表缺字段即拒启、非围栏表照旧 `Save` 并计 `unfenced`）；§4.2、§4.3、§4.4、§5.3 限定围栏表；§5.2 放开条件加「围栏表」（补评审漏掉的无条件覆盖风险）；§3.2 加新表纪律；§11 第 2 步加三个用例 |
| 状态机 F8（Race 无上限空转且无告警）| 成立 | 次要 | §4.4 Race 改为延后、耗预算、`deferReason=fence_race`，写明理由（持锁下只可能来自围栏倒退或判定不一致）；`SaveConditional` 的错误继续套 `storeerr.Tag`；§10 加 `DbFenceRace`；§5.4 改为复用 L-04 的哨兵；§14 TiDB 未核实项改为「已由 L-04 归入 store_busy」。「连续 N 次 Race 转普通失败」不采纳（§15）。修正评审：TiDB 写冲突不表现为 Race；旧版与 `legacy`（原 `off`）实例同样取排序锁，真正会并发的只有失租后的僵尸实例和外部工具 |
| 状态机 F9（缓存里的围栏低于行内围栏）| 成立 | 次要 | §6.3 改为「缓存围栏是行内围栏的下限」并列出三种落后情形（补评审漏掉的「SQL 已落、缓存未发布」）；§7.2 两路来源表冷路径一行改写；§7.4 条件 3 改写；§8.6 补 `legacy` 期间播种只剩墙钟；class-2 第二步回填列为可选（§4.3、§14 第 18 条）|
| 事实 F1（§9.1 处置「归零加回灌」会用旧数据覆盖新数据）| 成立 | 重要 | 与状态机 F3 合成一处：§9.1 处置改为「定真源、抬计数器、不归零、不回灌」（踢下线、余量 K 或先排空二选一、缓存按真源处理、倒退期死信作废）；入口从 `fence-reset` 改为 `epoch-raise`，并同步 ownerepoch 的写入方契约；写明死信增长与 critical 级别；§9.3 改用抬计数器；长期段点明探测只是检测手段；§14 第 5、20 条。修正评审：序列 B 中围栏本身比今天好，丢数据的是 v0.2 的处置步骤；「不需要排空前置」说过头，改为余量或排空二选一 |
| 事实 F2（死信工具、分期顺序、指标端口、Redis 内存）| 成立（(c) 部分已过时）| 重要 | §8.3 前置改写：L-04 第 2 批死信工具落地并演练；条件写死信用同一列表加带原因的信封；指标端口 `:9160` 已配，ServiceMonitor 与告警 apply 改为演练留证据；分期改为 1a → 1c → 1b，加金丝雀通过标准。§9.4 删除「手工 Redis 命令」，引用 L-04 §3.8 的禁令，写增长上限与处置，可选按 (玩家, 表) 去重（§14 第 17 条）。§14 第 9 条改为「1b 等第 2 批工具」。修正评审：`stale_owner` 在 class-2 上也会出现 |
| 事实 F3（cross_zone_test 不在 game.sln）| 部分成立（核心前提不成立）| 次要 | `game.sln` 已含 `cross_zone_test`，不需要单独编 vcxproj（§15）。§11 第 6 步改为经 `run_cpp_tests.ps1` 全量运行（同步 DLL），再按逐个写出的用例名过滤运行；通过标准按 Running / PASSED 计数与用例名核对（过滤器零匹配时退出码仍为 0）|
| 事实 F4（data_stress 两轮复用同一 (E, s)，混跑结论为空）| 成立（「两轮内容不同」一句不成立）| 次要 | §11 第 8 步定义 `-fenced`（E 每轮单调、`SaveSeq = seq`、期望键），verifier 按「全 `dual` 轮」与「混跑轮」两种断言；每轮必须换 E 或换号段；§9.1 补压测提醒。评审改法 2 的「围栏等于本轮」在混跑轮会误报，改为 ≤ 加至少一部分相等；改法 3 作备选（§15）|
| 事实 F5（`legacy`（原 `off`）/ 旧实例 + epoch 推进失败 + 放行 = 回退）| 部分成立 | 重要（原评次要，上调）| 与状态机 F2 合并：§5.5 原子脚本，写明「先 epoch 后游标、任一失败即延后」的最小替代，以及「只把 `markAppliedOwnerEpoch` 失败改成延后」为何无效（§15）；§8.4、§1.2 F-5 写明成立条件；class-2 第二步在 §5.2 只写一句、跟随 §14 第 8 条。修正评审：分区再平衡不是必要条件（重试队列各实例共用） |
| 事实 F6（`Mode: off` 被 YAML 解析成布尔；off 并非唯一的行为变化）| 成立 | 次要 | §4.7 模式改名 `legacy` / `dual`，附 go-zero v1.10.0 用 yaml.v2 的证据，options 约束允许空值、空值补 `legacy`；§8.2 逐条列出 `legacy` 下生效与只在 `dual` 生效的改动；§5.3 写明只在 `dual`；§11 第 2 步加 `LoadFromYamlBytes` 配置用例，k8s 契约测试断言模板取值；§8.6 用新取值名；§4.6 的现状改为 L-04 已落地连接级超时 |
| 事实 F7（「过期成员也不自动摘除」与代码相反）| 成立 | 次要 | §8.5 改写：成员由 `retryReclaimScript` 在搬空收据后摘除；成员资格不等于正在落库，「所有成员都具备能力」会 fail-open；补门禁契约「登记失效期间不消费、不落库」。§0.1 第 7 行不改 |
| 事实 F8（漏迁的落点库不拒启）| 成立（L-04 之后不再静默）| 次要 | §3.2 把「先 DDL 后代码由机器保证」限定为本 zone 库；§8.1 第 2 步加库清单穷举并作为 Stage 1a 门禁；§8.2 加 `store_open_failed` 与 schema gate 日志观察；§3.5 写明 `storage_audit.go` 不能用来穷举 |
| 事实 F9（附录的注释同步、被拒字节数、G5 未落实）| 成立 | 次要 | §16 补三行；§10 加 `db_fence_rejected_bytes_total{outcome}`，§11 第 9 步列为对比项；新增 §5.6 G5 的处置（评审建议的 §5.5 编号已给原子推进，顺延为 §5.6）。与 §5.5 合并后游标写入进推进脚本，但仍不做 Lua 取最大值；瓶颈文档的「G1 / G3 / G5」口径待同步 |
| 事实 F10（生成物数量）| 成立 | 次要 | §0 第 7 条与 §12 改为「生成物 22 个、手写约 24 个、合计约 46 个；A 批 31 个，已超 30 文件门槛」，保留「无论如何拆都要授权」；§11 第 1 步逐一列出 22 个路径，写明 `db_task.grpc.pb.{cc,h}` 不应变化、三份陈旧的 `mysql_database_table.sql` 的处理，交 §14 第 6 条确认 |
| CPP-1（§7.4 残余风险条件写窄）| 部分成立 | 次要（原评重要，下调）| §7.4 改写：条件 3 不再把冷路径当安全侧；条件 4 改为「新进程墙钟早于旧进程最后一次发号」，并写明 etcd 租约 180 秒带来的分钟级量级（评审「3 秒后新 Pod 拿到同一 node_id」不可达）；补 dev 旁路；后果补「最后一笔被丢时 MySQL 停在旧值」；缓解补主机间时钟偏差监控；§14 第 8 条写明可达时是多主机切 `dual` 的前置；§7.1 补 node_id 与旁路事实。「用旁路 false 再跑一遍」不作 §11 第 7 步的硬性要求，作为单独验收项 |
| CPP-2（`Observe` 无上界、`Next` 无溢出处理）| 部分成立 | 次要 | §0.1 第 2 行、§7.2 删除「没有坏处」并写明代价；`Next` 饱和显式失败；`Observe` 加上界与容差；超界处置二选一列入 §14 第 13 条（建议个人下限）；§7.5 加两条单测；§9.1、§9.3 补 seq 超前的处置（抬计数器）。data_stress 不规定 SaveSeq 公式（来源不成立，§15）|
| CPP-3（`equal` 的「同内容」前提只在进程内成立）| 部分成立 | 次要 | §4.4 `equal` 一段如实写明 (E, s) 的唯一性只在单进程内由 §7 保证，跨进程同 epoch（dev 旁路、node_id 复用）可能撞号，撞号时缓存与 MySQL 可能不同；v0.3 维持补发并由 verifier 事后核对，「令牌脚本里删缓存键」列为 §14 第 12 条。「`equal` 不发布缓存」与「低位放 node_id」两个改法都不采纳（§15）|
| CPP-4（§11 第 6 步的过滤器跑不到回归用例）| 成立 | 次要 | §11 第 6 步改为 `cross_zone_test` 全量运行（现 166 个用例）并加跑 `bag_test`、`currency_test`；通过标准改为「原有用例数不减且全绿，新增用例全绿」；§7.5 写明 `Replace` 被这两个工程直接调用 |
| CPP-5（两条单测在宿主里写不出来）| 部分成立 | 次要 | §7.2 写明发号器是普通值类型，生产实例放在 `tlsIdGeneratorManager`、不受 `tlsEcs.Clear()` 影响（待落码核对），单测用局部实例、不需要复位入口；播种下限做成纯函数 `SaveSeqFloorFrom`，`InitPlayerFromAllData` 那一行无单测、由代码评审与 §11 第 7 步覆盖；§7.5 快路径用例写明先断言 `IsEqual` 再调 `SavePlayerToRedis`。修正评审：「写不出来或不稳」说重了，崩溃也是红 |

共 24 行：成立 17 条（其中 4 条带限定语），部分成立 7 条；严重度重要 6 条、次要 18 条；没有阻断项，没有整条不采纳的意见（不采纳的是部分改法或评审中不成立的论据，见各行与 §15）。

### 17.2 与 L-04 第 1 批接口的衔接（跨条目）

- handler 已是 `error` 签名，本设计扩成 `(writeOutcome, error)`；被拒结局以 `(outcome, nil)` 返回，`reportStoreResult` 只上报 SQL 这一步（§4.1、§4.5）。
- `SaveConditional` 的错误继续套 `storeerr.Tag`；条件写哨兵不打标记、耗预算；`storeerr` 只用于 SQL 调用点（§4.1、§4.4）。
- 每个新增错误或结局都用 L-04 的词表落定 `deferReason`、是否豁免、死信 label：新增 `fence_race`；死信新增 `fence_stale_owner`、`fence_legacy_after_seq`、`owner_epoch_regressed`，追加进 `deadLetterReasons` 并改 Help（§4.4、§10）。
- 失租沿用 L-04 的 `errOrderingLeaseLost`，v0.3 默认不并入 `retryBudgetExempt`；核实意见有分歧，列 §14 第 14 条（§5.4）。
- 计数器探测与推进脚本的 Redis 失败计 `redis_unavailable`、耗预算，不进豁免（§4.4、§5.1、§5.5）。
- 连接级超时直接复用 `newServiceMysqlConfig` 的五项；本设计只补 `WithContext`（§4.6）。
- 合入顺序 L-04 在前；Stage 1b 还依赖 L-04 第 2 批死信工具（§8.3、§14 第 9 条）。
- 行号基线：v0.2 引用 `0aa1027bc`；工作区 KOC 整体后移约 4～51 行、PL 插入约 286 行，所以 v0.3 改写处以函数名为主（文首「证据口径」）。
