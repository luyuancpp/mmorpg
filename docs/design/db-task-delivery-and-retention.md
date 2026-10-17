# scene DBTask 投递失败后的重存与 db_task 保留期 / Kafka 盘预算（S5）

> **状态**：已落码，未编译、未测试，待 Codex 验证（AGENTS.md §10.1）。在 Codex 给出结果之前，本文任何一句都不等于「编译通过」或「已验证」。
> **范围**：§A = S5a（C++ scene：发 DBTask 失败后的脏快照）；§B = S5b（Go + 部署脚本：db_task 的 retention.bytes 与 Kafka PVC 预算）。go/db、go/login 与 `tools/scripts/k8s_deploy.ps1` 的注释按「本文 §B」引用，章节号不要改。
> **依据**：[S5 实现规格](../handoff/godb-pipeline-impl-20261015/S5-dirty-snapshot-retention-spec.md)（B-01…B-22）；S5a 的落码报告 [batch-reports.md](../handoff/godb-pipeline-impl-20261015/batch-reports.md) §C2；S5b 由主会话在 R3（Go 与部署脚本）与 R5a（复核修正）中落码，没有单独的阶段报告。**表述以截至 2026-10-16 的工作区代码为准**，与规格、阶段报告不一致的地方列在 §A.13 与 §B.9。
> **归属**：负责人 2026-10-13 把「scene 发 DBTask 失败的脏快照」与「db_task 的 retention / PVC」改判归 godb 交接（规格 B-18；[上线待做](../ops/godb-pipeline-launch-todo.md) §3）。全局单点交接不要重做这两项。
> **运维版**：`deploy/k8s/README.md`「Kafka 磁盘预算与 db_task retention.bytes」一节是 §B 的操作手册（公式、缺省值、账本行、扩盘 runbook、立即改 topic 配置）。本文讲为什么这样设计，命令以 README 为准。
> **旧稿**：`docs/design/kafka-topic-retention-strategy.md`（2026-04-12）里 db_task 的数值已过时，以本文 §B 与 README 为准。

---

## 0. 结论速览

1. **缺陷**：scene 每次存盘同时写 Redis 与两条 DBTask（Kafka → go/db → MySQL）。改动前，DBTask 同步被拒或投递回执失败只打日志，Redis 落地回调照样前移 dirty-save 快照，下一次存盘被快路径判成「没变化」跳过，MySQL 一直落后于 Redis；下线时 Redis 一落地就销毁实体，能补写 MySQL 的内存也没了。
2. **S5a 修法**：
   - 生产者加逐条回执（`KafkaProducer::sendTracked`），每条入队的被跟踪消息恰好回执一次；
   - scene 按玩家记一本投递账本（`db_task_dispatch::Ledger`），有未送达的失败就让快路径让路，下一次存盘按当前内存整份重写，绝不重放旧载荷；
   - 下线收尾过一道闸门（`player_exit::DecideDbTaskGate`）：等本玩家所有在途 DBTask 拿到回执才销毁实体；投递失败时最多重存 3 轮，超限就保留实体。
3. **S5b**：db 与 login 启动时在 topic 级设置 db_task 的 `retention.bytes`；`k8s_deploy.ps1` 按盘预算公式同时定出 PVC 大小与 `retention.bytes`，盘不够时按盘收缩并告警；db 与 login 两个 ConfigMap 的 `RetentionMs` / `RetentionBytes` 从同一个变量渲染，修掉保留期随重启顺序来回横跳的既有缺陷。
4. **行为变化（负责人已拍板，需知悉）**：
   - 下线销毁实体要等 DBTask 的 acks=all 回执；
   - Kafka 不可用时，退出实体一直保留，直到 Kafka 恢复、再来一次退出请求（rekick），或停机看门狗到期；
   - 周期存盘不再被快路径跳过，每个周期整份写一次 Redis；
   - prod / prod-like 档的 db_task 缓冲由 `retention.bytes` 决定，按设计速率约 2 小时，不是 retention.ms 的 24h / 6h。
5. **没覆盖**：停机看门狗（15s）到期、进程崩溃时，在途或未送达的那份 MySQL 写只能等玩家下次登录后由新属主补写；lag 秒级告警没做（B-12）。

---

## §A 脏快照：DBTask 投递失败后强制重存、下线等回执（S5a）

### A.1 现状与缺陷

存盘入口是 `cpp/libs/services/scene/player/system/player_lifecycle.cpp` 的 `PlayerLifecycleSystem::SavePlayerToRedisImpl`，每次存盘都有两路写：

- **Redis**：`PlayerDataRedis` 的 `Save`（`owner_epoch != 0` 时走带 CAS 的重载），落地回调是 `HandlePlayerAsyncSaved`，失败回调是 `HandlePlayerAsyncSaveFailed`，CAS 拒绝回调是 `HandlePlayerSaveRejected`（接线在 `cpp/libs/services/scene/core/system/redis.cpp` 的 `RedisSystem::Initialize`）。
- **MySQL**：lambda `sendSubTableTask` 为 `player_database`、`player_database_1` 各发一条 `taskpb::DBTask`，topic 由 `GetDbTaskTopic`（`cpp/libs/services/scene/player/constants/player.h`）按 home_zone 与世代号选出，key 是 `DbTaskKafkaKey(playerId)`。

改动前的缺陷链：

1. DBTask 同步发送被拒只打 `LOG_ERROR`，存盘照常算成功；投递回执失败只在 `KafkaProducer::dr_cb` 里计 `kafka_producer_stats` 的 delivery_failed 并打限流日志，不通知调用方。
2. Redis 落地后 `HandlePlayerAsyncSaved` 照常把 `PlayerLastPersistedSnapshotComp` 换成这次的载荷。
3. 下一次存盘时，dirty-save 快路径（快照与当前内存相等就跳过）判成「盘上已是同一份」，Redis 和 DBTask 都不写。MySQL 一直落后，直到内存再变。
4. 下线时 Redis 一落地实体就被销毁，能补写 MySQL 的内存也随之消失。

Redis 失败路径 `HandlePlayerAsyncSaveFailed` 刻意不动快照，Kafka 路径以前没有对等的处理。

### A.2 为什么不作废快照（规格 B-03）

任务卡片原文要求「失败即作废快照」。按字面实现是错的，所以改成另记一本账：

- 快照的语义是「这份字节**确实已进 Redis**」。战斗结算（`cpp/libs/services/scene/battle/system/player_battle.cpp`）与资产通道（`asset_op_system.cpp`）拿它判断「已落盘」并销账。为 Kafka 去作废它，会推迟或重做这些销账。
- 只有 Redis 落地回调会写快照（`HandlePlayerAsyncSaved` 里的 `get_or_emplace<PlayerLastPersistedSnapshotComp>(...).Replace(...)`）。DBTask 的回执可能早于 Redis 落地到达，作废之后会被落地回调重新写回。这正是原缺陷的成因。

所以快照语义不变。「还欠 MySQL 一次整份写」由 §A.4 的账本单独记，快路径同时问这两件事（§A.6）。

### A.3 生产者逐条回执契约（`KafkaProducer::sendTracked`）

代码：`cpp/libs/engine/infra/messaging/kafka/kafka_producer.{h,cpp}`；关联号登记簿是 `kafka_producer_policy.h` 的 `kafka_producer_policy::DeliveryTracker`（header-only：`Accept` / `Resolve` / `TakeAll` / `Size`）。

| 项 | 契约 |
|---|---|
| 关联号 | `sendTracked(topic, message, key, correlationId)` 与 `send()` 共用 `sendImpl`。关联号编码在 `msg_opaque` 的指针值里，回执时由 `Message::msg_opaque()` 带回。`static_assert(sizeof(std::uintptr_t) >= sizeof(std::uint64_t))` 保证 64 位不被截断。0 = 不跟踪，与改动前的 `send()` 逐字节相同 |
| 入队与登记 | 返回值不是 `ERR_NO_ERROR` = 没有入队，**永远不会**有回执。返回 `ERR_NO_ERROR` 才 `Accept`，而且必须在本次 `poll()` 之前：这条消息的回执最早就在那次 poll 里派发 |
| 恰好一次 | 每个 `Accept` 过的 id 恰好一次经下面三个出口之一离开：① `dr_cb` 里 `Resolve` 成功后通知（成功、失败都通知，失败在计数与限流日志之后）；② `rebuildAfterFatal` 在 purge 与有界 flush 之后，用 `TakeAll` 为还没派发的消息合成 `ERR__PURGE_QUEUE`（`synthesized = true`），必须在 `producer_.reset()` 之前做（`rd_kafka_destroy` 不回调 `dr_cb`），并打一行 `[Kafka] N tracked messages lost their delivery report when the fatal producer instance was purged; reported as synthesized failures.`；③ 调用方自己的超时兜底（scene 侧 330s，§A.5）。合成之后原回执迟到时 `Resolve` 返回 false，不会二次通知 |
| 结局含义 | `ERR_NO_ERROR` = acks=all 已确认（幂等生产者隐含 acks=all）。非零 = 没送到，或可能没送到（`ERR__MSG_TIMED_OUT` 属于后者）。补救只能重发**当前**状态，不能重发旧载荷 |
| 线程与回调 | 生产者是 `thread_local`。监听器只在本线程的 `poll()` / `flush()` / `send()`（队列满重试与重建路径）调用栈里同步回调，只许记账：不得再调 send / sendTracked / poll / flush，不得在回调里换监听器，不得改 ECS 组件集合 |
| 析构 | 第一行先 `deliveryListener_ = nullptr`，再 `flush(5s)`，不依赖 thread_local 的析构顺序。之后还有未回执的跟踪消息时打 `[Kafka] producer destroyed with N tracked messages still awaiting delivery reports` |
| 超时耦合 | `createProducer` 不覆盖 `message.timeout.ms`（librdkafka 缺省 300000）。scene 的 `kTicketTimeout` 必须**严格大于**它。改一处必须同步改另一处，否则迟到的成功回执会先被判成失败（后果只是多一次重存，不丢数据） |

