# S4 实现规格：S4：G1 治标（ctx 只随失租取消 / 缓存回写挪到核锁之后 / 续租有界退避 / 释放失败计数告警）+ go/db 停机顺序（先撤拉取再等 worker，minReadySeconds）+ go_services.ps1 优雅停（Ctrl+C → 超时强杀进程树）+ 测量打点 + 过时注释订正 + Lua tonumber + L-04 待补 11 条单测

## 摘要
本规格只读产出，没有编译和测试，等 Codex 验证。共改 28 个文件：新建 9 个，修改 19 个。不改 proto、DDL、Redis 键名、Kafka 组名，也不改 dbOpHandler 签名。

1. G1 治标。handleDBWriteOp 和 handleDBReadOp 改用 store.WithContext(ctx)，proto2mysql v0.2.0 已有这个方法，模块缓存 proto2mysql.go:378。
   - 写 SQL 的 ctx 来自排序锁租约，只在失租时取消；claim 被收回、进程停机都不取消正在跑的写 SQL。原因：取消后锁会随任务一起释放，而服务端可能还在执行这条语句，它可能在别的实例写入新版本之后才提交，造成回档。附录 ④ 修正 5 已确认现状没有这条路径，治标不能把它造出来。
   - SQL 在途时因失租被取消的，释放阶段不删锁，留给 TTL 过期。
   - 核锁（verify）挪进 runDBOp，放在「复核之后、缓存回写之前」。现状是缓存先写、后核锁，③ 还没满足。
   - 续租改成在本地单调时钟截止时间内退避重试，同时设次数上限和时间上限。
   - 释放结果按 released / not_owner / error / skipped_sql_abandoned 计数，not_owner 和失租各配一条告警。
   - 这些都不是栅栏：已经进入 MySQL 的语句撤不回。
2. 停机。Stop 改为五步：①撤拉取（新增 pullStop）；②各 claim 把已分发的任务做完并标记位点，会话释放时最后一次提交；③LeaveGroup；④取消 worker；⑤归还重试收据。claimCtx 不再随「兄弟 claim 先排空导致的会话取消」一起取消。db.yaml 加 minReadySeconds: 20。
3. go_services.ps1。先用一个短命辅助进程做 AttachConsole + GenerateConsoleCtrlEvent 发 Ctrl+C，超时再用 taskkill /T /F 强杀进程树。db 默认等 35 秒，其它服务 25 秒。
4. 测量打点。
   - db_task_stage_seconds 新增 sql、task_total、queue_wait、kafka_e2e 四个 stage。
   - 新增 db_task_redis_step_seconds{op,step}，step 有 11 个固定取值，在调用点逐个计时，不挂 Hook。
   - 新增 db_ordering_guard_total、db_subshard_batch_size。
   - 新增 db_redis_pool_*，按指针判断两个 client 是否同一对象，同一对象只导出一份。
   - stress_summarize 新增两段输出，名字与脚本对齐。
5. luaMarkAppliedEpoch 改为十进制串比较：先比长度，再逐字节比。
6. 订正 REPLACE / ODKU / 一致性哈希等过时注释，保留「无主键会追加新行」的结论。
7. 补齐 launch-todo §2.3 的 11 条 L-04 单测。

Java 版不适用：Java 没有 Kafka 存盘链，需在 PARITY.md 登记。

## 现状
（按当前工作区，HEAD 6281e4d27。KOC 指 go/db/internal/kafka/key_ordered_consumer.go。）

**G1 治标现状**
- ① 业务池超时 L-04 已做：newServiceMysqlConfig（go/db/internal/logic/pkg/proto_sql/db.go:142-156），拨号/读/写超时 5/30/30 秒，会话 lock_wait_timeout 与 innodb_lock_wait_timeout 各 10 秒（go/db/internal/config/config.go:159-162、:326-340）。本项跳过，只补单测。
- ② handler 丢掉了 ctx：
  - handleDBReadOp 签名是 `_ context.Context`（KOC:343-353）；
  - handleDBWriteOp 同样是 `_ context.Context`，调用的是 `store.Save(msg)`（KOC:814-837，Save 在 :833）。
  - proto2mysql v0.2.0 已有 WithContext（模块缓存 proto2mysql.go:378-395），所有语句都经 conn() 绑定 ctx（:315-321），Save 是 UPDATE → INSERT → 1062 重试 → FOR UPDATE 归类（:3732-3805）。
  - go-sql-driver v1.9.3 取消 ctx 时只关连接、返回 ctx.Err()（connection.go:478-481，packets.go:55/97/150/372），不发 KILL QUERY。
  - storeerr.Classify 把 context.Canceled 归为 KindNone，把 context.DeadlineExceeded 归为 KindConnLost（go/db/internal/storeerr/storeerr.go，Classify 内）。所以给 SQL 用带 deadline 的 ctx，会误触发探活 / 冷却。
- ③ 尚未满足：
  - runDBOp（KOC:2067-2082）的顺序是 handler → reportStoreResult → route.confirm → publishDBOpResult。缓存 SET 在 publishDBWriteResult → writeBackDBCache 里（KOC:839-864，SET 在 :858）。
  - verify 指 orderingLease.verify（KOC:1910-1925，带 token 的 Lua Extend），它在 handleTask 的成功分支里调用（KOC:1662-1667），已经在缓存回写之后。
  - 随后是 markAppliedSeq（:1670）和 markAppliedOwnerEpoch（:1674）。
- ④ 排序锁续租：
  - TTL 2 分钟、续租间隔 30 秒（KOC:1875-1876）。
  - watchdog（KOC:1941-1964）续租出错或易主一次就 setLost 退出；w.ctx 取消也会 setLost（:1949-1951）。
  - 失租只在 verify 时被读到，不会取消 SQL。
- ⑤ 释放：
  - releaseOrderingLock（KOC:1971-1988）丢掉了 Release 的 bool 返回值（:1984）。
  - locker.Release 在锁已过期或已被接管时只打 `Release lock failed: lock expired or not owned` 并返回 (false,nil)（go/db/internal/locker/locker.go，Release 内）。
  - 没有任何计数，也没有告警。

**停机现状（G6）**
- Stop（KOC:2274-2283）的顺序是 c.cancel() → wg.Wait() → releaseRetryOwnership() → consumer.Close()。
- cancel 之后：
  - runSubShard 在 w.ctx.Done 时直接返回（KOC:1376-1378），子通道里已被 acker.track 的任务既不 ack 也不 fail；
  - ConsumeClaim 走 session canceled → drainAfterStop → waitClaimPending，因此必然等满 claimDrainTimeout=10 秒（KOC:289、:1736-1751）。
- claimCtx 由 session.Context() 派生（KOC:1732）。sarama v1.43.1 的行为：
  - 任一 ConsumeClaim 返回就取消整个会话（consumer_group.go:870-875）；
  - release 等所有 claim 返回后，经 offsets.Close 做最后一次 flush 提交（consumer_group.go:963-985、offset_manager.go:110-137）；
  - Close 只做 LeaveGroup，不等 Consume 返回（:157-183）；
  - 心跳持续到 release 为止（:1000-1060）。
- Consume 循环在 KOC:1040-1051，重试循环在 KOC:1058-1075、1155-1178、1182-1253（派发 select 在 :1246-1251）。
- db.yaml 没有 minReadySeconds（deploy/k8s/manifests/go-svc/db.yaml:22-40）；terminationGracePeriodSeconds 45，preStop 5 秒（:93-109）。
- go/db 主程序用自己的 signal.Notify(quit, SIGINT, SIGTERM)（go/db/db.go:244-246）。
  - Linux 上 go-zero v1.10.0 会在 5.5 秒后向自身重发 SIGTERM（core/proc/shutdown.go gracefulStop）。因为 quit 仍在注册，信号只会进 quit，不会杀进程。这一点没有注释。
  - Windows 上 go-zero 的 proc 是空实现（signals+polyfill.go / shutdown+polyfill.go）。

**G9 现状**
- tools/scripts/go_services.ps1 的 Invoke-Stop（:848-891）对每个实例 `Stop-Process -Force`（:879），等同崩溃。
- 启动用 Start-Process -WindowStyle Hidden 加重定向，没有 -NoNewWindow（:693-710）。go run 模式下记录的 PID 是 go.exe，真实服务是它的子进程，强杀 go.exe 不一定连带杀掉子进程。
- 全仓没有任何 GenerateConsoleCtrlEvent / AttachConsole 的现成实现（grep 结果）。服务端也没有停机端点。

**测量现状**
- 已有指标（go/db/internal/metrics/metrics.go）：
  - db_task_stage_seconds{op,stage}，stage 有 op_total / op_handler / cache_write / result_publish，桶 stageBuckets 在 :39-41；
  - db_task_result_total、owner_epoch_guard、placement_guard、L-04 四个指标、store_cooldown；
  - mmorpg_sql_pool_*（G2b）。
- 没有 Redis 分步计时、batch、游标守卫计数、go-redis PoolStats。
- stress_summarize.ps1 的 DB 段（:490-535）按精确 key 读 `db_task_stage_seconds_sum{op=..,stage=..}` 和 db_task_result_total。
- test_stress_summarize.ps1:234-247 断言全量输出里恰好有 1 行 t18m_db。新增段如果也打印 t18m_db 行，这条断言会失败，要改成按段定位。
- go/db 的 PlacementRedis 缺省就是 RedisClient 指针本身（go/db/internal/svc/servicecontext.go，NewServiceContext 内）。
- go-redis v9.18 的 PoolStats 字段：Hits / Misses / Timeouts / WaitCount / Unusable / WaitDurationNs / TotalConns / IdleConns / StaleConns / PendingRequests（internal/pool/pool.go:227-241）。

**Lua 现状**
- luaMarkAppliedEpoch（KOC:557-563）用 tonumber 做 double 比较。值大于 2^53 时，相邻整数会舍入成同一个数，更大的 epoch 推不进去。注释（:556）自认「远小于 2^53」。
- go/db 里另外几处 tonumber 都在 retry_ownership.go，比较的是 Redis TIME 的毫秒值（约 1.7e12，小于 2^53），不是隐患。

**过时说法**
- KOC:724-727、:1222-1224 写着「生产端一致性哈希 / 临时重映射」。
- KOC:851 写着 serial per key via Kafka partition。
- KOC:1310、:1413-1414 写着读后写屏障。
- placement_route.go:218 写着「整行 REPLACE」。
- proto_sql/store_registry.go:419-420 写着 `REPLACE INTO`。
- migrate/plan.go 有三处 ODKU：:176-178 的告警字符串、:249-251 和 :299-301 的注释。
- migrate/plan_test.go:214-215、:256-257、:266 也写着 ODKU。

**L-04 单测与告警**
- docs/ops/godb-pipeline-launch-todo.md §2.3 的 11 条单测都没写。
- deploy/k8s/db-task-retry-alerts.yaml 只有 L-04 的 5 条规则。

**并行会话**
- 工作区里有别的会话未提交的改动：docs/PROGRESS.md、docs/ops/godb-pipeline-launch-todo.md、tools/scripts/tests/k8s_deploy_contract.tests.ps1。

## 复核阻断项 / 修正的处理
- **H-G1-1**：交接 §4.1 第 2 行①：业务池补读写超时
  - 处理：已由 L-04 完成（proto_sql/db.go:142-156 newServiceMysqlConfig，默认值在 config.go:159-162），本项不改代码，只补 launch-todo §2.3 第 9 条单测（service_pool_test.go）。
- **H-G1-2**：handleDBWriteOp 接 ctx（store.WithContext(ctx)），ctx 随排序锁失租取消
  - 处理：读写 handler 都改为 store.WithContext(ctx)。写 SQL 的 ctx 来自 orderingLease.beginSQL()：它以 context.Background 为父，由 WithCancelCause 派生，只在 setLost 时取消。claim 收回、进程停机、w.ctx 取消都不取消写 SQL，避免制造「锁已释放、旧语句才提交」的回档路径。不用 WithDeadline，因为 DeadlineExceeded 会被 storeerr 归成 conn_lost，误触发冷却。beginSQL 发 SQL 前按本地单调时钟复核截止时间。SQL 在途时被取消的标记为 abandoned，释放时不删锁。
- **H-G1-3**：缓存回写挪到 verify 之后（先查清现状）
  - 处理：现状不满足：缓存 SET 在 runDBOp→publishDBWriteResult 里，verify 在 handleTask 里，排在其后。改法：runDBOp 新增 lease 参数，写任务的顺序固定为 beginSQL → handler → confirm → lease.verify → publishDBOpResult；handleTask 删除原 verify 块。读路径不变。回归用例 TestHandleTask_LeaseLostAfterSQLDoesNotPublishCache：旧代码下变红（缓存已写），新代码下变绿。
- **H-G1-4**：续租失败改为有上限的退避重试
  - 处理：watch() 在 validUntil−30s（本地单调时钟）的截止时间之内重试。退避首次 500ms、逐次翻倍、封顶 5s，带 ±20% 抖动；单次 Extend 预算 3s；每轮故障最多 10 次。用尽次数记 renew_exhausted，到截止时间记 renew_deadline，两种情况都 setLost 并取消 SQL；Extend 返回非持有者时立即记 owner_changed，不重试。时间、退避、抖动都可注入，单测不依赖墙钟。
- **H-G1-5**：对 Release lock failed: lock expired 加计数与告警（写进 db-task-retry-alerts.yaml）
  - 处理：新增 db_ordering_lock_release_total{outcome=released|not_owner|error|skipped_sql_abandoned}、db_ordering_lease_lost_total{reason}、db_ordering_lease_renew_retry_total，进程启动时把各取值建成 0。告警文件新增 group db-task.ordering-lease：DbOrderingLockReleasedNotOwned（critical，increase[10m]>0）和 DbOrderingLeaseLost（warning）。locker 原有日志原样保留，§4.5 的日志核对项仍然有效。
- **H-G1-6**：写清 G1 不是栅栏、撤不回已进 MySQL 的语句
  - 处理：写进三处：ordering_lease.go 头注释、告警 description、瓶颈文档 §7 新增行。说明：驱动取消只关连接、不发 KILL QUERY；owner_changed 时本实例的语句可能已在服务端执行；根治靠条件写（另一会话）。
- **H-42-1**：go/db 停机顺序：先撤拉取再等 worker；db.yaml 加 minReadySeconds（G6）
  - 处理：新增 pullStop / consumeDone / stopOnce / shutdownDrainTimeout。Stop 五步：撤拉取 → 等 Consume 循环退出（≤shutdownDrainTimeout+5s，期间会话释放并最后一次提交位点）→ consumer.Close 离组 → cancel + wg.Wait → 归还收据，仍然是先提交、后离组。ConsumeClaim 的停机分支不看会话取消；claimCtx 改由 WithoutCancel(session ctx) 派生，加一个观察 goroutine：只在非停机时随会话取消。db.yaml 加 minReadySeconds: 20，并更新 terminationGracePeriodSeconds 的预算注释；契约测试补断言。
- **H-42-2**：go_services.ps1 本机停 db 先优雅停、超时再强杀（G9），查清仓库有无现成办法并推荐可靠做法
  - 处理：仓库里没有现成实现。go-zero 的信号处理在 Windows 上是空实现；停机端点有暴露面，不采用。推荐做法：用当前宿主 exe 起一个隐藏的辅助进程，依次执行 FreeConsole → AttachConsole(pid) → SetConsoleCtrlHandler(NULL,TRUE) → GenerateConsoleCtrlEvent(CTRL_C_EVENT,0)。Go 侧收到的是 os.Interrupt；go run 模式下 go.exe 忽略中断，等子进程退出。流程分三段：先对全部目标发 Ctrl+C；再逐个等待（db 35s，其它 25s，-StopTimeoutSeconds 可覆盖）；超时或 Ctrl+C 发不出去时用 taskkill /PID /T /F 杀进程树。保留 -ForceStop 走旧行为。契约用例钉住启动参数不得含 -NoNewWindow：共享控制台会让 Ctrl+C 波及其它服务。
- **H-42-3**：过时说法订正：go/db 几处注释、store_registry.go 的 REPLACE INTO、plan.go 三处 ODKU；保留「无主键会追加新行」
  - 处理：Save 的实际语句统一改写为 UPDATE by PK → 0 行则 INSERT → 1062 再 UPDATE → FOR UPDATE 归类。无主键时 INSERT 不会撞 1062：内容未变（clientFoundRows=false 时 UPDATE 返回 0）或并发首写都会追加新行。plan.go:177 的告警保留「缺主键:」前缀，因为 plan_test.go:232 断言了这个子串。一致性哈希、读后写屏障两处改成在 ① 落地前后都成立的中性表述，① 落地后由 S3 再收口。ARCH.md 与 partition-contract 归 S3，本项不碰。
- **H-41-4**：测量打点：db_task_redis_step_seconds{step}（11 个固定取值）、stage=sql/task_total/queue_wait/kafka_e2e、db_ordering_guard_total、db_subshard_batch_size、go-redis PoolStats，名字与汇总脚本对上，subsystem 为 db
  - 处理：11 个 step：lock_acquire、cursor_get、epoch_get、placement_lookup、expand_get、placement_recheck、lease_verify、cache_set、cursor_set、epoch_mark、lock_release，对应附录 C / 瓶颈 §6.2。重试认领、LREM、30s 续租不计入，前两项只在重试源出现，续租另有 lease 指标。另加 op label（read/write，2 个值），用于区分读写。新 stage 沿用 db_task_stage_seconds 与 stageBuckets，不改桶；新直方图用新桶。stress_summarize 新增两段并配用例。
- **H-LUA**：Lua tonumber（go/db 一处）：找到并给修法与回归用例
  - 处理：出处是 luaMarkAppliedEpoch（KOC:557-563），属于 PROGRESS 7162 说的 double 精度问题，不是附录里的 nil 判断。改法：新增 luaDecimalGreaterFn（is_dec 先校验规范十进制，dec_gt 先比长度再逐字节比，不受 strcoll 影响）。参数或存量值不规范时返回 error_reply，调用方仍按失败只记日志处理。回归用例：存量 2^53，写入 2^53+1 应推进（旧脚本不推进，红）；MaxUint64−1 写入 MaxUint64 应推进；存量 10、写入 9 不降；存量 9、写入 10 推进；存量非法返回错误且值不变。scene_manager / data_service / match 三处 tonumber 不在本项。
- **L04-T1**：retryBudgetExempt 新分支：四类存储侧标记下写豁免、读不豁免；原有四种豁免不变；未打标记的 Redis net.Error 不豁免
  - 处理：l04_deferral_test.go TestRetryBudgetExempt_StoreTaggedErrors，表驱动，用 &storeerr.Error{Kind} 外加一层 fmt %w 包装。
- **L04-T2**：deferReason 全表，含 Redis 侧超时归 redis_unavailable
  - 处理：l04_deferral_test.go TestDeferReason_EveryReason：13 个取值逐一覆盖，并验证 storeerr 标记优先于 errPlacementStoreUnavailable、未打标记的 DeadlineExceeded 归 redis_unavailable。
- **L04-T3**：重试循环每拍上限：ready 里 3 条一拍只认领 3 次；两个消费者共用 miniredis 时各自至多 3 次
  - 处理：l04_deferral_test.go TestConsumeRetryQueue_ClaimsAtMostReadyLengthPerTick / TwoConsumersEachCapped。在 go-redis 上挂测试 Hook，统计 retryClaimScript 的 EVAL 次数，并在每次认领后用另一个 client 把载荷 LPUSH 回 ready，模拟立即失败。旧代码会认领 200 次（红），新代码恰好 3 次。
- **L04-T4**：主库冷却时整拍跳过，三个队列长度照常采样
  - 处理：TestConsumeRetryQueue_SkipsTickWhileHomeStoreCoolingDown：替身 coolingStores 实现 storeHealth，CoolingDown 恒为 true。断言认领次数为 0，db_retry_queue_length 三个取值正确（经 DefaultGatherer 读取）。
- **L04-T5**：读任务预算用完：不进死信，收据被摘掉，read_abandoned{budget_exhausted}+1
  - 处理：TestConsumeOneRetryTask_ExhaustedReadIsDroppedNotDeadLettered。
- **L04-T6**：写任务存储类延后：conn_lost 时重试次数不变，连续超过 3 轮仍在 ready，载荷逐字节不变
  - 处理：TestStoreDeferral_RetryWriteKeepsBudgetAndPayload：替换 dbOpHandlers["write"] 返回 KindConnLost，用 claimRetry + handleTask 循环 4 轮。
- **L04-T7**：计数：deferred / dead_letter 按原因递增，含毒消息经 persistPoisonMessage 计数
  - 处理：TestDeferAndDeadLetterCounters：分别比较 Gather 前后的差值。
- **L04-T8**：runDBOp 上报：conn_lost 调 ReportConnLost，成功调 ReportHealthy，复核或缓存回写失败都不调用
  - 处理：run_db_op_test.go TestRunDBOp_ReportsOnlySQLOutcome。口径以 store_failure.go 注释「只报 SQL 这一步」为准：复核失败、缓存失败、核锁失败都不调 ReportConnLost；SQL 本身成功时仍调一次 ReportHealthy。另加：SQL 被本进程取消时什么都不报。
- **L04-T9**：业务池配置：三个超时与两个会话变量；负数不设；NewMysqlConfig 不带
  - 处理：proto_sql/service_pool_test.go 三个用例，改写 config.AppConfig 并在 t.Cleanup 里还原，不并行。
- **L04-T10**：探活不经过连接池：有连接器时新建连接 Ping 后关闭；池里只有死连接、池满时都不误判
  - 处理：service_pool_test.go 的 TestPingStore_* 四个用例：fake driver.Connector / Conn（实现 Pinger），用 sql.OpenDB 建池并占满（SetMaxOpenConns(1) 再持有 db.Conn）。断言池连接器的 Connect 计数为 0。
- **L04-T11**：冷却指标收集器按 source 输出三类 pool 的序列
  - 处理：metrics_test.go TestStoreCooldownCollector_EmitsAllPools：用 NewPedanticRegistry 收集，覆盖 nil map 的情况。
- **A4-F5**：附录 A ④ 复核修正第 5 条：SQL 拿不到 ctx；claim 撤销时 SQL 持锁跑完，不存在「锁释放后旧语句才提交」；DSN 无超时导致每次再平衡等满 10s
  - 处理：已并入设计：claim 撤销不取消写 SQL，现状的安全性质保持；只有失租才取消，取消后锁留给 TTL，并要求 margin 30s ≥ 两个会话锁等待之和 + 5s，不满足时启动打 ERROR。读 SQL 随 claimCtx 取消，排空更快。DSN 超时已由 L-04 补上，再平衡排空有上界。
- **A4-REST**：附录 A ④ 的 B1-B4 及其余修正（演练挪 staging、rollout restart、p* 搬回工具、inflight 计数、分拣 Lua、B2 换代等）
  - 处理：归 S1，本项不处理。与 S1 的代码重叠见 ordering_and_overlap。
- **A3-FACT**：附录 A ③ 前提核对：停机时先提交、后离组成立；排空时提交连续完成前缀，可用于播种
  - 处理：新 Stop 顺序保持这个性质：步骤②会话释放时提交，步骤③才 LeaveGroup。停机排空期间 acker 仍只标记连续前缀，可用于播种的结论不变。F1-F11（交接闸、收尾、回档 Step 2、Kafka 钉版本等）归 S2。
- **A1-F7**：附录 A ① 修正 7：代码注释 KOC:847 serial per key via Kafka partition、:1279-1281 订正
  - 处理：行号已漂移，现为 :851 和 :1310、:1413。S4 改成中性表述：屏障只在读写同分区时成立，参见 ①。文档部分（login-simplification、partition-contract 等）归 S3。
- **BRM-B1**：附录 B redis-merge 阻断 1：Redis 传输错误改计到新计数会让 lookup_error / read_error 门禁失效
  - 处理：不适用。S4 不改 placement_guard / owner_epoch_guard 的口径，只新增指标；本轮不做 PRE/POST 合并。
- **BRM-B2**：附录 B redis-merge 阻断 2：共享 client 挂两次 Hook 会重复计数，PoolStats 导出两份
  - 处理：不用 Hook，在 11 个调用点逐一计时，每个往返只记一次。RegisterRedisPools 用接口值相等（指针身份）判断 placement 与 main 是否同一对象，同一对象只出 redis_client=main。用例 TestRedisPoolCollector_SharedClientExportedOnce，summarize 测试另有不重复计数的用例。
- **BRM-F1**：修正 1：口径二选一
  - 处理：不适用，口径不变，见 BRM-B1。
- **BRM-F2**：修正 2：只在 PlacementRedis!=RedisClient 时给 placement 挂 Hook / 导出 PoolStats；test_stress_summarize 补共享模式不重复计数
  - 处理：已并入：PoolStats 按指针身份判断；summarize 用例里只有 redis_client=main 的序列，断言输出值不翻倍。
- **BRM-F3**：修正 3：sharedWithMain 用实参身份判定
  - 处理：已并入 RegisterRedisPools(main, placement)：只比较实参身份，不读配置。PRE 侧不适用。
- **BRM-F4..F7**：修正 4-7：routeTask 按 op 分支、MOVED fail-closed、Lua false 不转 nil、PRE 改普通 pipeline
  - 处理：F4、F5、F7 不适用（本轮不做往返合并）。F6 与本项有关：新 luaMarkAppliedEpoch 只返回整数 1 或 error_reply，不向客户端返回 Lua 布尔；GET 的 false 在脚本内部消化。retryClaimScript 的真 Redis 集成测试不在本项。