回执只在本线程的 poll / flush / send 里派发（C2 已对照 vendored librdkafka 核实：`dr_msg_cb` 走 trampoline，`rd_kafka_flush` 在 DR_MODE_CB 下内部调 `rd_kafka_poll`；scene 的生产者没有配 background queue / event_cb）。空闲节点上没有 send() 驱动，回执会一直压着，所以需要 §A.5 的 Pump。

### A.4 投递账本（`db_task_dispatch::Ledger`）

代码：`cpp/libs/services/scene/player/system/db_task_dispatch_ledger.{h,cpp}`。纯数据结构：不 include ECS 和 rdkafka，不读墙钟，时刻一律由调用方传入；只在 scene 主循环线程上使用，不加锁。

**记账单位**：代号（`SaveGeneration`）每次存盘 `BeginSave` 领一个，全局单调递增，从 1 起；票据（`Ticket`）每条 DBTask 一张，作为 `sendTracked` 的关联号，全局单调递增、不复用，0（`kNoTicket`）不合法。

**每个玩家的状态**：`inFlight`（已发出、还没有结局的票据数）、`lastFullyQueued`（最近一次两条 DBTask 都同步入队的代号）、`maxFailed`（出过失败的最大代号）。

**判据**：`NeedsResave = maxFailed != 0 && maxFailed >= lastFullyQueued`，意思是有一次失败还没被一次更新的、整份入队的存盘盖过。

| 事件 | 接口 | 对状态的影响 |
|---|---|---|
| 开始一次存盘 | `BeginSave(pid)` | 按需建条目，返回新代号 |
| 即将发一条 DBTask | `IssueTicket(pid, gen, now)` | `inFlight + 1`，登记票据与发票时刻。**必须在真正 produce 之前调**：回执可能在 send() 内部的 poll 里就到 |
| 同步被拒 | `OnProduceRejected(ticket)` | `inFlight − 1`，`maxFailed = max(maxFailed, gen)`，销票。不认识的票据什么都不做 |
| 本地失败（序列化失败） | `MarkLocalFailure(pid, gen)` | `maxFailed = max` |
| 本次存盘发完 | `FinishSave(pid, gen, allQueued)` | `allQueued` 时 `lastFullyQueued = max`；否则这一代也记为失败（防御写法，结论不变） |
| 回执 | `OnDelivery(ticket, delivered)` | `inFlight − 1`。成功返回 `kDelivered`；失败时 `maxFailed = max`，`gen >= lastFullyQueued` 返回 `kFailedNeedsResave`，否则返回 `kFailedSuperseded`。不认识的票据（重复回执、超时之后迟到、条目已回收）返回 `kUnknownTicket` |
| 超时兜底 | `ExpireStale(now, timeout)` | 按发票顺序检查，满 timeout 仍没有结局的按失败交给 `OnDelivery`；遇到第一张没到期的活票据就停 |
| 实体离开 | `ForgetIfNoInFlight(pid)` | 没有在途票据才回收条目，返回回收前是否欠账 |

设计要点：

- **用代号比大小，不用布尔清零**。回执可能先于 `FinishSave` 到（inFlight 在 produce 之前就加了），也可能晚于下一次存盘才到；两种顺序下结论相同，不需要额外的时序约定。一次失败之后，只有之后某次存盘的 DBTask **全部**同步入队、且没有代号 ≥ 它的失败回执，`NeedsResave` 才会自然解除；那次存盘自己的回执再失败，又会重新置位。
- `inFlight` 递减不下溢：已经是 0 时打 `LOG_ERROR`（`[DbTaskDispatch] in-flight counter underflow ...`）并钳在 0，否则回绕成约 4G 张在途票据，会让这个玩家的退出永远等下去。
- `Clear()`（单测用）不重置代号与票据计数器：生产者的登记簿里可能还挂着旧票据号，复用会让迟到的旧回执记到新票据头上。
- `AllowsDirtySaveSkip(allowSkipWhenPersisted, snapshotMatches, needsResave) = allowSkipWhenPersisted && snapshotMatches && !needsResave`，是快路径唯一的判据（`constexpr`，单测穷举 8 种组合）。

### A.5 粘合层 `DbTaskDispatchSystem` 与 Pump

代码：`cpp/libs/services/scene/player/system/db_task_dispatch_system.{h,cpp}`。全部静态；状态放在 .cpp 里的函数内 `thread_local State`（账本、待办玩家表、`onSettled`、`initialized`）；头文件不 include rdkafka。

- **初始化**：`RedisSystem::Initialize` 在 `SetSaveRejectedCallback` 之后调 `DbTaskDispatchSystem::Initialize(&PlayerLifecycleSystem::OnDbTaskDispatchSettled)`，用**无捕获** lambda 登记本线程 `KafkaProducer` 的监听器，转调 `OnDeliveryReport`，不绑任何对象（AGENTS §11.7）。
- **`SendDbTask`**：先 `IssueTicket(now)` 再 `sendTracked`；没入队时 `OnProduceRejected`、计 `produce_rejected`，`error = RdKafka::err2str(err)`。监听器还没登记时（初始化顺序被破坏，生产上不会发生）退化为不跟踪的 `send()`：同步失败照样记成本代失败（`MarkLocalFailure` 并计 `produce_rejected`），异步失败无从得知，即改动前的行为。
- **`OnDeliveryReport`**（监听器本体）：只改账本、计数、压待办；`inFlightAfter == 0` 或 `kFailedNeedsResave` 时把玩家号压进待办表。不碰 ECS，不打逐条日志（生产者已按限流记了失败）。
- **`Pump`**：由 `RedisSystem::Initialize` 里 `kDbTaskDispatchPumpIntervalSec = 0.05` 的 `runEvery` 定时器在 loop 顶层驱动，lambda 无捕获。每拍三步：
  1. `KafkaProducer::Instance().poll()` 派发回执；
  2. `ExpireStale(now, kTicketTimeout)`，`kTicketTimeout = 330s`；有过期票据时计 `ticket_expired`（逐张计），每拍打一行 `LOG_ERROR`（条数 + 首个玩家号，`metric=db_task_ticket_expired`），并把玩家压进待办。注意过期**不计** `delivery_failed`；
  3. 把待办表换出来、排序去重，逐个交给 `onSettled`。先换出再处理：onSettled 里的重存又会发 DBTask，它们的回执压进新的待办表，留给下一拍，不会在本拍里无限循环。
- **停机**：`RedisSystem::BeginShutdown` 只取消周期存盘，**不**取消 Pump：停机 drain 期间等回执、被保留的退出实体要靠它推完。`RedisSystem::Shutdown` 在 `loop_` 有值的分支里先取消 Pump 定时器，再调 `DbTaskDispatchSystem::Shutdown()`。后者幂等：注销监听器；有未落定玩家时打一行 `[DbTaskDispatch] shutting down with N player(s) whose MySQL copy may lag Redis (in-flight or undelivered DB write tasks): ids=[...]`，最多列 `kShutdownUnsettledLogLimit = 50` 个，超出时带 `(truncated)`；最后清空 `onSettled` 与待办表。

### A.6 存盘入口：快路径让路与发 DBTask 记账

`SavePlayerToRedisImpl` 的返回值是 `SaveResult`（`kQueued` / `kAlreadyPersisted` / `kHandoffAlreadyRequested` / `kRejected`），语义不变：`kQueued` = 已发出 Redis 存盘、之后一定有落地回调。Kafka 的结果不进返回值，只进账本。原因是 5 个以上调用方都按这个语义收尾，改它会牵连退出、交接与资产通道（规格 B-02）。