- **BRM-F8**：修正 8：不夸大 LOST 的修复面，失租要告警，日志写 MySQL 可能回退
  - 处理：已并入：not_owner 告警为 critical，日志文案是「MySQL may hold an older snapshot until the next save」，文档统一写「不是栅栏」。
- **BRM-F9**：修正 9：对比口径（op_handler 会变，LOST 由 ok 改记 error）
  - 处理：写进 rollout：写任务的 op_handler 多了一次 verify 往返；verify 失败改记 db_task_result_total{error}；R20 与 R19 只比 task_total、排空时长、Redis commandstats。
- **BRM-F10**：修正 10：核实 MySQL 往返数（InterpolateParams 未开）
  - 处理：不是代码项，列为 R20 的运维核实（performance_schema / Com_stmt_prepare），交 Codex / 负责人。Save 的语句链已按源码确认（proto2mysql.go:3732-3805）。
- **BRM-F11**：修正 11：kafka_e2e 用 CreateTime 有跨机时钟偏差；queue_wait 要记入队时刻；重试源排除在 kafka_e2e 之外
  - 处理：已并入：workerTask 新增 enqueuedAt，ConsumeClaim 与 consumeOneRetryTask 赋值；kafka_e2e 只统计 kafkaMsg 非空且 Timestamp 非零的任务，负值丢弃；Help 与文档注明时钟偏差。
- **BBA-B1**：附录 B batch-amortize 阻断 B1：pipeline 里 EVALSHA 遇 NOSCRIPT 不回落
  - 处理：不适用，S4 不引入 pipeline / EVALSHA。
- **BBA-B2**：阻断 B2 及修 B2：每行 SQL 前检查 lost()；watchdog 在 TTL 预算内带退避重试
  - 处理：波次部分不适用。watchdog 有界退避已并入 H-G1-4；单任务版的「SQL 前检查」即 beginSQL 的本地截止时间复核。
- **BBA-B3**：阻断 B3 及修 B3：写路径自相矛盾、expand 出错语义
  - 处理：不适用。expand GET 出错走 tryLockAndProcess 的现有语义不变，只加计时。
- **BBA-F-misc**：go-redis MaxRetries 重发 pipeline、epoch 段 pcall、阶段 F 先查 claimCtx、P2 等 P1、watchdog 摘 token、收益口径、游标条件化（可选）
  - 处理：pipeline、阶段 F、P2 并行、摘 token、收益口径都不适用。epoch 段：新 Lua 先显式校验、后写，SET 之前没有可能中途失败的副作用。游标条件化归条件写会话。另记风险：单条 SETNX 被 go-redis 自动重试时，可能把自己刚设上的锁判成「被占」，这是既有问题，本项不修。
- **BBA-BUCKET**：不要原地修改 stageBuckets，新桶放到新直方图
  - 处理：已遵守：stageBuckets 不动；redis_step 与 batch 使用新直方图和新桶。
- **BMF-REPLACE**：附录 B mysql-fencing 修正：placement_route.go:217「整行 REPLACE」等依赖排序锁的注释同步修改
  - 处理：「整行 REPLACE」那一处由 S4 订正（现为 :218）。claim drain 注释、选库须持锁注释、relocate.go、runbook 归条件写会话。
- **BMF-MEASURE**：附录 B mysql-fencing 测量口径：subShardIndex 改动与 Stage 2 分开测；M2 记被拒写 blob 字节
  - 处理：subShardIndex 已在 d5cbb8a8e 入库，作为 R20 基线的一部分如实写进 rollout。M2 归条件写会话。
- **HO-23**：交接 §2.3 重叠项：WAITAOF、db_task retention/PVC、scene 脏快照、Kafka 钉版本、回档 Step 2
  - 处理：分别归 S1、S5、S5、S2、S2，本项不碰。

## 逐文件改动
### [new] go/db/internal/metrics/pipeline.go
S4 新增指标集中放在这个文件，以减少与 S1/S2/S3 在 metrics.go register() 上的冲突。

**指标**
- taskRedisStepSeconds：HistogramVec，Subsystem db，Name task_redis_step_seconds，labels {op,step}。桶 redisStepBuckets = {0.0001,0.00025,0.0005,0.001,0.002,0.005,0.01,0.025,0.05,0.1,0.25,0.5,1}。
- orderingGuardTotal：CounterVec，db_ordering_guard_total{outcome}。
- subShardBatchSize：Histogram，db_subshard_batch_size，桶 {1,2,4,8,16,32,64,128,256}。
- orderingLeaseLostTotal：CounterVec，db_ordering_lease_lost_total{reason}。
- orderingLeaseRenewRetryTotal：Counter，db_ordering_lease_renew_retry_total。
- orderingLockReleaseTotal：CounterVec，db_ordering_lock_release_total{outcome}。

**常量**
- RedisStepLockAcquire=lock_acquire、RedisStepCursorGet=cursor_get、RedisStepEpochGet=epoch_get、RedisStepPlacementLookup=placement_lookup、RedisStepExpandGet=expand_get、RedisStepPlacementRecheck=placement_recheck、RedisStepLeaseVerify=lease_verify、RedisStepCacheSet=cache_set、RedisStepCursorSet=cursor_set、RedisStepEpochMark=epoch_mark、RedisStepLockRelease=lock_release；`var RedisSteps = []string{...}`，按写路径顺序排列，共 11 个。
- OrderingApply=apply、OrderingApplyNoCursor=apply_no_cursor、OrderingStale=stale、OrderingIncomparable=incomparable、OrderingError=error。
- LeaseLostOwnerChanged=owner_changed、LeaseLostRenewExhausted=renew_exhausted、LeaseLostRenewDeadline=renew_deadline、LeaseLostVerifyError=verify_error。
- LockReleaseReleased=released、LockReleaseNotOwner=not_owner、LockReleaseError=error、LockReleaseSkippedSQLAbandoned=skipped_sql_abandoned。

**注册**
`func registerPipeline()`：独立的 sync.Once + MustRegister；把 ordering_guard、lease_lost、lock_release 的全部取值用 Add(0) 建成序列，让 increase 能抓到第一次。

**函数**（每个都先调用 registerPipeline）
- ObserveRedisStep(op, step string, d time.Duration)
- CountOrderingGuard(outcome string)
- ObserveSubShardBatch(n int)
- CountOrderingLeaseLost(reason string)
- CountOrderingLeaseRenewRetry()
- CountOrderingLockRelease(outcome string)

**go-redis 连接池收集器**
- `type RedisPoolClient interface{ PoolStats() *redis.PoolStats; Options() *redis.Options }`。
- `NewRedisPoolCollector(main, placement RedisPoolClient) prometheus.Collector`：只构造、不注册。Collect 输出 main；只有 placement != nil 且 placement != main（按接口值，即指针身份比较）时才再输出 placement。
- 输出的 const metrics，label 为 redis_client=main|placement：
  - 计数器：db_redis_pool_hits_total、misses_total、timeouts_total、wait_total（WaitCount）、wait_seconds_total（WaitDurationNs/1e9）、unusable_total、stale_conns_total；
  - 仪表：db_redis_pool_conns（TotalConns）、idle_conns、pending_requests、max_conns（Options().PoolSize）。
- `RegisterRedisPools(main, placement RedisPoolClient)`：注册到默认注册表；失败只打 ERROR，不挡启动（与 RegisterSQLPools 同口径）。

**注释**
- label 全是封闭集合，不含 player_id。
- uint32 计数器回绕时，Prometheus 会当成一次 counter reset。

### [modify] go/db/internal/metrics/metrics.go
- 包注释的 Stages 段补 sql / task_total / queue_wait / kafka_e2e 的含义：
  - sql：runDBOp 里 handler 的耗时（写含大字段闸 + Save，读是 SELECT）；
  - task_total：handleTask 整段；
  - queue_wait：入 worker 通道到 handleTask 开始；
  - kafka_e2e：生产者 CreateTime 到 handleTask 结束，只统计 Kafka 来源，有跨机时钟偏差，大于 5s 落进 +Inf 桶，但 sum/count 的平均值仍然准确。
- taskStageSeconds 的 Help 补上这四个取值。
- 常量块（:430-435）新增 StageSQL="sql"、StageTaskTotal="task_total"、StageQueueWait="queue_wait"、StageKafkaE2E="kafka_e2e"。
- DeferLeaseLost 的注释改为「排序锁失租（SQL 前 / SQL 中被取消 / 写完核锁时发现）」。
- Start()（:440）在 register() 之后加一行 registerPipeline()。
- stageBuckets 不动。

依赖：go/db/internal/metrics/pipeline.go

### [new] go/db/internal/kafka/ordering_lease.go
从 KOC 整块搬来并重写：orderingLockTTL / orderingLockRenewInterval（:1875-1876）、orderingLockKey（:1883-1885）、orderingLease 及其方法（:1887-1925）、acquireOrderingLock（:1927-1966）、orderingLockReleaseTimeout（:1968-1969）、releaseOrderingLock（:1971-1988）。errTaskDeferred / errOrderingLockBusy 留在 KOC 原处。

**头注释**：写明 G1 治标的四件事，以及「不是栅栏」（驱动取消只关连接、不发 KILL QUERY，本地截止时间依赖进程时钟，根治靠 db-write-fencing-mysql.md）。

**常量**
- orderingLockTTL=2m、orderingLockRenewInterval=30s、orderingLockReleaseTimeout=2s（三者值不变）。
- orderingLeaseSafetyMargin=30s。注释：必须大于服务端一条被遗弃语句的最长存活时间，即会话 lock_wait_timeout + innodb_lock_wait_timeout，默认各 10s，再加余量。
- orderingLeaseRenewCallTimeout=3s、orderingLeaseRenewBackoffMin=500ms、orderingLeaseRenewBackoffMax=5s、orderingLeaseRenewMaxAttempts=10。
- `var errOrderingLeaseReleased = errors.New("ordering lease released")`。

**type orderingLeaseTiming**
- 字段：ttl, renewEvery, margin, callTimeout, backoffMin, backoffMax time.Duration；maxAttempts int；now func() time.Time；after func(time.Duration) <-chan time.Time；jitter func(time.Duration) time.Duration。
- `var orderingLeaseTimingDefault`：time.Now、time.After，jitter 用 math/rand/v2 的 0.8~1.2 倍。
- `backoff(attempt int)` = jitter(min(backoffMin<<(attempt-1), backoffMax))。
- 测试通过替换包级变量并在 t.Cleanup 里还原。

**type orderingLease**
- 字段：result *locker.TryLockResult；timing；stop / done chan；sqlCtx context.Context；cancelSQL context.CancelCauseFunc；以及 mu 保护的 validUntil、lostErr、lostReason、sqlAbandoned。
- setLost(reason string, cause error)：只记第一次。lostErr = fmt.Errorf("%w: %w", errOrderingLeaseLost, cause)；调用 metrics.CountOrderingLeaseLost(reason)；cancelSQL(lostErr)。
- 其它方法：lost()、abandoned()；localDeadline() = validUntil − margin；extendValidUntil(t) 只向前推。

**acquireOrderingLock(ctx, w, task)**（签名不变）
- start := timing.now()。
- TryLock(ctx, orderingLockKey, timing.ttl) 计时为 ObserveRedisStep("write", RedisStepLockAcquire)。
- 原有两个错误分支原样保留。
- 成功后：sqlCtx, cancelSQL = context.WithCancelCause(context.Background())；validUntil = start + ttl；go lease.watch()。

**watch()**
- 循环：
  - remaining := localDeadline − now；remaining ≤ 0 时 setLost(renew_deadline) 并 return。
  - wait := min(next, remaining)；select stop / after(wait)。
  - 醒来后再判一次截止时间。
  - callCtx 取 WithTimeout(context.Background(), callTimeout)，调用 Extend(callCtx, ttl)。
- 三种结果：
  - 成功且仍持有：extendValidUntil(start+ttl)，attempts=0，next=renewEvery。
  - 成功但已不是持有者：setLost(owner_changed) 并 return。
  - 出错：attempts++；达到 maxAttempts 时 setLost(renew_exhausted)；否则 CountOrderingLeaseRenewRetry，用 storeLogLimiter.allow("ordering_lease_renew") 限速打 ERROR（英文：ordering lease renewal failed, retrying within the lease budget: key=... attempt=%d/%d err=%v suppressed_since_last=%d），next = backoff(attempts)。
- 刻意不监听 w.ctx：停机不等于失租。

**beginSQL() (context.Context, func(handlerErr error), error)**
- 已失租：直接返回 lostErr。
- now ≥ localDeadline：setLost(renew_deadline, errors.New("ordering lease past its local deadline before SQL")) 后返回 lostErr。
- 否则返回 sqlCtx 与 finish。finish 在 handlerErr != nil 且 sqlCtx.Err() != nil 时把 sqlAbandoned 置为 true。

**verify(ctx)**
- 已失租：返回 lostErr。
- 记下 start，调用 Extend(ctx, ttl)，计时为 ObserveRedisStep("write", RedisStepLeaseVerify)。
- err 非空且 ctx.Err() != nil：返回 fmt.Errorf("verify ordering lease: %w", err)，不 setLost（claim 收回、停机不算失租）。
- err 非空的其它情况：setLost(verify_error)，返回 lostErr。
- 已不是持有者：setLost(owner_changed)，返回 lostErr。
- 成功：extendValidUntil(start+ttl)，返回 nil。

**releaseOrderingLock(ctx, lease, task)**（签名不变）
- nil 时 return；close(stop)；<-done；defer cancelSQL(errOrderingLeaseReleased)，防止 ctx 泄漏。
- abandoned 时：CountOrderingLockRelease(skipped_sql_abandoned)，打 ERROR「ordering lock kept until TTL after its SQL was canceled on lease loss: taskID=%s key=%d msgType=%s cause=%v」，然后 return，不删锁。
- 否则：releaseCtx = WithTimeout(WithoutCancel(ctx), 2s)；released, err := Release，计时为 lock_release。
  - err 非空：error，日志沿用原文案。
  - !released：not_owner，打 ERROR「ordering lock already expired or taken over at release; MySQL may hold an older snapshot until the next save: taskID=%s key=%d msgType=%s leaseLost=%v」。
  - 其余：released。

**leaseMarginCoversLockWaits(lockWaitSec, innodbWaitSec int, margin time.Duration) error**
- 任一值 ≤0（关闭，服务端默认值是一年 / 50 秒）时返回错误「orphaned statement can outlive the ordering lock」。
- (lockWait + innodbWait)s + 5s > margin 时返回错误。

依赖：go/db/internal/metrics/pipeline.go

### [modify] go/db/internal/kafka/key_ordered_consumer.go
A. handleDBReadOp（:343-353）改成接 ctx：`store.WithContext(ctx).FindOneByWhereClause(...)`。handleDBWriteOp（:814-837）改成接 ctx：`store.WithContext(ctx).Save(msg)`，并在注释写明写的 ctx 来自租约、只在失租时取消。dbOpHandler 类型不变。

B. luaMarkAppliedEpoch（:554-563）
- 新增 const luaDecimalGreaterFn，内含 Lua 函数 is_dec(s)（s=='0' 或匹配 ^[1-9]%d*$）与 dec_gt(a,b)（先比 #a/#b，再用 string.byte 逐字节比）。注释写明原因和复用约定。
- luaMarkAppliedEpoch = luaDecimalGreaterFn + 主体：
  - new=ARGV[1] 不是规范十进制时返回 error_reply；
  - cur=GET 为 false 时，new 不等于 '0' 才 SET，然后 return 1；
  - cur 不规范时返回 error_reply；
  - dec_gt(new,cur) 时 SET；
  - return 1。
- :556 的 2^53 注释改写。

C. redisAppliedEpochStore.ReadAppliedEpoch / MarkAppliedEpoch（:582-602）：分别包一层计时，ObserveRedisStep("write", RedisStepEpochGet / RedisStepEpochMark)，逻辑不变。

D. orderingForTask（:476-511）
- 在 rc.Get 外包计时（cursor_get，op 取 "write"）。
- 每个 return 点调用 CountOrderingGuard：legacy 重试分支与各个 incomparable 返回记 incomparable；redis.Nil 记 apply_no_cursor；err 与 parse 失败记 error；stale 记 stale；最后的 apply 记 apply。
- dbTask.Op != "write" 的早退分支不计。

E. markAppliedSeq（:517-523）：只在 rc.Set 外包计时（cursor_set），**调用点不动**。

F. writeBackDBCache（:850-864）的 SET 加 ObserveRedisStep("write", cache_set)；publishDBReadResult 的 SET（:373）加 ObserveRedisStep("read", cache_set)。原有的 StageCacheWrite 保留。:851 注释改为「同 key 在单进程内经子 shard 串行，跨实例靠排序锁」。

G. 结构体与常量
- KeyOrderedKafkaConsumer 新增字段 pullStop chan struct{}、pullStopOnce sync.Once、consumeDone chan struct{}、stopOnce sync.Once、shutdownDrainTimeout time.Duration，并写中文注释。
- NewKeyOrderedKafkaConsumer 初始化 pullStop / consumeDone，shutdownDrainTimeout 取 claimDrainTimeout；并调用 leaseMarginCoversLockWaits(cfg.ServerConfig.Database.SessionLockWaitSeconds, ...SessionInnodbLockWaitSeconds, orderingLeaseSafetyMargin)，出错只打 logx.Errorf，不拒启。
- 新增 consumerShutdownGrace=5s、consumeRetryBackoff=1s。
- 方法 stopPulling()（nil 安全，Once 关闭）、pullStopped()（nil 时返回 false）、isStarted()（在 workersMu 下读 started）。

H. workerTask（:152-169）新增 enqueuedAt time.Time。

I. Start() 的 Consume 循环（:1040-1051）
- 改为 `defer close(c.consumeDone)`；`for !c.pullStopped()` 内调用 Consume。
- 出错时 select pullStop / ctx.Done / time.After(consumeRetryBackoff)。
- c.ctx.Err() != nil 时 break；退出时打 Info。

J. StartRetryConsumer（:1058-1075）的 select 加 `case <-c.pullStop: return`。consumeRetryQueue 的循环检查（:1165-1169）加 pullStop。consumeOneRetryTask 的派发 select（:1246-1251）加 `case <-c.pullStop: return false`，注释写明收据留在本实例的 processing 里，由 Stop 归还；retryTask 构造时设 enqueuedAt: time.Now()。

K. runSubShard（:1371-1401）在 processTaskBatch 之前调用 metrics.ObserveSubShardBatch(len(batch))。

L. handleTask（:1538-1686）
- 入口记 taskStart；在 recover 的 defer 之后注册计时 defer：task_total（op 取 taskOpLabel(task.dbTask)）；kafka_e2e（只在 task.kafkaMsg != nil 且 Timestamp 非零、差值 ≥0 时）。
- task 非 nil 且 enqueuedAt 非零时记 queue_wait。
- 取锁行（:1594）不变。
- 在 processDBTask 前写 `var lease writeLease; if orderingLock != nil { lease = orderingLock }`，避免把 nil 指针装进接口。
- 调用改为 processDBTask(processCtx, w, dbTask, route, lease, ...)。
- err 非空且 orderingLock.lost() != nil、err 又不是 errOrderingLeaseLost 时，用 fmt.Errorf("%w: %w", errOrderingLeaseLost, err) 包装后再走原有的日志 / deferFailedTask。
- 成功分支**删除** verify 块（:1659-1667），改为注释「核锁已在 runDBOp 内、缓存回写之前完成」。markAppliedSeq（:1670）与 markAppliedOwnerEpoch（:1674）的调用与顺序原样保留。

M. ConsumeClaim（:1724-1840）
- claimCtx 改为 context.WithCancel(context.WithoutCancel(session.Context()))，加观察 goroutine：select session.Context().Done() 时，只在 !pullStopped() 下 cancelClaim()；ConsumeClaim 返回时 close(stopWatch)。注释写明理由（兄弟 claim 先排空返回会取消整个会话）。
- 主循环顶部、select 新增的 pullStop 分支、`<-session.Context().Done()` 分支（先判 pullStopped）、消息通道关闭的 !ok 分支（先判 pullStopped），都改走 drainClaimForShutdown。failureCh 分支除外。
- 收到消息后、acker.track 之前再判一次 pullStopped：这条消息不 track，由下一任属主重放。
- 新建 workerTask 时设 enqueuedAt: time.Now()。

N. 新方法 `func (h *consumerGroupHandler) drainClaimForShutdown(claim sarama.ConsumerGroupClaim, acker *claimAcker) error`
- 等待上限 timeout = shutdownDrainTimeout（≤0 时取 claimDrainTimeout）。
- select 四路，**不看会话 ctx**：
  - acker.wait() 完成：打 Info「claim drained for shutdown」，return nil；
  - failureCh：acker.stop()，return err；
  - h.consumer.ctx.Done：acker.stop()，return nil；
  - timer 到期：acker.stop()，打 ERROR「shutdown drain timed out; remaining tasks replay from the committed offset: topic partition timeout」，return nil。
- 不再二次等待。

O. processDBTask / tryLockAndProcess / processTaskWithoutLock / runDBOp 都加 `lease writeLease` 参数并逐层透传。
- processDBTask 在 GetExpandStatus（:1852）外包计时（expand_get，op 取 taskOpLabel(task)）。
- 新增接口：
  type writeLease interface {
    beginSQL() (context.Context, func(handlerErr error), error)
    verify(ctx context.Context) error
  }

P. runDBOp（:2067-2082）
- route 检查不变。
- 写任务：lease 为 nil 时返回 fmt.Errorf("no ordering lease for write taskID=%s; refusing to run SQL")（fail-closed）；beginSQL 出错直接返回（一条 SQL 都没发）。读任务：sqlCtx=ctx。
- 执行 handler 并计时 StageSQL；finish(handlerErr)。
- sqlCtx.Err() 为空时才 reportStoreResult。
- handlerErr 非空时返回。
- 然后 route.confirm(ctx)。
- 写任务再执行 lease.verify(ctx)：错误已是 errOrderingLeaseLost 的原样返回，否则包成 "verify ordering lease before cache publication: %w"。
- 最后 publishDBOpResult。
- processTaskWithoutLock 的注释（:2042）改为「op_handler（写）= SQL + 复核 + 核锁 + 缓存回写」。

Q. Stop()（:2274-2283）
- 改为 c.stopOnce.Do(c.stop)。stop 依次执行：
  1. stopPulling；
  2. isStarted() 且 consumeDone 非 nil 时，等 consumeDone，上限 shutdownDrainTimeout+consumerShutdownGrace；超时打 ERROR；
  3. consumer.Close()，沿用原日志；
  4. c.cancel()；c.wg.Wait()；
  5. releaseRetryOwnership；
  6. 打 Info「db consumer stopped: groupID topic took=%v」。
- 每一步写中文注释，说明先提交后离组的理由。

R. 注释订正
- :724-727 与 :1222-1224 改为「消费端不能用 Key%N 重算原始分区：生产端分区器不是 Key%N（C++ librdkafka 默认 consistent_random，即 CRC32(key) mod N；login 读在 ① 落地前是 sarama 默认哈希），见 db-task-kafka-partition-contract.md」，删掉「一致性哈希 / 临时重映射」。
- :1306-1310 与 :1409-1414 改为「同 key 的读只有与写落在同一分区时，这道屏障才成立；读写分区对齐见瓶颈文档 ①」。

依赖：go/db/internal/kafka/ordering_lease.go

### [modify] go/db/internal/kafka/placement_route.go
- routeTask 里 router.lookup(w.ctx, ...)（:272）外包计时 ObserveRedisStep(op, RedisStepPlacementLookup)。
- confirm 里 r.router.lookup(ctx, ...)（:244）外包计时 ObserveRedisStep(r.op, RedisStepPlacementRecheck)。
- :218 注释「整行 REPLACE,重复落地幂等」改为「整行 Save（按主键 UPDATE，0 行再 INSERT），重复落地幂等」。
- 其余逻辑不动。

依赖：go/db/internal/metrics/pipeline.go

### [modify] go/db/internal/kafka/store_failure.go
只改注释。errOrderingLeaseLost 的注释（:27-29）改为：排序锁失租（续租放弃 / 本地截止时间已过 / 核锁发现易主）；可能发生在 SQL 之前（不发 SQL）、SQL 途中（SQL 被取消，锁留给 TTL）或 SQL 之后（不发布缓存、不推进游标）；是否豁免重试预算仍不在本批范围（条件写另议），照旧耗预算。代码不变。

### [modify] go/db/internal/logic/pkg/proto_sql/store_registry.go
:419-420 注释里的「(REPLACE INTO `player_database` …)」改为「(UPDATE `player_database` … / INSERT INTO `player_database` …，Save 按主键先 UPDATE、0 行再 INSERT)」，其余文字不动。

### [modify] go/db/internal/migrate/plan.go
- :176-178 的告警字符串保留「缺主键:%s 按 proto 应有 PRIMARY KEY (%s),实际没有。」前缀（plan_test.go:232 断言了该子串），把 ODKU 那一句换成「存盘(proto2mysql Save:按主键 UPDATE,0 行再 INSERT)没有主键时不会撞 1062,内容未变或并发首写都会追加新行,」。
- :248-251 与 :298-301 两处注释里的「存盘走 INSERT ... ON DUPLICATE KEY UPDATE —— 没有主键就没有重复可言,每次存盘都会追加」改为同一口径，并保留「表膨胀、读到任意一行」的后果。
- 字符串保留中文：格式已被测试钉住，属于范围控制；交付说明里写明。

### [modify] go/db/internal/migrate/plan_test.go
同口径订正 :214-215、:256-257 两处注释和 :266 的 Fatalf 文案，断言逻辑不变。这是同一份业务知识的同步订正，可选；如果要压文件数，可以拿掉。