1. **快路径**：`snapshotMatches = allowSkipWhenPersisted && snap && snap->HasSnapshot() && player_exit::IsPersistedPayloadCurrent(*message, *snap->snapshot)`。只在快照相等时才查账本（`dbTaskNeedsResave = snapshotMatches && DbTaskDispatchSystem::NeedsResave(playerId)`），`AllowsDirtySaveSkip` 为真才返回 `kAlreadyPersisted`。快照相等但欠账时强制真写：计 `forced_resave`，打 `LOG_INFO "[SavePlayerToRedis] forcing a real save for player X: an earlier DB write task was not delivered (metric=db_task_forced_resave)"`。
2. **发 DBTask**：lambda 之前 `BeginSave` 领代号，置 `allDbTasksQueued = true`；两处 `SerializeToString` 失败都走 `MarkLocalFailure`；发送改为 `DbTaskDispatchSystem::SendDbTask(playerId, saveGeneration, dbTaskTopic, dbTaskBytes, playerIdStr)`，没入队时保留原有的 `Kafka send failed` 日志并带上 `err=`；两条都发完后调 `FinishSave(playerId, saveGeneration, allDbTasksQueued)`。没有全部入队时再打一行汇总 `LOG_ERROR`（`... MySQL lags Redis until the next save re-sends the full state (metric=db_task_produce_rejected)`）。
3. **强制重存永远重新 marshal 当前内存**。`owner_epoch != 0` 时 `PlayerOwnerEpochComp::lastSaveSeq` 自增，新的 save_seq 写进 DBTask 与 `fence_save_seq`，所以重发的是一份更新的版本，不是旧字节。

### A.7 下线收尾的 DBTask 回执闸门

纯决策在 `cpp/libs/services/scene/player/system/player_exit_intent.h`：`player_exit::DecideDbTaskGate(inFlight, needsResave, rounds, maxRounds)`，上限 `kMaxExitDbTaskResaveRounds = 3`，名字表 `kDbTaskGateDecisionNames`。闸门状态记在 `PlayerExitIntentComp` 的两个纯运行时字段上：`awaitingDbTaskAck`（只由 `FinishExitOrAwaitDbTaskAck` 置位或清除）、`dbTaskResaveRounds`（只由闸门的重存分支递增，由 rekick 清零；与 Redis 不收敛的 `resaveRounds` 分开计）。

执行在 `PlayerLifecycleSystem::FinishExitOrAwaitDbTaskAck(Guid)`：实体无效时直接 `FinishExitAfterPersist` 并返回 true；`PlayerExitIntentComp` 缺失时打 `LOG_WARN` 后同样直接收尾（沿用旧行为）。其余按决策表：

| needsResave | 轮次 < 3 | inFlight | 决策 | 动作 | 返回 |
|---|---|---|---|---|---|
| 否 | — | 0 | `kFinish` | 清 `awaitingDbTaskAck` → `FinishExitAfterPersist`（销毁） | true |
| 否 | — | > 0 | `kAwaitAck` | 首次进入时计 `exit_await_ack`，打 `LOG_INFO (metric=exit_db_task_await_ack)`；置 `awaitingDbTaskAck` | false |
| 是 | 是 | 任意 | `kResave` | `++dbTaskResaveRounds`，置 `awaitingDbTaskAck`，计 `exit_resave`，打 `LOG_WARN (round r/3, metric=exit_db_task_resave)`；然后 `SavePlayerToRedisImpl(entity, /*allowSkipWhenPersisted=*/false)`：`kQueued` → 等落地回调与新回执；`kRejected` → `DestroyDeposedPlayer(playerId, "exit_save_refused")` 不存盘丢弃（`metric=save_marshal_refused`）；交接已发起（`kHandoffAlreadyRequested`）且还有未落地的 Redis 存盘 → 等它落地；否则计 `exit_unsynced`，打 `LOG_ERROR (metric=exit_db_task_unsynced)` 后 `FinishExitAfterPersist` | kQueued / 等落地：false；丢弃 / 收尾：true |
| 是 | 否 | 任意 | `kRetainExhausted` | **每次判定**都计 `exit_resave_capped`，打 `LOG_ERROR (metric=exit_db_task_resave_capped)`；置 `awaitingDbTaskAck`，保留实体与 `UnregisterPlayer` | false |

需要重存优先于等回执：失败已经确定，再等剩下的回执也补不回那一份；重存发的是当前内存整份，剩下的旧票据之后无论成败都会被它盖过（`kFailedSuperseded`）。

**入口**：原来直接调 `FinishExitAfterPersist` 的收尾口全部改走闸门。所有退出原因统一走，不为 `kReleasedByTransfer` 或有改派票据的情况开例外：

- `HandlePlayerAsyncSaved` 退出分支：收敛（`kFinish`）时闸门返回 false 就 break 到函数末尾，照常更新快照（这份字节确实已进 Redis）；重存后收敛的分支直接交给闸门（快照在前面已 Replace）。
- `HandleExitGameNode`：快路径内联收尾（`kAlreadyPersisted` / `kHandoffAlreadyRequested`，更早一次存盘的 DBTask 可能还在途），以及「重存超限保留」的 rekick 收敛。
- `HandlePlayerSaveRejected`：旧代际在途写被拒、退出中的实体重存时判成「盘上已是同一份」或交接已发起，且没有未落地存盘。
- `OnDbTaskDispatchSettled` 的末尾（见下）。

**恢复收尾**：`PlayerLifecycleSystem::OnDbTaskDispatchSettled(Guid)` 是 Pump 的 `onSettled`，只在 loop 顶层调用，全程只用 `GetPlayer` / `valid` / `try_get` / `any_of`，不 `get_or_emplace`：

1. 实体无效 → `DbTaskDispatchSystem::OnPlayerGone` 回收条目（欠账时计孤儿，§A.9）；
2. 在线玩家（没有 `UnregisterPlayer`）→ 返回，欠账由下一次存盘消化；
3. 意图缺失或 `!awaitingDbTaskAck` → 返回，收尾还没走到闸门，Redis 落地回调会自己走过来；
4. 还有未落地的 Redis 存盘（`HasUnsettledPlayerSave`）→ 返回，落地回调会再判；
5. 还有在途回执且没有确定的失败 → 返回，接着等；
6. 重新 `MarshalPlayerForSave`，与落地快照比 `IsPersistedPayloadCurrent`（等回执期间实体可能又被改过）。marshal 失败 → `DestroyDeposedPlayer(playerId, "exit_save_refused")`。不收敛 → 复用 `DecideAfterPersist` 与 `resaveRounds` / `kMaxExitResaveRounds`（5）做上限：没到上限就计 `[ExitPersist]` 的 `exit_resave` 并重存一次；到上限就计 `[ExitPersist]` 的 `exit_resave_capped` 后保留实体。这样避免「等回执 → 发现变了 → 重存 → 又等回执」的无界循环；
7. 收敛 → 回到 `FinishExitOrAwaitDbTaskAck`。

**rekick**：`HandleExitGameNode` 的「已在退出中」分支，在原有的 `ShouldRekickExhaustedExit` 之后加了一个 else-if：`player_exit::ShouldRekickDbTaskExhausted(awaitingDbTaskAck, dbTaskResaveRounds, kMaxExitDbTaskResaveRounds, InFlight, NeedsResave, HasUnsettledPlayerSave)` 五个条件同时成立时（停在闸门上、确实欠账、轮次已到上限、没有在途 DBTask、没有未落地的 Redis 存盘），把 `dbTaskResaveRounds` 清零，计 `exit_rekick`，打 `LOG_WARN (metric=exit_db_task_rekick)`，然后重新进闸门。停机时周期存盘已取消，`exitAllPlayers` 这一次就是被保留实体最后一次重存机会。

`PlayerLifecycleSystem::IsSaveInFlight` 的语义随之扩展：闸门等待期间 `UnregisterPlayer` 一直挂着，它照常返回 true，停机 drain 的谓词据此继续等。

### A.8 被保留实体的出口与停机看门狗

被保留（`kRetainExhausted`）或等回执（`kAwaitAck`）的退出实体有四个出口：

1. **Kafka 恢复**：在途回执到达 → Pump → `OnDbTaskDispatchSettled` → 闸门。
2. **周期存盘**：`RedisSystem::Initialize` 的 1s 分槽定时器（`SCENE_PLAYER_SAVE_INTERVAL_SECONDS`，缺省 300，按 `playerId % 300` 每秒一槽）遍历 `tlsEcs.playerList`，退出中的实体也在其中。欠账时快路径让路、整份重写；落地后由退出分支照常判定。新一代整份入队之后 `NeedsResave` 解除，闸门转为等回执；这批回执再失败，就再判一次 `kRetainExhausted`。
3. **再来一次退出请求**：停机 `exitAllPlayers`、s2s LeaveScene、排空、疏散都会走 `HandleExitGameNode`，触发 §A.7 的 rekick。
4. **停机看门狗**：`Node::StartShutdownDrainWatchdog`（`cpp/libs/engine/core/node/system/node/node.cpp`，`kShutdownDrainBudget = 15s`）。scene 的停机谓词（`cpp/nodes/scene/main.cpp` 里的 `SetShutdownDrainComplete`）要求所有停机玩家 `IsSaveInFlight` 为假、`FlushProducer(0)` 为真（另有剩余玩家数、A1′ 在途数、改派待确认数都归零）。Kafka 不可用时 15s 远短于 300s 的 `message.timeout.ms` 与 330s 的票据超时，看门狗必然先到期，打 `Before-shutdown drain budget exceeded; continuing with work still in flight.` 后继续停机。随后 `DbTaskDispatchSystem::Shutdown` 的 ERROR 行列出未落定的玩家号，`KafkaProducer` 析构时再报还没回执的跟踪消息数。这些玩家的 Redis 是最新的，下次登录自愈；但合服、回档这类以 MySQL 为准的操作会读到旧档，那行 ERROR 就是追查依据。