依赖：go/db/internal/migrate/plan.go

### [modify] go/db/db.go
- RegisterStoreCooldown（:122）之后加 `metrics.RegisterRedisPools(ctx.RedisClient, ctx.PlacementRedis)`，配注释：同一对象只导出一份，见 BRM-B2。
- signal.Notify（:244-245）上方加注释：不要对 quit 调用 signal.Stop / Reset。Linux 上 go-zero 会在 5.5 秒后向自身重发信号，quit 仍在注册时信号只进通道，不会杀掉正在排空的 Stop。
- 其余不动。

依赖：go/db/internal/metrics/pipeline.go

### [new] go/db/internal/kafka/ordering_lease_test.go
租约单测。miniredis 加 fake orderingLeaseTiming：可控的 now、after 返回由测试触发的通道并把请求时长写进 requested 通道、jitter 恒等。用例列表见 tests。

依赖：go/db/internal/kafka/ordering_lease.go

### [new] go/db/internal/kafka/run_db_op_test.go
runDBOp 顺序与上报单测，外加 handleTask 级回归。替身：fakeLease（实现 writeLease，记录调用并可注入错误）、healthStores（fakeStores 加 storeHealth 的计数）。用例见 tests。

依赖：go/db/internal/kafka/key_ordered_consumer.go

### [new] go/db/internal/kafka/consumer_shutdown_test.go
停机单测。替身：fakeClaim（实现 sarama.ConsumerGroupClaim，带缓冲的 messages 通道）、fakeConsumerGroup（实现 sarama.ConsumerGroup，Consume 里按 sarama 语义调用 Setup / ConsumeClaim：第一个 claim 返回时取消会话 ctx，全部返回后调用 Cleanup 并返回，Close 记录事件与当时的 c.ctx.Err()）。复用 key_ordered_consumer_test.go 的 recordingSession（ctx 可取消）与 recordingHarness。用例见 tests。

依赖：go/db/internal/kafka/key_ordered_consumer.go

### [new] go/db/internal/kafka/l04_deferral_test.go
launch-todo §2.3 中 kafka 包的第 1-7 条（第 8 条在 run_db_op_test.go）。包含测试用的 go-redis Hook claimCounter：统计 Args()[1]==retryClaimScript 的 eval 次数，可选把认领到的载荷用另一个 client LPUSH 回 ready。

### [new] go/db/internal/kafka/instrumentation_test.go
打点单测，并定义包内测试 helper `metricSample(t, family string, labels map[string]string) (count float64, sum float64)`：经 prometheus.DefaultGatherer.Gather() 读取，不 import testutil，避免改 go.sum。用例见 tests。

依赖：go/db/internal/metrics/pipeline.go

### [new] go/db/internal/kafka/applied_epoch_lua_test.go
luaMarkAppliedEpoch 的十进制比较回归。用 miniredis（gopher-lua，数字同样是 double，能复现旧缺陷）加 redisAppliedEpochStore。

依赖：go/db/internal/kafka/key_ordered_consumer.go

### [modify] go/db/internal/metrics/metrics_test.go
追加四个用例：TestStoreCooldownCollector_EmitsAllPools、TestRedisPoolCollector_SharedClientExportedOnce、TestRedisPoolCollector_SeparatePlacementClient、TestRedisStepNamesPinned（11 个、去重、按写路径顺序，并与 summarize 段注释对照）。用 fakeRedisPoolClient 返回固定的 PoolStats 与 Options{PoolSize:10}。

依赖：go/db/internal/metrics/pipeline.go

### [new] go/db/internal/logic/pkg/proto_sql/service_pool_test.go
launch-todo §2.3 第 9、10 条：newServiceMysqlConfig / NewMysqlConfig 的参数；pingStore 绕过连接池（fake driver.Connector / Conn / Pinger，用 sql.OpenDB 建池）。用例见 tests。

### [modify] deploy/k8s/db-task-retry-alerts.yaml
- 文件头「指标定义」补上 ordering_lock_release_total / ordering_lease_lost_total / ordering_lease_renew_retry_total，并注明 G1 治标不是栅栏。
- 新增 group db-task.ordering-lease（interval 30s），两条规则：
  1. DbOrderingLockReleasedNotOwned：expr `sum by (namespace) (increase(db_ordering_lock_release_total{outcome="not_owner"}[10m])) > 0`，for 0m，severity critical。description（中文）：释放时锁已不归本实例；持锁期间别的实例可能已写下更新版本，本实例的整行写可能晚于它落库，MySQL 可能停在旧档，直到玩家下一次存盘；G1 治标只缩小窗口，不是栅栏。处置：按日志「ordering lock already expired or taken over at release」里的 key/msgType 抽样核对 MySQL 行与 Redis PlayerAllData；查同时段 Redis 延迟、节点或虚拟机暂停、DbOrderingLeaseLost。
  2. DbOrderingLeaseLost：expr `sum by (namespace, reason) (increase(db_ordering_lease_lost_total[10m])) > 0`，for 0m，severity warning。description：失租后本条写被取消或作废并延后重试；SQL 若已在途，锁留给 TTL 过期，该玩家的存盘最多排队约 2 分钟；owner_changed 表示别的实例已持有这把锁；renew_exhausted / renew_deadline 表示 Redis 续租持续失败。
- 安装方式沿用文件头（kubectl apply -n observability），由负责人执行。

依赖：go/db/internal/metrics/pipeline.go

### [modify] deploy/k8s/manifests/go-svc/db.yaml
- Deployment spec 在 strategy（:37）上方加 `minReadySeconds: 20`（两格缩进），注释：readiness 只探 gRPC，不反映入组；新 Pod Ready 后至少等完一次入组，即 eager 再平衡（心跳 3s，加上其它成员撤销 claim 的排空 ≤10s），再停旧 Pod；新 Pod 起来后很快崩溃时滚动也会停住。该字段属于 Deployment spec，改它不会触发 Pod 重建。
- terminationGracePeriodSeconds 的注释（:107-108）改为：preStop 5s + 撤拉取后排空在途任务 ≤10s（超时作废剩余、由新属主从已提交位点重放）+ 离组 + 等 worker 退出（写 SQL 不被取消，受读超时 30s 约束，通常为毫秒级）+ 归还重试收据 ≤5s；被 SIGKILL 也不丢（锁留到 TTL，收据等租约过期后回收）。
- 值仍为 45，不改。

### [modify] tools/scripts/tests/k8s_deploy_contract.tests.ps1
在「db 是多副本形态」用例（约 :241-253）追加：`Assert-Match -Text $block -Pattern '(?m)^  minReadySeconds: 20\s*$' -Because '新 Pod Ready 后至少等完一次入组再停旧 Pod(G6)'`。该文件工作区里另有会话未提交的改动，改前重新读取，只追加这一行。

依赖：deploy/k8s/manifests/go-svc/db.yaml

### [modify] tools/scripts/go_services.ps1
- param 新增 `[switch]$ForceStop`（跳过 Ctrl+C，直接强杀，即旧行为）与 `[int]$StopTimeoutSeconds = 0`（>0 时覆盖所有服务的优雅等待）。头部 .PARAMETER 文档同步补充。
- 目录 db 条目加 `StopTimeoutSeconds = 35`；目录注释说明可选键 StopTimeoutSeconds。

**新增函数**（均放在函数体内，测试经 AST 加载不依赖脚本级变量）
- Get-GracefulStopSeconds([hashtable]$Info)：优先 $StopTimeoutSeconds，其次 $Info.StopTimeoutSeconds，默认 25（覆盖 chat / trade / friend / guild lifecycle 的 24s 硬截止）。
- Get-ConsoleCtrlCHelperScript([int]$ProcessId)：返回辅助脚本文本。脚本用 Add-Type -Namespace MmorpgConsole -Name Native 声明四个 kernel32 P/Invoke：FreeConsole、AttachConsole(uint)、SetConsoleCtrlHandler(IntPtr,bool)、GenerateConsoleCtrlEvent(uint,uint)。依次执行 FreeConsole、AttachConsole(pid)（失败 exit 2）、SetConsoleCtrlHandler(Zero,$true)、GenerateConsoleCtrlEvent(0,0)（失败 exit 3），成功 exit 0。PID 以 [int] 形式嵌入，没有注入面。
- Send-ConsoleCtrlC([int]$ProcessId)：宿主取 (Get-Process -Id $PID).Path，用 `Start-Process -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand', <UTF-16LE base64>) -WindowStyle Hidden -Wait -PassThru` 启动辅助进程，返回 ExitCode -eq 0。
- Wait-ProcessExit([int]$ProcessId,[double]$TimeoutSeconds)：Get-Process 取不到即返回 $true，否则 WaitForExit(毫秒)。
- Stop-ProcessTree([int]$ProcessId)：`taskkill.exe /PID n /T /F`，失败时退回 Stop-Process -Force -ErrorAction SilentlyContinue。
- Get-StopClockSeconds：返回 [Diagnostics.Stopwatch]::GetTimestamp()/Frequency，可在测试中替换。

**Invoke-Stop（:848-891）重写为三段**，目标实例的选择逻辑不变：
1. 对每个存活实例：-ForceStop 时直接 Stop-ProcessTree，输出 [kill]；否则 Send-ConsoleCtrlC。发不出去时立即 Stop-ProcessTree，输出「[kill] key (PID n): console interrupt not deliverable; force-killed process tree」；发出去的记下截止时间 = now + Get-GracefulStopSeconds。
2. 逐个按剩余时间 Wait-ProcessExit：退出的输出「[stop] key (PID n, exited in Xs)」；超时的 Stop-ProcessTree，输出「[kill] key (PID n): no exit within Ns after Ctrl+C; force-killed process tree」。
3. 已退出或不存在的实例保持原有的 [gone] 输出。
- PID 记录移除与 Write-PidFile 不变。
- 注释说明 Windows 上 go-zero 不处理信号，Go 侧把 CTRL_C_EVENT 当 os.Interrupt；go run 模式下 go.exe 忽略中断、等子进程退出。
- 启动处（:693-710）不改，但加注释：必须保持每个实例独立控制台，不得加 -NoNewWindow，否则 Ctrl+C 会波及同控制台的其它服务。

### [new] tools/scripts/tests/go_services_stop.tests.ps1
`#requires -Version 7`，加载 lib/test_harness.ps1，经 AST 只加载 go_services.ps1 的函数（同 go_services_ports.tests.ps1）。PidFile 用临时目录里的真实 JSON 文件。桩函数：Get-Process、Send-ConsoleCtrlC、Wait-ProcessExit、Stop-ProcessTree、Get-StopClockSeconds；Stop-Process 一调用就 throw。用例见 tests。

依赖：tools/scripts/go_services.ps1

### [modify] tools/scripts/stress_summarize.ps1
在 2c 之后、3 之前（:535-537）新增两段，读的都是 $dbSnapshots。缺序列一律用 Get-MetricSumOrNull 并打印 n/a（旧快照不能折成 0）。正则按 client_golang 的字母序 label 写，例如 `^db_task_stage_seconds_sum\{op="write",stage="sql"\}`。

**2d `=== DB write path (db_task_stage_seconds sql/task_total/queue_wait/kafka_e2e, Round 20+) ===`**
- 列：snapshot | w_sql w_task w_queue w_e2e | r_sql r_task | batch | no_cursor lease_lost rel_notown | rpool_wait rpool_tmo。
- batch 是 db_subshard_batch_size 的 sum/count，保留一位小数。
- no_cursor 取 db_ordering_guard_total{outcome="apply_no_cursor"}。
- lease_lost 是 db_ordering_lease_lost_total 各 reason 之和。
- rel_notown 取 db_ordering_lock_release_total{outcome="not_owner"}。
- rpool_* 是 db_redis_pool_wait_total / timeouts_total 跨 redis_client 求和；共享模式只有 main 一份，不重复计数。

**2e `=== DB Redis steps (db_task_redis_step_seconds{op="write"} avg, Round 20+) ===`**
- 11 列，顺序同 metrics.RedisSteps（列头缩写 lock_acq cur_get ep_get plc_lkp exp_get plc_rck verify cache cur_set ep_mark lock_rel），最后一列是各步平均值之和 sum。
- 段注释写明 11 个名字与 go/db/internal/metrics/pipeline.go 的 RedisSteps 一一对应，改名要同改两边。

- 文件头 Synopsis 补两行。既有 2c 段不改。

依赖：go/db/internal/metrics/pipeline.go