### A.9 孤儿口径

孤儿 = 实体离开本节点时，账本里还记着一次没送达的失败。

- `DestroyPlayer`（唯一的实体销毁出口）在 `DestroyEntity` 与 `tlsEcs.playerList.erase` 之后调 `DbTaskDispatchSystem::OnPlayerGone(playerId)`。`OnDbTaskDispatchSettled` 发现实体已不在时也调它。
- `OnPlayerGone` → `ForgetIfNoInFlight`：还有在途票据就不回收，等回执到了经 Pump → `OnDbTaskDispatchSettled`（实体已不在）再回收一次；回收时仍欠账就计 `orphan_unsynced`，打 `LOG_WARN "[DbTaskDispatch] player X left this node with an undelivered DB write task; MySQL lags Redis until the next owner saves this player (metric=db_task_orphan_unsynced)"`。
- 下线收尾有闸门，正常不产生孤儿。孤儿来自不经闸门的销毁：被废黜（`DestroyDeposedPlayer`，如 owner_epoch CAS 拒绝）、交接放行后的源端销毁、退出拒存丢弃（`exit_save_refused`）、以及闸门里交接已发起无法再写时的收尾（这时 `exit_unsynced` 与 `orphan_unsynced` 都会加 1，`orphan_unsynced` 包含 `exit_unsynced` 那一部分，`db_task_dispatch_system.h` 注释已写明）。
- MySQL 的自愈：新属主的首存一定写盘。只有落地回调会写落盘快照，新建的实体没有快照，首存不可能走快路径。在那之前只有 Redis 是最新的。

### A.10 go/db 能挡什么：为什么绝不重放旧字节（规格 B-07）

本设计的不变量是**补救一律重新 marshal 当前内存整份重发，绝不重放旧载荷**（`kafka_producer.h`「投递语义」段同此）。不靠 go/db 去挡旧字节，理由按 go/db 的各层守卫：

- **游标**（`go/db/internal/kafka/key_ordered_consumer.go` 的 `orderingForTask` / `markAppliedSeq`）比的是同分区 offset，只挡 offset ≤ 游标的重复投递。重发的消息拿到更大的 offset，一律放行。
- **owner_epoch**（同文件 `checkOwnerEpoch` / `guardOwnerEpoch`）只在 taskEpoch 小于已落库的最大 epoch 时拒绝。同节点重登不铸造新 epoch（`go/scene_manager/internal/logic/enterscenelogic.go` 的 `mint: !samePhysicalNode || sameNodeZeroMint`），同 epoch 的旧载荷照样放行。
- **SQL 行围栏**（规格写作之后由 MySQL 条件写会话落码，`go/db/internal/kafka/player_row_fence.go` 的 `savePlayerRow`，设计见 `db-write-fencing-mysql.md`）：`player_database` / `player_database_1` 的带版本写按 `(fence_owner_epoch, fence_save_seq)` 裁决，`(epoch, seq)` 更旧的判 stale，同 `(epoch, seq)` 视为幂等重放。截至 2026-10-16 工作区，它能挡住带版本的旧字节；但 `owner_epoch == 0` 的兼容窗口里写的是无版本行，没有任何守卫。S5 的不变量不依赖它。

重发当前状态是幂等的：整行覆盖，内容相同或更新，游标前进，epoch 判 match，save_seq 更大。librdkafka 的内部重试由幂等生产者在分区内去重。回执失败里有一类是「可能已持久化」（`ERR__MSG_TIMED_OUT`），即使它其实已经写进日志，后面的整份重写也会按序覆盖，结果一致。S5 不改 go/db 的任何代码，`markAppliedSeq` / `markAppliedOwnerEpoch` 的调用顺序与位置不变。

### A.11 计数与日志

计数在 `db_task_dispatch_stats`（`db_task_dispatch_system.h`）：进程级 relaxed atomic，写法同 `owner_epoch_stats`，**不接 Prometheus，玩家号绝不当 label**。`RedisSystem` 的 30s 统计定时器在计数有变化、或还有未落定玩家时打一行 `[DbTaskDispatch]`（key=value 平铺的累计值）；`Snapshot::AlarmingIncreaseSince` 为真时升 WARN，否则 INFO。玩家号不进这一行。

| key | 含义 | 稳态 |
|---|---|---|
| `produce_rejected` | DBTask 同步发送被拒（没入队） | 0 |
| `local_failure` | DBTask 序列化失败，没走到发送 | 0 |
| `delivery_failed` | 投递回执失败（含重建时合成的失败） | 扣除下一项后应为 0 |
| `delivery_failed_superseded` | 上一项里已被更新的整份入队存盘盖过、不需要重存的部分 | 可非 0 |
| `ticket_expired` | 330s 内没有任何回执，按失败处理 | 0 |
| `forced_resave` | 快路径本可跳过，因欠账强制真写 | Kafka 正常时 0 |
| `exit_await_ack` | 下线收尾在等回执（每次退出至多计一次） | 随下线增长，正常 |
| `exit_resave` | 闸门因投递失败重存 | 0 |
| `exit_resave_capped` | 闸门重存到上限仍失败，保留实体（每次判定计一次） | 0 |
| `exit_rekick` | 被保留的实体又收到退出请求，重新开始重存 | 0 |
| `exit_unsynced` | 闸门无法再重存（交接已发起），带着 MySQL 落后收尾 | 0 |
| `orphan_unsynced` | 实体离开本节点时仍欠账（含 `exit_unsynced` 那一部分） | 0 |

行尾另有 `unsettled_players=N in_flight_tickets=M`。告警性增长 = `produce_rejected`、`local_failure`、(`delivery_failed` − `delivery_failed_superseded`)、`ticket_expired`、`exit_resave_capped`、`exit_unsynced`、`orphan_unsynced` 任一增长。

注意同名：`[ExitPersist]` 行（`player_lifecycle.cpp` 的退出统计定时器）也有 `exit_resave` 与 `exit_resave_capped` 两个 key，含义是「Redis 落地内容与内存不收敛 → 重存 / 重存到 5 轮上限」，与 `[DbTaskDispatch]` 的同名 key（投递失败 → 重存 / 到 3 轮上限）不是一回事，按行前缀区分。`[DirtySave]` 行的解析格式没动，但 Kafka 不可用期间强制真写会让 `skip_pct` 下降（`forced_resave` 计入 total、不计入 skipped）。`tools/scripts/stress_summarize.ps1` 目前不解析 `[DbTaskDispatch]` 行。

日志里的 `metric=` 标签（英文正文、中文注释）：`db_task_forced_resave`、`db_task_produce_rejected`、`exit_db_task_await_ack`、`exit_db_task_resave`、`exit_db_task_resave_capped`、`exit_db_task_unsynced`、`exit_db_task_rekick`、`db_task_orphan_unsynced`、`db_task_ticket_expired`；拒存丢弃沿用 `save_marshal_refused`。另有两行无标签的 ERROR：`[DbTaskDispatch] shutting down with N player(s) ...` 与 `[Kafka] N tracked messages lost their delivery report ...`。

### A.12 行为变化与残余风险

负责人已拍板（规格「待决项」）：接受下线收尾等回执；所有退出原因统一走闸门。行为变化：

1. 正常下线销毁实体要等 acks=all 回执，比改动前多一个 broker 往返加至多一个 Pump 间隔（≤ 50ms）。
2. Kafka 不可用时，下线实体与 Redis 失败时一样被保留：先等回执（最长约 300s 的 `message.timeout.ms`），失败后最多重存 3 轮，然后保留，直到 Kafka 恢复、rekick 或停机看门狗到期。节点内存里的保留实体上限是在线人数。
3. 没登记 broker 或 Kafka 长时间不可用时，每次 DBTask 都失败，账本对所有在线玩家都记着欠账。结果是周期存盘不再被快路径跳过，每个周期整份写一次 Redis，Redis 写量回到没有 dirty-save 时的水平。
4. 退出流程拿到 `kRejected`（持久组件缺失、save_seq 耗尽）一律 `DestroyDeposedPlayer(..., "exit_save_refused")`，不存盘丢弃，不写 A1′、交接标记，也不发改派。这是 ECS 评审批 6 的口径（负责人 2026-10-16「核心拍板 2」，见 `docs/handoff/ecs-refactor/batch6-review-fixes.md` T2.1），闸门的重存分支与 `OnDbTaskDispatchSettled` 同样适用。

残余风险：