### [modify] tools/scripts/test_stress_summarize.ps1
1. 把 :234-247 的 DB 断言挪到 Get-SectionRow 等 helper 定义之后，改为 `Get-SectionRow -SectionMarker 'DB task stages' -SnapshotName 't18m_db'`。原来「全量输出恰好 1 行」的断言在新增段之后不再成立。
2. 新增假快照 t20m_db.txt，内容包括：
   - 新 stage（write sql 0.002/1、task_total 0.02/1、queue_wait 0.001/1、kafka_e2e 0.05/1，read sql 0.003/1、task_total 0.006/1）；
   - 11 个 step 的 sum/count，取互不相同的值，列串位就会变红；
   - batch sum 30 / count 10；
   - ordering_guard apply_no_cursor 4；lease_lost owner_changed 1 加 renew_deadline 2；release not_owner 1；
   - redis_pool 只有 redis_client="main" 的 wait_total 5、timeouts_total 0。
3. 断言：
   - 'DB write path' 段 t20m_db 行的各列数值匹配，rpool_wait 为 5（不翻倍）；
   - 'DB write path' 段 t18m_db 行除 snapshot 外全为 n/a；
   - 'DB Redis steps' 段 t20m_db 行 11 个值加 sum 匹配；
   - 'DB Redis steps' 段 t18m_db 行全为 n/a。

依赖：tools/scripts/stress_summarize.ps1

### [modify] docs/design/db-task-pipeline-bottlenecks.md
- §4.4 表 G1、G6、G9 三行的「修法」列各追加「(治标已落码,未编译未测试,见 §7)」。
- §7 末尾追加「S4 落码(G1 治标与 go/db 小项)」小表：文件、要点、待 Codex。明写 G1 治标不是栅栏、撤不回已进 MySQL 的语句；只有失租才取消 SQL；在途 SQL 被取消时锁留给 TTL。
- 其它段不动（S1/S2/S3 也会改这份文件，改前重新读取）。

依赖：go/db/internal/kafka/ordering_lease.go

### [modify] docs/ops/godb-pipeline-launch-todo.md
- §2.3 状态行改为「已写(日期),文件:l04_deferral_test.go / run_db_op_test.go / service_pool_test.go / metrics_test.go,待 Codex」。
- 文件末尾追加「§4 G1 治标、停机顺序与测量打点(S4)」，含四小节：
  - 4.1 编译与单测命令（同 tests / rollout）；
  - 4.2 只在 compose 或 kind 上做的故障注入（优雅停、续租失败、锁被接管加 LOCK TABLES 卡住 SQL）及通过标准；
  - 4.3 上线前置（告警 apply、minReadySeconds 在下一次滚动生效）；
  - 4.4 状态。
- 该文件工作区里有别的会话未提交的改动，只追加、不改旧行，改前重新读取。

### [modify] docs/PROGRESS.md
文件末尾追加一条「补充:存盘管线 S4(G1 治标、停机顺序、go_services 优雅停、测量打点、Lua tonumber、L-04 单测)」（文件配了 union merge），内容写明：
- 改动清单与关键语义：只有失租取消写 SQL；在途 SQL 取消后锁留到 TTL；核锁前移到缓存回写之前；续租有界退避；Stop 五步；minReadySeconds 20；十进制串比较；
- 未编译、未测试、待 Codex；
- 与 S1/S2/S3/条件写会话的重叠点；
- Java 版不适用（PARITY 已登记 / 待登记）。

## 接口（类型 / 配置键 / 指标 / Redis 键 / 组名）
**一、go/db/internal/kafka**

dbOpHandler 签名不变：

```go
func(ctx context.Context, store *proto2mysql.DB, task *db_proto.DBTask, msg proto.Message) error
```

新增接口：

```go
type writeLease interface {
	beginSQL() (ctx context.Context, finish func(handlerErr error), err error)
	verify(ctx context.Context) error
}
```

生产实现是 *orderingLease。handleTask 只在 orderingLock 非 nil 时赋值，避免把 nil 指针装进接口。

签名变化（都是包内函数）：

```go
func processDBTask(ctx context.Context, w *worker, task *db_proto.DBTask, route *placementRoute, lease writeLease, partition int32, seq uint64, isOfflineExpand bool) error
func tryLockAndProcess(ctx context.Context, worker *worker, key string, task *db_proto.DBTask, seq uint64, route *placementRoute, lease writeLease) error
func processTaskWithoutLock(ctx context.Context, redisClient redis.Cmdable, task *db_proto.DBTask, route *placementRoute, lease writeLease) error
func runDBOp(ctx context.Context, redisClient redis.Cmdable, task *db_proto.DBTask, msg proto.Message, handler dbOpHandler, route *placementRoute, lease writeLease) error
```

runDBOp 写任务的顺序固定为：beginSQL → handler(sqlCtx) → finish → [sqlCtx 未被取消才 reportStoreResult] → confirm(ctx) → verify(ctx) → publishDBOpResult。读任务没有 beginSQL / verify，handler 用 ctx。

新增方法：

```go
func (h *consumerGroupHandler) drainClaimForShutdown(claim sarama.ConsumerGroupClaim, acker *claimAcker) error
func (c *KeyOrderedKafkaConsumer) stopPulling()
func (c *KeyOrderedKafkaConsumer) pullStopped() bool
func (c *KeyOrderedKafkaConsumer) isStarted() bool
```

- Stop() 外部签名不变，内部改为 stopOnce 加五步。
- 结构体新字段：KeyOrderedKafkaConsumer.{pullStop, pullStopOnce, consumeDone, stopOnce, shutdownDrainTimeout}；workerTask.enqueuedAt。
- 新常量：consumerShutdownGrace=5s，consumeRetryBackoff=1s。
- Lua 常量：luaDecimalGreaterFn（新增，供条件写会话复用），luaMarkAppliedEpoch（重写；Redis 里的值格式不变，仍是规范十进制）。

**二、ordering_lease.go（新）**

- acquireOrderingLock、releaseOrderingLock 签名不变。
- 方法：

```go
func (l *orderingLease) beginSQL() (context.Context, func(error), error)
func (l *orderingLease) verify(ctx context.Context) error
func (l *orderingLease) lost() error
func (l *orderingLease) abandoned() bool
func (l *orderingLease) setLost(reason string, cause error)
func leaseMarginCoversLockWaits(lockWaitSeconds, innodbLockWaitSeconds int, margin time.Duration) error
```

- type orderingLeaseTiming，包级变量 orderingLeaseTimingDefault（测试可替换）。
- 常量：orderingLockTTL=2m，orderingLockRenewInterval=30s，orderingLockReleaseTimeout=2s，orderingLeaseSafetyMargin=30s，orderingLeaseRenewCallTimeout=3s，orderingLeaseRenewBackoffMin=500ms，orderingLeaseRenewBackoffMax=5s，orderingLeaseRenewMaxAttempts=10。
- 变量：errOrderingLeaseReleased。

**三、go/db/internal/metrics**

1. db_task_stage_seconds{op,stage}：stage 新增 sql | task_total | queue_wait | kafka_e2e，桶仍是 stageBuckets。常量 StageSQL / StageTaskTotal / StageQueueWait / StageKafkaE2E。
2. db_task_redis_step_seconds{op=read|write, step}：
   - step 有 11 个：lock_acquire、cursor_get、epoch_get、placement_lookup、expand_get、placement_recheck、lease_verify、cache_set、cursor_set、epoch_mark、lock_release。
   - 桶 {0.0001, 0.00025, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1}。
   - 读任务只出现 placement_lookup、expand_get、placement_recheck、cache_set。
3. db_ordering_guard_total{outcome=apply|apply_no_cursor|stale|incomparable|error}，启动时建 0。
4. db_subshard_batch_size（直方图），桶 {1, 2, 4, 8, 16, 32, 64, 128, 256}。
5. db_ordering_lease_lost_total{reason=owner_changed|renew_exhausted|renew_deadline|verify_error}，启动时建 0。
6. db_ordering_lease_renew_retry_total（无 label）。
7. db_ordering_lock_release_total{outcome=released|not_owner|error|skipped_sql_abandoned}，启动时建 0。
8. db_redis_pool_*，label redis_client=main|placement；placement 与 main 是同一对象时只出 main：
   - 计数器：hits_total、misses_total、timeouts_total、wait_total、wait_seconds_total、unusable_total、stale_conns_total；
   - 仪表：conns、idle_conns、pending_requests、max_conns。

新函数：

```go
ObserveRedisStep(op, step string, d time.Duration)
CountOrderingGuard(outcome string)
ObserveSubShardBatch(n int)
CountOrderingLeaseLost(reason string)
CountOrderingLeaseRenewRetry()
CountOrderingLockRelease(outcome string)
NewRedisPoolCollector(main, placement RedisPoolClient) prometheus.Collector
RegisterRedisPools(main, placement RedisPoolClient)
```

另有 type RedisPoolClient interface{ PoolStats() *redis.PoolStats; Options() *redis.Options }，以及 var RedisSteps []string。所有 label 都不含 player_id。

**四、告警**（deploy/k8s/db-task-retry-alerts.yaml，新 group db-task.ordering-lease）

- DbOrderingLockReleasedNotOwned（critical）
- DbOrderingLeaseLost（warning）

**五、没有新增的东西**

- 配置键：无。日志里打出的 margin 校验依赖已有的 Database.SessionLockWaitSeconds / SessionInnodbLockWaitSeconds（默认各 10）。
- Redis 键：无新增。排序锁键 distributed:lock:kafka:ordering:{topic}:{key}:{msgType}、游标、applied_epoch 的键名与值格式都不变。
- Kafka 组名、topic：不变。

**六、K8s**

db Deployment 加 minReadySeconds: 20；terminationGracePeriodSeconds 仍为 45。

**七、脚本**

- go_services.ps1 新参数：-ForceStop、-StopTimeoutSeconds（默认 0，即按服务取值）。
- 目录可选键 StopTimeoutSeconds，db=35，其它默认 25。
- 新函数：Get-GracefulStopSeconds、Get-ConsoleCtrlCHelperScript、Send-ConsoleCtrlC、Wait-ProcessExit、Stop-ProcessTree、Get-StopClockSeconds。
- 输出标签：[stop] / [kill] / [gone]，文案为英文。
- stress_summarize.ps1 新增段标题：'DB write path (...Round 20+)'、'DB Redis steps (...Round 20+)'。

## 顺序与重叠
**一、S4 内部落码顺序**

1. metrics/pipeline.go 与 metrics.go：其它文件依赖它们的常量和函数。
2. ordering_lease.go：从 KOC 搬出这一块并重写。
3. key_ordered_consumer.go、placement_route.go、store_failure.go、db.go。
4. 注释订正：store_registry.go、plan.go、plan_test.go。
5. 测试：kafka 包 6 个新文件、metrics_test、proto_sql 一个新文件。
6. 脚本与部署：go_services.ps1 及其测试、stress_summarize 及其测试、db.yaml 及契约测试、告警。
7. 文档：瓶颈文档 §7、launch-todo §4、PROGRESS。

建议拆 3 个 PR，合计 28 个文件，不超过 30。三者可以按任意顺序合入，但打点相关的 summarize 段要与 go/db PR 同一天上：
- PR-1：go/db 代码与测试，19 个文件；
- PR-2：go_services.ps1、stress_summarize 及两个测试，4 个文件；
- PR-3：部署、契约测试、告警、文档，6 个文件（含 plan_test 时是 7 个）。

**二、⚠ markAppliedSeq / markAppliedOwnerEpoch（单独标出，供条件写会话合并）**

1. **调用顺序与位置不变**：先游标、后 epoch，都在 handleTask 的成功分支里，在 processDBTask 返回之后、LREM / ack 之前。
2. **它们之前的 verify 被移走**：原来在 handleTask 里 processDBTask 返回之后执行（KOC:1662-1667），现在挪进 runDBOp，放在「复核之后、缓存回写之前」。
   - 新的 Redis 顺序：SQL → 复核 MGET → verify EVAL → 缓存 SET → 游标 SET → epoch EVAL → LREM → ack → 释放。
   - 旧的顺序：SQL → 复核 → 缓存 SET → verify → 游标 SET → epoch。
3. **函数体有改动**：markAppliedSeq 加 cursor_set 计时；MarkAppliedEpoch 加 epoch_mark 计时，luaMarkAppliedEpoch 改为十进制串比较。

条件写会话把「游标 SET 与 applied_epoch 推进」合成一条原子脚本时，需要注意：
- ① 核锁仍要在缓存回写之前；如果把 token 校验并入脚本，要放在缓存写之前。
- ② 复用 luaDecimalGreaterFn。其设计稿 db-write-fencing-mysql.md:363-365 仍用 tonumber。
- ③ 若合并或改名 cursor_set、epoch_mark 两个 step，要同时改 metrics.RedisSteps、TestRedisStepNamesPinned、stress_summarize 的 2e 段与 test_stress_summarize。
- ④ orderingForTask 新增的 db_ordering_guard_total 计数，在 G3（先判 epoch）重排时要保留。
- ⑤ releaseOrderingLock 的 skipped_sql_abandoned 分支要与 Stage 2 删锁设计协调。
- ⑥ runDBOp 多了 lease 参数，而条件写的 fence outcome 处理正好落在 handler 之后，需要在 S4 之上 rebase。dbOpHandler 签名不变。