1. 全部未编译、未测试。
2. 回执靠新增的 50ms Pump 驱动；`kTicketTimeout = 330s` 依赖 librdkafka 缺省的 `message.timeout.ms = 300000`。有人改后者却不同步改前者，迟到的成功回执会被先判成失败（只多一次重存，不丢数据）。
3. 停机看门狗到期、进程崩溃时，在途或未送达的那份 MySQL 写没有补救，只能等下次登录由新属主补写；合服、回档以 MySQL 为准时读到旧档。停机时的 ERROR 行是证据，崩溃时没有。
4. 被废黜、交接放行这些路径产生的孤儿，要等新属主的首存来补；新节点在首存之前崩溃时，只剩 Redis 一份。
5. `player_lifecycle.cpp` / `player_lifecycle.h` 同时有其它会话（条件写、MarshalPlayerForSave、ECS 评审批 6）的未提交改动，合并时按函数名重新定位，不回退别人的改动。

### A.13 与规格、阶段报告不一致处（以代码为准）

| # | 规格 / C2 报告 | 截至 2026-10-16 工作区代码 |
|---|---|---|
| 1 | `SavePlayerToRedisImpl` 返回 bool，「返回 false 就收尾并计 exit_unsynced」 | 返回 `SaveResult` 枚举，按值逐个处理（§A.7 表）。只有交接已发起、且没有未落地存盘时才计 `exit_unsynced` 后收尾 |
| 2 | C2：闸门重存拿到 `kRejected` 时 fail-closed 保留实体，`OnDbTaskDispatchSettled` 的 marshal 失败也保留 | 改为 `DestroyDeposedPlayer(playerId, "exit_save_refused")` 不存盘丢弃（ECS 评审批 6 T2.1，负责人 2026-10-16 拍板）。`player_lifecycle.h` 里 `FinishExitOrAwaitDbTaskAck` 的声明注释仍写「拒存时 fail-closed 保留实体，返回 false」，与 .cpp 不一致，待改该文件的会话同步 |
| 3 | `OnDbTaskDispatchSettled` 不收敛时无条件重存 | 复用 `DecideAfterPersist` 与 `resaveRounds` / `kMaxExitResaveRounds` 做上限，到上限保留实体并计 `[ExitPersist]` 的 `exit_resave_capped`；没有快照按不收敛处理 |
| 4 | `kRetainExhausted` 只在首次计 `exit_resave_capped` | 每次判定都计。kResave 已经把 `awaitingDbTaskAck` 置位，照规格写法这个计数在生产上永远不会增长 |
| 5 | `DestroyPlayer` 开头、紧挨 `AnomalyDetector::ClearPlayer` 调 `OnPlayerGone` | `DestroyPlayer` 里已没有 `ClearPlayer`。`OnPlayerGone` 放在末尾、`playerList.erase` 之后。C2 落码时另有「playerList 里已没有该玩家号才调」的同号替身检查；截至本日工作区，ECS 评审相关的未提交改动已把它改成无条件调用。影响：离场通知里若重建了同号实体，无在途票据时条目会被回收，可能多计一次 `orphan_unsynced`；新实体没有落盘快照，首存必然整份写，MySQL 结论不变 |
| 6 | `Ledger::FinishSave(allQueued=false)` 只是不更新 `lastFullyQueued` | 同时把这一代记为失败（防御，结论不变）；`Clear` 不重置代号与票据计数器 |
| 7 | `SendDbTask` 只有 sendTracked 一条路 | 监听器未登记时退化为不跟踪的 `send()`（§A.5） |
| 8 | 单测 `ExpiredTicketDuringPumpCountsAndEnqueues` 断言 `ticket_expired` 增量 1 | 用例发了两张票据，断言增量 2；另增 `DbTaskDispatchStats.AlarmingIncreaseIgnoresSupersededFailures`；拒存用例已改名为 `DbTaskDispatchEcs.ExhaustedRekickWithMissingComponentDiscardsWithoutReleaseMark` |
| 9 | `kafka_producer.h` 类注释引用本文 | 改为指向 `db_task_dispatch_system.h`（C2 落码时本文还不存在） |
| 10 | B-07 结论「同 epoch、更大 offset 的旧载荷没有任何守卫能挡」 | go/db 之后已有 SQL 行围栏，能挡带版本的旧字节；兼容窗口（epoch 0）仍无守卫。S5 的不变量不变（§A.10） |

---

## §B 保留期与 Kafka 盘预算（S5b）

### B.1 db_task topic 的建立方与保留配置的来源

**建立方只有 Go**：go/db 的 `db.go` 与 go/login 的 `login.go` 每次启动都调 `kafkautil.EnsureTopics`，传入 `TopicSpec{Name, Partitions, RetentionMs, RetentionBytes}`（`go/shared/kafkautil/topic_init.go`）。C++ 不显式建 topic。

`TopicSpec.RetentionBytes` 是每分区的 topic 级 `retention.bytes`：

| 取值 | 行为 |
|---|---|
| 0 | 不管理，沿用 broker 的 `log.retention.bytes` |
| > 0 | 建 topic 时放进 `ConfigEntries`；每次启动在同一次 `IncrementalAlterConfig` 里 SET |
| < 0 | `ensureTopics` 在建 topic 和改配置之前直接返回错误 `kafka topic %s has invalid retention.bytes %d (0 = unmanaged, > 0 = bytes per partition)`：Kafka 里 -1 表示不限，会让磁盘无界。go/db 会因此 panic（`Kafka db-task partition contract rejected`） |

`ensureTopics` 把要管理的项组装成一个 entries map（`retention.ms` > 0、`retention.bytes` > 0，以及 `RequireDurability` 的两项），map 非空才调一次 `IncrementalAlterConfig`，日志为 `kafka topic %s retention updated: retention.ms=%d retention.bytes=%d`。必须用增量接口：sarama 的 `AlterConfig` 走遗留协议，会全量替换该 topic 的动态配置，把运维手工设的其它覆盖项抹掉。

**配置键**（均为 optional）：

| 服务 | 键 | 缺省 |
|---|---|---|
| go/db | `ServerConfig.Kafka.RetentionMs` / `ServerConfig.Kafka.RetentionBytes`（`go/db/internal/config/config.go`） | 86400000 / 0 |
| go/login | `Kafka.RetentionMs` / `Kafka.RetentionBytes`（`go/login/internal/config/config.go`） | 86400000 / 0 |

本机的 `go/db/etc/db.yaml`、`go/login/etc/login.yaml` 只加了一行注释示例 `# RetentionBytes: 0`，本机行为不变（沿用 broker 缺省，docker-compose 的 broker 缺省是 128MiB）。

**K8s 渲染**：`tools/scripts/k8s_deploy.ps1` 的 `New-GoSvcConfigMapYaml` 用同一对变量渲染 db 与 login 两个 ConfigMap（`go-svc-db-config` / `go-svc-login-config`）的 Kafka 段：`RetentionMs: ${dbTaskRetentionMs}`（来自 `Apply-KafkaProfileDefaults` 的 `$KafkaDbTaskRetentionMs`）、`RetentionBytes: ${dbTaskRetentionBytes}`（来自 `$script:KafkaDiskPlan.DbTaskRetentionBytes`，缓存为空时现场 `Resolve-KafkaDiskPlan` 并 `Assert-KafkaStorageChangeIsSafe`）。

这修掉了既有缺陷 B-15：以前 db 的 ConfigMap 不写 `RetentionMs`（回落到 Go 缺省 24h），login 写的是档位值，两个服务启动时对同一个 topic 做 IncrementalAlterConfigs，dev / prod-like 档的 retention.ms 随重启顺序在 1h / 6h 与 24h 之间横跳。现在所有档位两边同值，契约测试断言两个 ConfigMap 相等。

db_task 的 retention.ms 档位值：dev 3600000（1h）、prod-like 21600000（6h）、prod 86400000（24h）。topic 名是 `db_task_zone_<zone>`，世代号 ≥ 2 时带 `_g<N>`（C++ `GetDbTaskTopic`，脚本 `tools/scripts/lib/kafka_consumer_groups.ps1` 的 `Get-DbTaskTopicName`）。

### B.2 最短缓冲时长 T_buf 的定义（规格 B-18）

godb 交接 §2.3 承诺提供「回档 / 重放需要的最短缓冲时长」，此前没写出来。本文定义如下。

**T_buf** = go/db 完全停止消费时，db_task 的积压能在 Kafka 里留多久而不被删除。它由两个上限中先到的那个决定：

```text
T_buf = min(retention.ms, 按 retention.bytes 能容纳的时长)
按 retention.bytes 能容纳的时长 ≈ P × (retention.bytes − seg) ÷ (R × 1.2)      # 账本行的 implied_db_task_buffer_min
```

规格 B-18 的要求口径是：T_buf 的目标 ≥ max(能容忍的 DB 故障时长, 回档 Step 5 把位点拨回 TargetTime 需要的窗口)，缺省目标 2h（全局单点交接要求 db_task 缓冲 ≥ 2h）。

代码的落法：

- 目标写成参数 `-KafkaDbTaskBufferTargetMs`，档位缺省 dev 1h、prod-like / prod 2h；
- retention.bytes 按 `T = min(BufferTargetMs, retention.ms)` 算（§B.3）：留得比 retention.ms 还久没有意义，只会白占盘；
- 账本行打出按设计速率估算的实际缓冲 `implied_db_task_buffer_min`。它短于 retention.ms 时打 `[KafkaDiskLedger]BufferBelowRetention:` 告警（只告警，不阻断）。

**与规格的差异（以代码为准）**：规格把「能容忍的 DB 故障时长」写成等于 retention.ms。按代码，prod / prod-like 档的 retention.bytes 按 2h 算，会先于 24h / 6h 的 retention.ms 生效。所以按设计速率，能容忍的 DB 故障时长约 2 小时（prod ≈ 121 分钟、prod-like ≈ 124 分钟、dev ≈ 71 分钟），不是 retention.ms。prod 档每次部署都会出现 BufferBelowRetention 告警，属于预期。

**使用约束**：

- 回档（`tools/scripts/k8s_zone_rollback.ps1` 的 Step 5 把 zone 消费组拨回 `-TargetTime`）的 TargetTime 不能早于「现在 − T_buf」，否则 Kafka 里已经删掉的那段重放不全；
- T_buf 按设计速率估算，不是承诺，实际速率高于设计值时更短；FitLimited 时会短得多（§B.4 的例子约 10 分钟）。

### B.3 盘预算公式与缺省值

纯函数在 `k8s_deploy.ps1`：`Get-KafkaDiskBudgetConstants`（常数唯一出处）、`Get-KafkaDbTaskRetentionBytesTarget`、`Get-KafkaDiskPlan`、`Get-KafkaStorageChangeVerdict`。它们不读脚本参数、不碰集群，契约测试按 AST 抽出来直接喂值。常数：倾斜余量 `SkewFactor = 1.2`、盘余量 `Headroom = 1.3`、retention.bytes 取整粒度 `RetentionGranuleBytes = 64MiB`、PVC 下限 `MinStorageGi = 20` 与步长 `StorageStepGi = 10`、每分区至少 `MinSegmentsPerPartition = 2` 个 segment。

记 R = 每 zone 的 db_task 设计字节率，P = 每 zone 的 db_task 分区数（`go/db/etc/db.yaml` 的 `ServerConfig.Kafka.PartitionCnt`，现为 10），seg = segment 字节数，Z = zone 数，A = 审计 topic 分区数（`go/data_service/etc/data_service.yaml` 的 `Kafka.TransactionLogPartitions` + `Kafka.SnapshotPartitions`，现为 6 + 3 = 9，没有自己的 retention.bytes，按 broker 缺省记），reserve = 其它 topic 预留（GiB），Gi = 1073741824：

```text
target      = roundUp(R × min(BufferTargetMs, retention.ms) ÷ 1000 × 1.2 ÷ P, 64MiB) + seg
required    = 1.3 × (Z × P × (target + seg) + A × (broker 缺省 retention.bytes + seg) + reserve × Gi)
RequiredGi  = max(20, ceil(required ÷ Gi ÷ 10) × 10)
capacity    = (EffectiveGi × Gi ÷ 1.3 − A × (broker 缺省 + seg) − reserve × Gi) ÷ (Z × P) − seg
生效值      = target ≤ capacity ? target : floor(capacity, 64MiB)          # 后者 FitLimited = true
floor(capacity, 64MiB) < 2 × seg 时 throw(「放不下」,附所需 Gi)
```

**每个 broker 的份额取上界**，公式里不乘 RF ÷ broker 数：一个 broker 对同一分区至多持有一份副本（RF ≤ broker 数），所以它的落盘不超过单份数据的总量。均摊只在副本已在各 broker 间分匀时才成立。3 → N 扩容是放行的，而新 broker 不会自动接手已有分区，`kafka-reassign-partitions.sh` 之前老 broker 仍持有全部分区；按均摊算会把收缩后的 retention.bytes 抬高 N/3 倍、写爆老 broker。单 broker（RF 1）与 3 broker（RF 3）的结果与均摊相同，≥ 4 个 broker 时偏保守。broker 数与 RF 仍校验（RF ≤ broker 数），并打进账本行。

**输入来源**（`Resolve-KafkaDiskPlan`）：

- broker 数 / RF 取 `Get-KafkaTopology`；P 与 A 用 `Get-AuthoritativeScalar` 从仓库 yaml 读，不是正整数即 throw；
- `-KafkaDbTaskRetentionBytes` 为 0 时按 `Get-KafkaDbTaskRetentionBytesTarget` 算 target，> 0 时用显式值（仍会按盘收缩）。负数，或 > 0 却小于 2 个 segment，都在这里拒绝；
- Z = zones 配置 ∪ `-ZoneId`；非 DryRun 时再并入线上名字以 `-NamespacePrefix-` 开头、既不在 zones 配置里也不是 `-ZoneName` 的 namespace（`Get-LiveZoneNamespaceNames`），每个算一个 zone；
- zones 配置缺席时：`-ReleaseProfile dev` 只告警（`[KafkaDiskLedger]ZonesConfigMissing:`），**其它档直接拒绝部署**（Z 不可信）。

**缺省值**（`Apply-KafkaProfileDefaults`，参数 0 = 按档位；下表 RequiredGi 按 2 个 zone、仓库当前 P = 10、A = 9 手算，未运行）：

| `-KafkaProfile` | 设计速率 | 缓冲目标 | retention.ms | segment | broker 缺省 retention.bytes | 其它预留 | target retention.bytes | RequiredGi | 按设计速率的缓冲 |
|---|---|---|---|---|---|---|---|---|---|
| dev | 262144（256 KiB/s） | 3600000（1h） | 1h | 16MiB | 128MiB | 1 GiB | 150994944（144MiB） | 20（公式 ≈ 7.0，取下限） | ≈ 71 分钟 |
| prod-like | 1048576（1 MiB/s） | 7200000（2h） | 6h | 32MiB | 512MiB | 2 GiB | 973078528（928MiB） | 40（≈ 33.2） | ≈ 124 分钟 |
| prod（缺省） | 3145728（3 MiB/s） | 7200000（2h） | 24h | 32MiB | 512MiB | 4 GiB | 2785017856（2656MiB） | 80（≈ 79.7） | ≈ 121 分钟 |

新参数（0 都表示按档位 / 自动）：`-KafkaDbTaskRetentionBytes`、`-KafkaDbTaskDesignBytesPerSec`、`-KafkaDbTaskBufferTargetMs`、`-KafkaStorageGi`（`ValidateRange(0, 65536)`）、`-KafkaOtherTopicsReserveGi`（`ValidateRange(0, 4096)`）。

### B.4 按盘收缩与线上盘的读取

**实际用的盘 EffectiveGi**（账本行 `pvc_source`）：

| 来源 | 条件 | EffectiveGi | 渲染进 kafka.yaml 的 `StorageTemplateGi` |
|---|---|---|---|
| `explicit` | 显式 `-KafkaStorageGi > 0` | 显式值 | 显式值 |
| `live` | 线上有 StatefulSet `kafka` | min(线上模板, 最小的实际 PVC) | 线上模板（不可变字段，不同即 apply 被拒） |
| `live-pvc` | StatefulSet 不在、PVC 在（扩盘 runbook 的 `--cascade=orphan` 之后） | 最小的实际 PVC | 最小的实际 PVC |
| `computed` | 都没有（全新集群、DryRun） | RequiredGi | RequiredGi |

- 线上模板：`Get-LiveKafkaStorageGi`，读 `kubectl get statefulset kafka` 的 `volumeClaimTemplates[0]` 大小。
- 实际 PVC：`Get-LiveKafkaPvcMinGi`，只认名字恰好是 `kafka-data-kafka-<序号>` 的 PVC，读 `status.capacity`（扩容做完才更新），还没绑定的取 `spec.resources.requests`。
- 两者都只认 `<整数>Gi`；别的单位、查询失败、格式不对一律 throw（fail-closed）。DryRun 不查集群，一律按「线上没有」算。
- 只看模板会按一块并不存在的盘算（某块 PVC 没扩成、停在 `FileSystemResizePending`，或 orphan 之后不带参数重跑），所以取两者中小的。

**按盘收缩（FitLimited）**：盘装不下 target 时，生效值 = floor(capacity, 64MiB)，只告警、不阻断（负责人 2026-10-13 拍板）；连 2 个 segment 都放不下才 throw。只在放不下时才按 64MiB 取整：target 本身是「64MiB 整数倍 + seg」，一律取整会把公式盘也截掉一个粒度，与缺省值表对不上。

例：prod 档、线上 20Gi、2 个 zone → 生效值从 2785017856 收缩到 268435456（256MiB），按设计速率约 622 秒（约 10 分钟）。

`[KafkaDiskLedger]FitLimited:` 告警写明：收缩后的值低于 topic 现值时（例如 broker 缺省 512MiB → 256MiB），db / login 滚动重启时 `EnsureTopics` 立即下调 topic 级 retention.bytes，Kafka 在下一次 retention 检查（`retention.check.interval` 2 分钟）内删掉超出部分的最旧积压，不管 db 读没读到。**发布前确认 db 每个分区的消费 lag ≈ 0**，或者先扩盘。