**三、与 S1（④ 重试只交给分区属主 + WAITAOF）**

- 重叠函数：consumeRetryQueue、consumeOneRetryTask、StartRetryConsumer（S4 只加 pullStop 分支）、Stop（S4 改为五步；S1 的 inflight 计数和属主表清理要接在第④步之前或之后）、ConsumeClaim（S1 的属主表要在 claim 起止时登记；S4 改了 claimCtx 的派生方式和停机分支）、metrics.go（S4 只改 Stage 常量、Help、Start 一行，新指标都在 pipeline.go）、stress_summarize.ps1（S1 也加新段）、db.go。
- S4 不碰 deferFailedTask、pushPayloadDurably、saveToRetryQueue，WAITAOF 的写入点不冲突。
- 建议 S4 先合（改动小、相互独立），S1 rebase 时保留各处的 pullStop 分支。属主判定「claim 的 ctx 还活着」要注意：停机排空期间 claimCtx 不随会话取消，S1 应改用 claim 本身的生命周期。

**四、与 S2（③ 消费组按 zone 拆 + Kafka 钉版本 + 回档 Step 2）**

- 重叠点：Start() 的 Consume 循环（S2 的交接闸放在 Consume 之前，等待时要同时 select pullStop 和 ctx，否则 Stop 会被闸卡住）、Setup（播种；S4 不改）、k8s_deploy_contract.tests.ps1（工作区里已有别的会话未提交的改动）、db.yaml 注释、stress_summarize 的 KafkaConsumerGroup 缺省参数（S4 不改）、瓶颈文档。
- 新的 Stop 顺序保持先提交、后离组，S2 的播种前提不变。

**五、与 S3（① 读写分区对齐）**

- 重叠点：ConsumeClaim（S3 在解码后加一行写分区审计；S4 改了循环结构和 enqueuedAt）、KOC 的五处注释（S4 写成在 ① 前后都成立的中性表述，① 落地后由 S3 把「读写同分区」收口）、metrics.go（S3 的审计计数器建议也放独立文件）、瓶颈文档。
- ARCH.md 与 partition-contract 归 S3。

**六、与 S5（C++ 脏快照 + retention / PVC）**

只有 PROGRESS 和 launch-todo 两份文档重叠，都只追加。

**七、工作区未提交改动**

docs/PROGRESS.md、docs/ops/godb-pipeline-launch-todo.md、tools/scripts/tests/k8s_deploy_contract.tests.ps1 在工作区里有别的会话未提交的改动。落码前重新 git status / 读取；只追加，不回退、不整理他人的行。

## 测试
### go/db/internal/kafka/ordering_lease_test.go
替身：miniredis 加 fake orderingLeaseTiming（可控 now；after 把请求时长写进 requested 通道并返回由测试触发的通道；jitter 恒等），不依赖墙钟。

- TestOrderingLease_RenewSuccessExtendsDeadline：触发一次续租成功，localDeadline 前移到续租发出时刻 + TTL − margin，没有失租。
- TestOrderingLease_RenewErrorRetriesWithBackoffThenRecovers：mr.SetError 之后连续两次失败，requested 依次为 500ms、1s，renew_retry_total +2；mr.SetError("") 之后恢复，没有失租。
- TestOrderingLease_RenewErrorsExhaustAttemptsCancelSQL：一直失败，第 10 次 setLost(renew_exhausted)；beginSQL 交出的 ctx 被取消，context.Cause 里包着 errOrderingLeaseLost；lease_lost{renew_exhausted} +1。
- TestOrderingLease_RenewDeadlineCancelsSQL：时钟推过 localDeadline，结果为 renew_deadline。
- TestOrderingLease_OwnerChangedIsLostImmediately：用 mr.Set 把锁键改成别的 token，下一次续租就失租（owner_changed），不重试。
- TestOrderingLease_WorkerStopDoesNotCancelSQL：取消 w.ctx 之后 sqlCtx.Err()==nil，lost()==nil。
- TestOrderingLease_BeginSQLPastLocalDeadline：时钟越过截止时间后调用 beginSQL，返回 errOrderingLeaseLost，handler 不会被调用。
- TestOrderingLease_ReleaseSkipsAfterAbandonedSQL：beginSQL → setLost → finish(context.Canceled) → release；锁键仍在 miniredis 里，lock_release{skipped_sql_abandoned} +1。
- TestOrderingLease_ReleaseOutcomes：正常释放后键被删，记 released；释放前锁键被改写，记 not_owner；mr.SetError 时记 error。
- TestOrderingLease_ReleaseCancelsSQLContext：释放后 sqlCtx.Err()!=nil。
- TestOrderingLease_VerifyCanceledContextIsNotLeaseLoss：用已取消的 ctx 调 verify，返回错误，但 lost()==nil。
- TestLeaseMarginCoversLockWaits：表驱动，10/10 通过；-1/10 报错；20/10 报错。

### go/db/internal/kafka/run_db_op_test.go
替身：fakeLease（实现 writeLease，记录调用顺序，可注入 beginSQL / verify 错误，可返回已取消的 ctx）；healthStores（fakeStores 加 storeHealth 的 ReportConnLost / ReportHealthy 计数）。runDBOp 的 publish 用独立的 redisClient，以便注入缓存失败。

- TestRunDBOp_WriteRequiresLease：写任务 lease=nil 时返回错误，handler 调用 0 次。
- TestRunDBOp_LostBeforeSQLSendsNothing：beginSQL 出错时 handler 0 次，返回值满足 errors.Is(errOrderingLeaseLost)。
- TestRunDBOp_VerifyBeforeCachePublication：verify 返回失租时缓存键不存在；正常时调用顺序为 handler → confirm(MGET) → verify → SET。
- TestHandleTask_LeaseLostAfterSQLDoesNotPublishCache：用 newTestWorker；harness 的 afterCall 钩子把排序锁键改成别的 token。缓存键不存在，游标未推进，ready 里有一条；deferred{write,lease_lost} +1。旧代码下这条用例为红。
- TestRunDBOp_HandlerGetsLeaseContextForWrites：写任务 handler 拿到的 ctx 就是 fakeLease 的 ctx；读任务拿到的是 runDBOp 的 ctx。
- TestRunDBOp_ReportsOnlySQLOutcome（L-04 第 8 条，表驱动）：
  - handler 返回 KindConnLost：ReportConnLost(storageID) 1 次，Healthy 0 次；
  - 成功：Healthy 1 次；
  - 复核失败、缓存失败、verify 失败：ConnLost 都是 0 次。
- TestRunDBOp_CanceledSQLIsNotReported：sqlCtx 已取消，handler 返回 KindConnLost 包裹的错误，ConnLost 与 Healthy 都是 0 次。
- TestRunDBOp_ObservesSQLStage：db_task_stage_seconds{op=write,stage=sql} 的 count +1。

### go/db/internal/kafka/consumer_shutdown_test.go
替身：fakeClaim、fakeConsumerGroup（按 sarama v1.43.1 语义：任一 ConsumeClaim 返回即取消会话 ctx；Close 记录事件和当时的 c.ctx.Err()）、recordingSession（ctx 可取消）、recordingHarness（钩子可阻塞）。

- TestConsumeClaim_ShutdownDrainsDispatchedTasks：分发 3 条，harness 阻塞第 2 条；close(pullStop)；放行之后 ConsumeClaim 返回 nil，3 个 offset 全部被标记。
- TestConsumeClaim_ShutdownStopsTakingMessages：pullStop 之后再往 claim 里放的消息不会被 track、不会被处理。
- TestConsumeClaim_ShutdownIgnoresSiblingSessionCancel：排空期间取消会话 ctx，claimCtx 不被取消，在途任务完成并被标记。
- TestConsumeClaim_ShutdownDrainTimeoutCancelsRest：shutdownDrainTimeout 设 50ms，阻塞的任务不放行；按时返回 nil，acker 进入 failed，更高的 offset 不被标记。
- TestConsumeClaim_RebalanceStillCancelsClaim：不关 pullStop，只取消会话，claimCtx 被取消（原有行为不变）。
- TestStop_OrderPullDrainLeaveCancelRelease：
  - 用真实 Start()，retryConsumeInterval 设 1h；fake Consume 驱动一个 claim；本实例 processing 预置一条收据。
  - Stop 之后断言：Close 发生时 c.ctx 尚未取消、processing 仍非空；Stop 结束时 ctx 已取消、收据回到 ready、consumeDone 已关闭。
- TestStop_Idempotent：第二次 Stop 不 panic、不阻塞。
- TestStop_BeforeStartDoesNotWait：没有 Start 就 Stop，不等 consumeDone，立即返回。
- TestRetryConsumer_StopsClaimingAfterPullStop：claimCounter Hook 计数，pullStop 之后调用 consumeRetryQueue，认领 0 次。

### go/db/internal/kafka/l04_deferral_test.go
- TestRetryBudgetExempt_StoreTaggedErrors：四类 storeerr 标记写豁免、读不豁免；errOrderingLockBusy / errPlacementFrozen / errPlacementMoved 读写都豁免；errPlacementStoreUnavailable 只写豁免；未打标记的 &net.OpError、timeout net.Error 写不豁免。
- TestDeferReason_EveryReason：13 个取值逐一覆盖；storeerr 标记包住 errPlacementStoreUnavailable 时归 conn_lost；未打标记的 DeadlineExceeded 与 Redis 超时归 redis_unavailable；普通错误归 other。
- TestConsumeRetryQueue_ClaimsAtMostReadyLengthPerTick：ready 预置 3 条，claimCounter 推回模式下一拍恰好认领 3 次（旧代码 200 次）；RetryCount 每条只 +1。
- TestConsumeRetryQueue_TwoConsumersEachCapped：两个消费者实例 ID 不同、各用独立 client 连同一个 miniredis，各自 3 次。
- TestConsumeRetryQueue_SkipsTickWhileHomeStoreCoolingDown：认领 0 次；db_retry_queue_length 的 ready=3、processing=0、dead=0。
- TestConsumeOneRetryTask_ExhaustedReadIsDroppedNotDeadLettered：dead 长度 0，processing 长度 0，read_abandoned{budget_exhausted} +1。
- TestStoreDeferral_RetryWriteKeepsBudgetAndPayload：4 轮之后 ready 有 1 条、dead 为 0、RetryCount==0，LIndex -1 的字节与初始载荷完全相同；deferred{write,conn_lost} +4。
- TestDeferAndDeadLetterCounters：errPlacementFrozen 让 deferred{write,placement_frozen} +1；quarantineOrderingConflict 让 dead_letter{ordering_incomparable} +1；persistPoisonMessage 让 dead_letter{poison} +1；ConsumeClaim 毒消息路径同样 +1。

### go/db/internal/kafka/instrumentation_test.go
helper metricSample(family, labels) 经 DefaultGatherer 读取 count / sum，用差值断言。

- TestRedisSteps_KafkaWriteObservesEachStepOnce：一条 Kafka 来源写，OwnerEpoch=3、seq>0，走 handleTask 成功；metrics.RedisSteps 的 11 个取值在 op=write 下各 +1。
- TestRedisSteps_ReadObservesReadSteps：读任务下 placement_lookup / expand_get / placement_recheck / cache_set 在 op=read 下各 +1，其余 7 个为 0。
- TestOrderingGuardCounter_Outcomes：首写记 apply_no_cursor；同分区更大 seq 记 apply；更小 seq 记 stale；另一分区记 incomparable；mr.SetError 时记 error；读任务不计数。
- TestSubShardBatchSizeObserved：先往 subShardChans[0] 预放 3 条再起 runSubShard，batch 直方图 count +1、sum +3。
- TestStages_QueueWaitTaskTotalKafkaE2E：
  - enqueuedAt 非零时 queue_wait +1；每次 handleTask 都让 task_total +1；
  - Timestamp 非零的 Kafka 任务让 kafka_e2e +1；重试任务和 Timestamp 为零的任务 kafka_e2e 不变；
  - Timestamp 在未来（时钟偏差）时不观测。

### go/db/internal/kafka/applied_epoch_lua_test.go
- TestMarkAppliedEpoch_DecimalCompareBeyondDoublePrecision，表驱动：
  - 无键时写入 7，结果为 7；
  - 存量 9007199254740992 写入 9007199254740993，结果为 ...993（旧脚本不推进，红）；
  - 存量 18446744073709551614 写入 18446744073709551615，推进（旧脚本红）；
  - 存量 9007199254740993 写入 ...992，不变；
  - 存量 10 写入 9，不变（防止只做字典序比较的错误修法）；
  - 存量 9 写入 10，推进；
  - 写入 0 时无键仍无键、有键不变。
- TestMarkAppliedEpoch_RejectsNonCanonicalStoredValue：存量 abc、007 时 MarkAppliedEpoch 返回 error，值不变；ReadAppliedEpoch 的既有语义不变。