**盘变更判定**（`Get-KafkaStorageChangeVerdict`，由 `Assert-KafkaStorageChangeIsSafe` 在写操作之前执行）：

| 判定 | 条件 | 结果 |
|---|---|---|
| `absent` | 线上模板与 PVC 都没有 | 放行 |
| `auto` | 没给 `-KafkaStorageGi` | 放行 |
| `unchanged` | 显式值 = 线上模板且不大于最小 PVC；StatefulSet 不在时显式值 = 最小 PVC | 放行 |
| `expand-refused` | StatefulSet 在、显式值 > 模板 | throw，附扩盘 runbook |
| `shrink-refused` | 显式值 < 模板；或 StatefulSet 不在、显式值 < 最小 PVC | throw「PVC 不能缩」 |
| `pvc-not-expanded` | 显式值 > 最小的实际 PVC | throw：某块盘还没扩到这么大，按它算会写爆那块盘 |

**写路径门禁**：`k8s_deploy.ps1` 的写操作门禁块（`zone-up` / `all-up` / `infra-up`）在 `Write-MysqlConnectionLedger`、`Write-RedisConnectionLedger` 之后依次调 `Resolve-KafkaDiskPlan`、`Write-KafkaDiskLedger`、`Assert-KafkaStorageChangeIsSafe`，全部早于任何集群写操作。zone-up 也做存储变更检查：它同样按这块盘算 retention.bytes，按一块线上并不存在的盘算出来的值会写爆真实的盘。`Apply-Infra` 的 kafka.yaml 分支把 `__KAFKA_STORAGE_SIZE__` 替换为 `StorageTemplateGi`（缓存为空时现场 Resolve 并判盘变更，算不出即 throw，不回落任何默认尺寸）。

**账本行**以 `Kafka disk ledger:` 开头，后面跟一行 `Kafka disk ledger note:`，各字段含义见 README。告警段落的开头不留空格：`[KafkaDiskLedger]FitLimited:`、`[KafkaDiskLedger]BufferBelowRetention:`、`[KafkaDiskLedger]ZonesConfigMissing:`、`[KafkaDiskLedger]LiveZonesNotInConfig:`。

### B.5 扩盘 runbook

见 `deploy/k8s/README.md`「Kafka 磁盘预算与 db_task retention.bytes」的「扩盘 runbook」与「已存在的 topic 立即改配置 / 回滚」两小节。要点：StatefulSet 的 `volumeClaimTemplates` 不可变，扩盘不能靠 apply。步骤：

1. 确认 StorageClass 的 `allowVolumeExpansion` 为 true；
2. 逐个 patch PVC `kafka-data-kafka-<i>`，确认 CAPACITY 生效；需要文件系统扩容时逐个重启 broker，并确认 under-replicated 分区为 0；
3. `kubectl -n <infra> delete statefulset kafka --cascade=orphan`；
4. 带 `-KafkaStorageGi <N>` 重跑 infra-up；
5. 对**每个** zone 重跑 zone-up。

exec 进 broker 跑 Kafka CLI 时必须把 `KAFKA_HEAP_OPTS` 覆盖成小堆（`-Xms16m -Xmx64m`），否则工具继承 broker 的大堆，可能把 kafka-0 撑到 OOMKilled。不想为改保留期滚动 go/db 时（发版红线见 `db-task-pipeline-bottlenecks.md` §4.5），可以用 `kafka-configs.sh --alter` 立即生效，ConfigMap 等下次正常发布再收敛。顺序是先扩盘再调大；调小立即生效。

### B.6 「一节点一 topic」的定位

「一节点一 topic」指旧的消费路径：每个 gate / scene 节点一个 `gate-<node_id>` / `scene-<node_id>` topic，另加一个消费组（`cpp/libs/engine/core/node/system/node/node_kafka_command_handler.h`），开关是 `Kafka.DisableLegacyPerNodeTopic`（`cpp/libs/engine/config/config.cpp` 读取）。生产者已全部切走。

规格 B-13 / D5 定为本轮不翻：它与全局单点 P0-B 的 GroupID 模板改动耦合，翻之前要逐环境确认旧 topic 的写入与积压为 0。截至 2026-10-16 工作区，K8s 的 node ConfigMap 已由全局单点 P0-B 写成 `DisableLegacyPerNodeTopic: true`（`k8s_deploy.ps1` 的模板常数，契约测试断言字面值 true）；本机 `bin/etc/base_deploy_config.yaml` 仍是 `false`。这与规格「K8s 不写这个键 = false」的现状描述不同，以代码为准，但不是 S5 的改动。已经 auto-create 出来的旧 topic 在 broker 缺省保留期到期前仍占盘，由 `-KafkaOtherTopicsReserveGi` 预留覆盖。

### B.7 行为变化

1. db 与 login 的 ConfigMap 都带 `RetentionMs` 与 `RetentionBytes`，两边同值；db_task 的 retention.ms 不再随重启顺序横跳。
2. db_task 有了 topic 级 `retention.bytes`。现网小盘集群按盘收缩时，缓冲比以前（broker 缺省 512MiB）更短，下调那一刻会删掉超出部分的未消费积压。
3. kafka.yaml 的 PVC 不再写死 `20Gi`：新集群按公式建盘（prod 档 2 个 zone 为 80Gi）；已有集群沿用线上值。
4. 非 dev 的 `-ReleaseProfile` 缺 zones 配置时拒绝部署（含 infra-up：它也按 Z 定 PVC）。
5. zone-up 也会因 `-KafkaStorageGi` 与线上不符被拒。
6. prod / prod-like 档每次部署都打 `BufferBelowRetention` 告警（§B.2）。
7. kind 本机缺省 `-KafkaProfile prod` 会申请 80Gi PVC；local-path 不强制容量，但 retention 上限约是以前的 5 倍，高写入压测可能写满宿主盘。本机建议显式传 `-KafkaProfile dev`。

### B.8 残余风险与后续项

1. **F9 未核实**：旧版二进制（没有 `RetentionBytes` 字段）读到 ConfigMap 里的新键会忽略还是报错。go/db 依赖 go-zero v1.10.0，go/login 依赖 v1.9.2（各自 go.mod）。若会报错，S5b 回滚必须连同旧的 ConfigMap 生成器一起回退。列为 Codex 验证项。
2. zone-down 删的是 namespace，那个 zone 的 db_task topic 仍按旧的 retention.bytes 留着数据，直到 retention.ms 到期（prod 24h），盘预算数不到它。期间部署别的 zone 时按超量承诺对待，用 `--alter` 把残留 topic 调小。
3. 加 zone 之后，新部署的 zone 按新的 Z 拿到更小的 retention.bytes，先部署的 zone 在下次 zone-up 之前保留旧值，期间超量承诺。要对全部 zone 重跑 zone-up。
4. lag 秒级告警没做（B-12）：积压逼近 T_buf 时没有主动告警，Kafka 的删除是静默的。`implied_db_task_buffer_min` 是以后定告警阈值的依据。
5. 其它 topic 的预留是固定值，没有按 broker 缺省取最坏值；到大规模时（控制面约 4.8 GB/h 每 broker）要调大预留。审计 topic 仍按 broker 缺省记，保留策略待 G2a 拍板；G2a 换代期间新旧审计 topic 并存，会临时占双倍。
6. 设计速率是推算值，账本行的缓冲只是估计。
7. 既有风险，本轮未修：scene 先于 db / login 发 DBTask 时，broker 会把 db_task 自动建成 1 个分区（kafka.yaml 没有关掉 auto.create），db 启动时分区契约 panic（B-22，建议另案：zone-up 预建 db_task 或关 auto.create）；生产端没开压缩，`message.max.bytes` 三处没对齐（B-14）。

### B.9 与规格不一致处（以代码为准）

| # | 规格 | 截至 2026-10-16 工作区代码 |
|---|---|---|
| 1 | required = 1.3 × RF × Σ ÷ Brokers | 每个 broker 份额取上界：required = 1.3 × Σ，不乘 RF ÷ Brokers（R5a 复核更正，§B.3） |
| 2 | 线上盘 = StatefulSet 模板；判定 absent / auto / unchanged / expand-refused / shrink-refused | 线上盘 = min(模板, 各 PVC `status.capacity` 最小值)；新增来源 `live-pvc` 与判定 `pvc-not-expanded`；kafka.yaml 渲染 `StorageTemplateGi`（StatefulSet 在时等于线上模板） |
| 3 | 只有 infra-up / all-up 调 `Assert-KafkaStorageChangeIsSafe` | zone-up / all-up / infra-up 都调 |
| 4 | Z = `@(Get-InfraZoneIds).Count` | Z = zones 配置 ∪ `-ZoneId` ∪ 线上带前缀的 zone namespace；不调 `Get-InfraZoneIds`；非 dev 缺 zones 配置时拒绝 |
| 5 | 只拒绝 `-KafkaDbTaskRetentionBytes < 0` | 另拒绝「> 0 但小于 2 个 segment」 |
| 6 | 生效值一律 min(target, floor64MiB(fit)) | 只在 target > capacity 时才按 64MiB 向下取整，否则用 target 原值 |
| 7 | 纯函数三个、胶水四个 | 另有 `Get-KafkaDiskBudgetConstants`（常数唯一出处）、`Get-LiveKafkaPvcMinGi`、`Get-LiveZoneNamespaceNames` |
| 8 | `Get-LiveKafkaStorageGi` 的 jsonpath 只取 storage | 取 `{.metadata.name}:{...storage}`，以区分「StatefulSet 不存在」与「存在但没有模板」 |
| 9 | FitLimited 告警只写扩盘 runbook | 另提醒下调 retention.bytes 会删未消费积压，发布前确认 lag ≈ 0 |
| 10 | T_buf 中「能容忍的 DB 故障时长 = retention.ms」 | 按代码，prod / prod-like 档由 retention.bytes 决定，约 2h（§B.2） |
| 11 | K8s node ConfigMap 不写 `DisableLegacyPerNodeTopic` | 已由全局单点 P0-B 写成 true（不是 S5 的改动，§B.6） |