### go/db/internal/metrics/metrics_test.go
- TestStoreCooldownCollector_EmitsAllPools（L-04 第 11 条）：source 返回 {home:2} 时输出 home=2、extra_zone=0、extra_non_zone=0；nil map 时三项都是 0。
- TestRedisPoolCollector_SharedClientExportedOnce：main 与 placement 是同一个 fake 指针时，只有 redis_client=main 的一组序列。
- TestRedisPoolCollector_SeparatePlacementClient：两个不同指针输出两组；hits_total、wait_seconds_total（WaitDurationNs 换算成秒）、max_conns=PoolSize 都正确。
- TestRedisStepNamesPinned：len(RedisSteps)==11，去重后仍为 11，顺序等于写路径字面列表。

### go/db/internal/logic/pkg/proto_sql/service_pool_test.go
- TestNewServiceMysqlConfig_SetsTimeoutsAndSessionWaits（L-04 第 9 条）：Dial / Read / Write 为 5/30/30 秒；Params 里 lock_wait_timeout=10、innodb_lock_wait_timeout=10，sql_mode 保留。
- TestNewServiceMysqlConfig_NegativeDisables：值为 -1 时三个超时都是 0，两个会话变量不存在。
- TestNewMysqlConfig_HasNoServiceTimeouts：没有超时，也没有两个会话变量。
- TestPingStore_UsesConnectorNotPool（L-04 第 10 条）：Connect 1 次、Ping 1 次、Close 1 次；池连接器的 Connect 0 次。
- TestPingStore_FullPoolDoesNotBlock：池 SetMaxOpenConns(1) 并持有 db.Conn；在 1 秒预算内返回 nil。
- TestPingStore_DeadPoolConnsDoNotMatter：池里的连接 Ping 都返回 driver.ErrBadConn，pingStore 仍返回 nil。
- TestPingStore_ConnectErrorIsConnLost：Connect 返回 *net.OpError 时 storeerr.Classify 结果为 KindConnLost。

修改 config.AppConfig 的用例必须在 t.Cleanup 里还原，且不能 t.Parallel。

### tools/scripts/tests/go_services_stop.tests.ps1
- 优雅：Ctrl+C 发出且在时限内退出，Stop-ProcessTree 0 次，PID 记录被移除，输出 [stop]。
- 超时：Ctrl+C 发出但不退出，Stop-ProcessTree 1 次，输出 [kill]，包含 no exit within。
- 发不出：Send 返回 $false 时立即强杀，Wait-ProcessExit 0 次。
- -ForceStop：Send 0 次，直接强杀。
- 时限：db 用 35，chat 用 25；-StopTimeoutSeconds 10 时全部用 10。
- 并行：两个实例的调用顺序为 Send A、Send B、Wait A、Wait B。
- 剩余时间扣减：Get-StopClockSeconds 桩依次返回 0、0、0、12；B 的 Wait 参数是 25−12。
- 已退出：输出 [gone]，不发 Ctrl+C。
- 辅助脚本文本：Get-ConsoleCtrlCHelperScript 1234 包含 AttachConsole(1234)、GenerateConsoleCtrlEvent(0, 0)、SetConsoleCtrlHandler，且以 exit 0 结束。
- 契约：AST 扫描 Start-ServiceInstance 内的 Start-Process 调用，都带 -WindowStyle Hidden，且没有 -NoNewWindow。

### tools/scripts/test_stress_summarize.ps1
- 原 t18m_db 的 DB 段断言改为按 'DB task stages' 段定位，6 个平均值不变。
- 'DB write path' 段 t20m_db 行：w_sql 2.0ms、w_task 20.0ms、w_queue 1.0ms、w_e2e 50.0ms、r_sql 3.0ms、r_task 6.0ms、batch 3.0、no_cursor 4、lease_lost 3、rel_notown 1、rpool_wait 5（只有 main，不翻倍）、rpool_tmo 0。
- 同段 t18m_db 行：除 snapshot 外全为 n/a。
- 'DB Redis steps' 段 t20m_db 行：11 个互不相同的平均值按 RedisSteps 顺序排列，再加 sum；t18m_db 行全为 n/a。

### tools/scripts/tests/k8s_deploy_contract.tests.ps1
「db 是多副本形态」用例新增一条断言：Deployment 块里有 minReadySeconds: 20。replicas、strategy、PDB、反亲和、terminationGracePeriodSeconds: 45 的断言保持不变。

## 上线与回滚
**交 Codex 验证**（Claude 不跑，交付说明写「未编译，待 Codex 验证」）

环境与准备：
- 环境变量照 Codex 任务单 §1：GOTOOLCHAIN=local、GOFLAGS=-mod=readonly、GOPROXY=https://goproxy.cn,direct（负责人 2026-10-15 已同意联网下载）。Go 1.27.2，路径 C:\Program Files\Go\bin\go.exe（负责人已确认，不重装）。
- 建两个临时 worktree：被测建在含 S4 的提交上，基线建在它的父提交上。做法与收尾照任务单 §2.1 / §2.6，不在主工作区跑。
- internal/kafka 的用例依赖 miniredis 监听 127.0.0.1，沙箱拦截回环时先申请放行。

命令（工作目录为 worktree 下的 go/db）：

```sh
gofmt -l internal/kafka internal/metrics internal/logic/pkg/proto_sql internal/migrate .
go vet ./...
go build ./...
go test ./internal/kafka/... ./internal/metrics/... ./internal/logic/pkg/proto_sql/... ./internal/migrate/... -count=1 -v
# race 需要 CGO 与 gcc;Windows 上没有就到 Linux / WSL 跑,并写明平台
go test -race ./internal/kafka/ -run 'TestOrderingLease|TestConsumeClaim_Shutdown|TestStop_|TestRunDBOp|TestRetryConsumer_StopsClaiming' -count=1
```

在仓库根目录执行（PowerShell 7）：

```sh
pwsh -File tools/scripts/tests/go_services_stop.tests.ps1
pwsh -File tools/scripts/tests/go_services_ports.tests.ps1
pwsh -File tools/scripts/test_stress_summarize.ps1
pwsh -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
```

通过标准：
- gofmt 无输出，vet 与 build 退出码为 0；
- 新用例全部 PASS，SKIP 为 0；
- 既有用例的失败集合是基线失败集合的子集；
- 失败时保留第一条错误及其前后 20 行，以及 go test -v 的失败用例全文；
- 红了就停下归因，不连续重试。

**本地故障注入**（只在 compose 或 kind 上，由 Codex 执行）：
1. 优雅停：用 data_stress 持续写入，执行 go_services.ps1 -Command stop -Services db。
   - 通过标准：db 日志依次出现 Shutting down gracefully、claim drained for shutdown、db consumer stopped took<15s；没有 [kill]；本实例的 kafka:retry:processing:*:<实例> 为空；重启后 lag 正常排空、死信不增长。
2. 续租失败：在 compose 的 Redis 上执行 CLIENT PAUSE 70000 WRITE。
   - 通过标准：db_ordering_lease_lost_total{reason=~renew_deadline|renew_exhausted} 增长；在途写的锁键留到 TTL；恢复后抽样玩家的 MySQL 行等于最后一次写入。
3. 锁被接管：另一个会话执行 LOCK TABLES player_database WRITE 让 SQL 挂起，同时手工改写测试玩家的排序锁键。
   - 通过标准：skipped_sql_abandoned +1、owner_changed +1；解锁后行最终等于最新版本。
4. 回滚兼容：新旧二进制混跑，旧实例对新实例保留的锁只报 errOrderingLockBusy（不耗预算）。

**线上发布**（负责人执行，按瓶颈文档 §4.5）：
- 低峰进行；一次一个 zone，每个 zone 一次一个 Pod。
- 每换一个 Pod，核对：lag、重试队列、processing、死信、db_ordering_lock_release_total{not_owner} 与 db_ordering_lease_lost_total 都为 0；Stop 日志里的 took 值；没有 shutdown drain timed out（有积压时允许出现）。
- 兼容性：没有键名或值格式变化（applied_epoch 仍是规范十进制），新旧版本混跑安全；回滚直接换回旧镜像，不需要清理。
- db.yaml 的 minReadySeconds 不会单独触发滚动，在下一次滚动时生效。
- 告警需手工 apply：kubectl apply -n observability -f deploy/k8s/db-task-retry-alerts.yaml。

**对比口径**：
- 写任务的 op_handler 多了一次 verify 往返；verify 失败由原来记 ok 改为记 db_task_result_total{error}。
- R20 作为 DB 段的新基线（第 0 批的 subShardIndex 打散也包含在内）。R20 与 R19 只比 task_total、排空时长、Redis commandstats；robot / login 段照常与 R19 对比。
- 用 performance_schema / Com_stmt_prepare 核实 Save 的实际语句数（附录 B redis-merge 修正 10），属于运维项。
- R20 按 AGENTS §6 全流程执行，突发全量存盘场景待负责人确认。

**登记与回写**：
- Java 版：在 D:\luyuan\xuanming-server-mmo\PARITY.md 追加一行：G1 治标、停机顺序、go_services 优雅停、db 测量打点、Lua tonumber（go/db）均为「不适用」（Java 没有 Kafka 存盘链，由 scene 直接写 MySQL）。
- Codex 结果回来后，回写 launch-todo §4 的状态与瓶颈文档 §7。

## 待决项（负责人已按推荐默认定）
- S4 全部条目（G1 治标、停机顺序、go_services.ps1、测量打点、Lua tonumber、注释订正、L-04 单测）是否都在负责人当前对话的开工授权之内？交接 §0 第 6 条规定：授权只认负责人在当前对话里的明确答复；§4.1 第 2 行把 G1 写成「动代码前问」。本任务给出的「把代码做完」来自脚本文本，我无法核实。
  - 采用：按 2026-10-13 的拍板加「把代码做完」直接落码（28 个文件，不超过 30，不动 proto / DDL / 玩家可见行为）。如果负责人只授权了部分条目，先落测量打点、注释订正、L-04 单测、Lua 这几项，G1 治标与停机顺序等确认后再合。

## 风险
1. **不是栅栏**
   - 被取消的写语句如果已经到了 MySQL，仍可能提交；驱动只关连接，不发 KILL QUERY。
   - owner_changed 时锁已经不归本实例，「锁留到 TTL」也挡不住，G1 窗口依然存在，只是缩小了。根治靠条件写。
2. **本地截止时间依赖进程的单调时钟**：虚拟机暂停恢复时，SQL goroutine 可能抢在 watchdog 之前发出语句。beginSQL 只复核一次，缩小窗口，不能关闭。
3. **在途 SQL 被取消后锁留到 TTL**：失租原因是截止时间时，剩余约 30 秒；该玩家这段时间的写会排队，报 errOrderingLockBusy，不耗预算。database/sql 还在等连接时就被取消，也会被保守地记为 abandoned。
4. **margin 与会话锁等待耦合**：margin 30 秒要求两个会话锁等待之和 ≤ 25 秒。TiDB 阶段若按 launch-todo 2.5.7 把 SessionLockWaitSeconds 写成 -1，被遗弃语句就没有上界，启动时只打 ERROR。
5. **停机超时被强杀**：写 SQL 在停机时不被取消，最坏要等读超时 30 秒，wg.Wait 可能让总时长超过 40 秒，被 SIGKILL。这是安全的：锁留到 TTL，收据等租约过期后回收。
6. **对 sarama 行为的依赖**：停机排空依赖 v1.43.1 的三条行为——首个 claim 返回即取消会话、release 时 flush 提交、心跳持续到 release，都已读源码确认。升级 sarama 要重新核实；claimCtx 改为 WithoutCancel 加观察 goroutine 后，再平衡路径的语义必须保持。
7. **指标口径变化**：写任务的 op_handler 多了 verify 往返；verify 失败改记 result=error。R20 对比时要按这个口径。
8. **kafka_e2e 有时钟偏差**：用的是生产者的 CreateTime，跨机有偏差；值为负时丢弃；超过 5 秒落进 +Inf 桶。
9. **go-redis PoolStats 回绕**：字段是 uint32，回绕时会被当成 counter reset。
10. **Ctrl+C 辅助方案未在本机实测**：
    - 依赖目标进程有控制台（Start-Process 不带 -NoNewWindow，推断每个实例有独立的隐藏控制台）；AttachConsole 失败时退回旧的强杀行为。
    - 每次停机要编译一次 Add-Type，约 1 秒。
    - 如果将来改成共享控制台，Ctrl+C 会波及其它服务，已用契约用例钉住。
11. **合并冲突**：key_ordered_consumer.go 与 S1、S2、S3、条件写会话都有重叠，按 ordering_and_overlap 的合并顺序处理。
12. **失租仍耗重试预算**：L-04 的口径不变。由于续租改为有界重试，失租频率应当低于现状。
13. **既有问题未修**：go-redis 默认 MaxRetries=3，单条 SETNX 读超时重发时，可能把自己刚设上的锁判成「被占」，锁悬挂 120 秒。本项不修。
14. **全部未编译、未测试**；R20 之前没有打点数据。