---

## 上线与回滚

**S5a（C++ scene）**：

- 部署：scene 节点滚动升级即可。不改 proto、配置、库表，与 go/db 之间的消息格式不变，新旧版本可以并存。
- 观察 30s 一行的 `[DbTaskDispatch]`：`exit_await_ack` 随下线增长属正常；稳态下 `produce_rejected`、`delivery_failed`（扣除 `delivery_failed_superseded`）、`ticket_expired`、`exit_resave_capped`、`exit_unsynced`、`orphan_unsynced` 都应为 0。停机 drain 的耗时应基本不变（Kafka 正常时只多一个回执往返）。
- 回滚：换回旧的 scene 镜像。账本只在内存里，没有持久化状态要处理。

**S5b（Go + 部署）**，按序：

1. 合入 Go 改动（`TopicSpec.RetentionBytes` 与两个服务的 `KafkaConfig.RetentionBytes`），构建 db / login 镜像。Go 侧必须与 k8s_deploy 渲染新键同批或先合入（F9 未核实前，旧二进制读到新键的行为未知）。
2. 逐 zone 跑 zone-up：新 ConfigMap 带 `RetentionMs` / `RetentionBytes`，db / login 重启时经 `IncrementalAlterConfigs` 生效。先看账本行与告警：FitLimited 且生效值低于 topic 现值时，先确认 db 消费 lag ≈ 0 再发布，或先扩盘。不想为改保留期滚动 go/db 时用 README 的 `kafka-configs.sh --alter`。
3. infra-up：已有集群沿用线上 PVC，盘低于预算时 retention.bytes 按盘收缩并 WARN（例：prod 档、20Gi、2 个 zone 时从 broker 缺省 512MiB 降到 256MiB，约 10 分钟缓冲）；要扩盘按 runbook 先 patch PVC、`--cascade=orphan`，再带 `-KafkaStorageGi N` 重跑 infra-up；新集群直接按公式建盘。
4. 加 zone 或扩盘之后，对全部 zone 重跑 zone-up。

回滚：topic 级配置在回滚后仍留在 topic 上，本身无害；要撤用 `kafka-configs.sh ... --alter --delete-config retention.bytes`（命令见 README）。旧二进制遇到 ConfigMap 里的 `RetentionBytes` 若会报错（F9），回滚必须连同旧的 ConfigMap 生成器一起回退。

**Java 版（AGENTS §12）**：两项都不适用：Java 版由 scene 同步写 MySQL，在线存盘失败已作废比对基准（Java 仓 `PARITY.md` 既有条目），没有 db_task topic。Java 自己 Kafka 的保留与盘预算登记为「待自查」。按本批分工，`PARITY.md` 本批不改，下一批统一登记。

---

## 验证（交 Codex）

仓库根 `D:\luyuan\xuanming-server-mmo-cpp`，PowerShell 7 + VS 开发者环境。前一步红了就停下归因，不连续重试。报错落在其它会话的未提交文件里（scene 库下大量 M 状态文件、`player_database_loader.cpp`、`generated/*`、`muduo_grpc/client_session.cpp` 等）时单独记录，不算本项的问题。

**C++**（MSBuild 必须串行 `/m:1`；主干整体编不过，不以 game.sln 为通过标准）：

```powershell
msbuild cpp\libs\engine\infra\infra.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
msbuild cpp\libs\services\scene\scene.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
msbuild cpp\tests\kafka_command_test\kafka_command_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
msbuild cpp\tests\cross_zone_test\cross_zone_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
msbuild cpp\nodes\scene\scene.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64   # 核对链接无 unresolved external
pwsh tools/scripts/run_cpp_tests.ps1 -Filter kafka_command
pwsh tools/scripts/run_cpp_tests.ps1 -Filter cross_zone
# 单独跑新用例:
.\build\cpp\tests\kafka_command_test.exe --gtest_filter=DeliveryTracker.*
.\build\cpp\tests\cross_zone_test.exe --gtest_filter=DbTaskDispatch*:ExitDbTaskGate*:ExitPersist*
# 回归(同样链接 scene.lib,走退出 / 存盘路径):
msbuild cpp\tests\bag_test\bag_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
msbuild cpp\tests\currency_test\currency_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
pwsh tools/scripts/run_cpp_tests.ps1 -Filter bag
pwsh tools/scripts/run_cpp_tests.ps1 -Filter currency
```

- 通过标准：各工程 0 error；`DeliveryTracker.*`（5 例）、`DbTaskDispatchLedger.*`（12 例）、`DbTaskDispatchStats.*`（1 例）、`ExitDbTaskGate.*`（3 例）、`DbTaskDispatchEcs.*`（9 例）全绿；既有 `ExitPersist*` 与 bag / currency 用例与改动前相比没有新增失败。
- 失败时保留：msbuild 第一条 error 的前后 20 行、gtest 失败用例全文。

**Go**（环境变量 `GOTOOLCHAIN=local`、`GOFLAGS=-mod=readonly`、`GOPROXY=https://goproxy.cn,direct`；不改 go.mod / go.sum）：

```powershell
cd go/shared; gofmt -l kafkautil; go vet ./kafkautil/; go test ./kafkautil/ -run 'TestEnsureTopics|TestPartitionContract|TestParseReplicationFactor' -count=1 -v
cd go/db;     gofmt -l internal/config; go vet ./...; go build ./...
cd go/login;  gofmt -l internal/config; go vet ./...; go build ./...
```

- 新用例：`TestEnsureTopicsSetsRetentionBytesOnCreateAndAlter`、`TestEnsureTopicsLeavesRetentionBytesUnmanagedWhenZero`、`TestEnsureTopicsRejectsNegativeRetentionBytes`、`TestEnsureTopicsRetentionBytesWithoutRetentionMs`。
- 通过标准：gofmt 无输出；vet / build / test 退出码 0。
- F9：用一个最小用例分别在 go-zero v1.10.0（go/db）与 v1.9.2（go/login）下核实 `conf.MustLoad` 遇到结构体里没有的键 `RetentionBytes` 时是忽略还是报错，结论写进上线待做。

**PowerShell**：

```powershell
pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
```

- 通过标准：全部 PASS，含末尾「S5b 保留期与 Kafka 盘预算」一节：各档缺省值（target 150994944 / 973078528 / 2785017856，2 个 zone 的 RequiredGi 20 / 40 / 80）、prod 线上 20Gi 时生效值 268435456 且缓冲约 622 秒、放不下时 throw、判定表、5 broker 取上界、模板与实际 PVC 取小 / `live-pvc` / `pvc-not-expanded`、线上 zone namespace 并入 Z、zones 配置缺席的告警与非 dev 拒绝、两个 ConfigMap 的 `RetentionMs` / `RetentionBytes` 相等、账本行、kafka 清单的 `storage:` 无残留占位、负向参数。

**文档**：`python tools/scripts/gen_docs_index.py --check`。本批不改 `docs/README.md`，在下一批登记本文之前，`--check` 预期因索引过期返回 1；登记后应返回 0。

**故障注入**（登记进上线待做，本轮不执行）：

1. 本机 docker-compose：一个玩家下线时 `docker pause kafka` 6 分钟。预期：实体被保留（`IsSaveInFlight` 为真），约 300s 后回执失败触发闸门重存；`docker unpause kafka` 后实体销毁，MySQL 两张玩家表的行与 Redis 一致；`[DbTaskDispatch]` 的 `exit_await_ack`、`exit_resave`、`delivery_failed` 有增量，`exit_unsynced` / `orphan_unsynced` 为 0。
2. kind：`pwsh -File tools/scripts/k8s_deploy.ps1 -Command infra-up -DryRun -KafkaProfile dev`（以及缺省 prod 档）核对 `Kafka disk ledger:` 一行与渲染出的 `storage: 20Gi` / `80Gi`。
3. 停机看门狗：Kafka 不可用时停 scene 节点，预期 15s 后打 `Before-shutdown drain budget exceeded`，随后 `[DbTaskDispatch] shutting down with N player(s) ...` 列出未落定的玩家号。
