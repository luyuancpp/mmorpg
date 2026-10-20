<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**summary**: v0.3 的主体设计按文意可以实现：围栏模型、模式×类别矩阵、结局处理表、计数器探测、override 加原子推进的不变量、分期与回退都能自洽。§4.3 / §4.4 对 proto2mysql e81a1e7 中 SaveConditional 的语义假设已逐条读源码核对，全部成立。字段编号与现状一致：player_database 18/19、player_database_1 3/4 已在 HEAD 声明，收件箱占 20；DBTask 9、PlayerAllData 7 已出现在在途改动里，号位与设计一致。但设计有一批正文是在 go/db 的 A3（G1 治标、Stop 五步、luaDecimalGreaterFn）入库之前写的，落码前必须重写，否则会写出错误代码。具体是：§5.5 的推进脚本仍用 tonumber，并且先写游标、后校验 epoch，脚本中途出错不会回滚，游标会前进而 epoch 不动，破坏它要保护的不变量；§4.6 提议「Stop 时取消写 SQL」，与已落地租约 ctx 的语义正面冲突（只在失租时取消）；§6.1 / §6.2 没有按 runDBOp 里的 verify 重写；§5.2 / §10 把已经存在的 db_ordering_guard_total 当成新指标，也没提 RedisSteps 与 stress_summarize 的固定契约；Stage 0 的放行口径「plan 只有 4 条 fence ADD COLUMN」已过时。更紧迫的是，工作区里有来源不明的在途实现，同时改了 go/db/internal/kafka 与 C++ 发号。它与 v0.3 正面冲突，自身也有确定的丢盘与 L-04 回退路径：拒绝的写被静默 ack 并推进游标；存储错误没有 storeerr 标记；没有 Mode 开关，滚动升级时 class-2 被静默拒绝；C++ 用的是设计已否决的每玩家计数器。它还违反了交接 §3.4「不动 internal/kafka」的约定。上线前须先由用户裁决这份在途实现的去留。
## findings
  - [0]
    **id**: F1
    **severity**: high
    **title**: 在途 go/db 行围栏实现：被拒写静默 ack 且推进游标、不复核、存储错误丢失 storeerr 标记
    **location**: go/db/internal/kafka/player_row_fence.go:17-53（未跟踪，在途）；go/db/internal/kafka/key_ordered_consumer.go:952-954、2428-2434（未提交在途）；连带 HEAD 的 key_ordered_consumer.go:1906-1910（handleTask 尾部 markAppliedSeq / markAppliedOwnerEpoch）
    **detail**: (1) 静默 ack 并推进游标。runDBOp 遇到 errPlayerRowStale 直接 return nil（:2431-2434），handleTask 按成功处理，执行 markAppliedSeq + markAppliedOwnerEpoch 后 ack。stale_owner（B1）和行上已有版本时的未版本化写（B3）都被静默丢弃，不进死信，违反设计 F-2 / F-3。推进游标还会丢掉更新的写。例：行内 (E2,s1)，游标 (p,80)；僵尸 E1 的写在 offset 100 被 SQL 拒，但游标被推进到 100；新主 (E2,s2) 在 offset 90 的重试回来，被判 orderingStale 并丢弃，MySQL 停在 s1。
(2) 不复核。older 判定发生在 route.confirm 之前就返回（设计 §4.4 要求先复核）。搬库窗口里，旧库判 older 的写被 ack，新库缺这一版。
(3) 存储错误没有标记。savePlayerRow 把 RunInTransaction 的错误原样返回（:952-954），没套 storeerr.Tag。retryBudgetExempt / deferReason 只认标记（store_failure.go:27、79-100），而 deferReason 注释写明「剩下的网络错误只可能来自 Redis」。MySQL 宕机或 1205/1213 时，玩家写会耗尽预算进死信，conn_lost 不触发探活，原因还被误记成 redis_unavailable，等于把 L-04 第 1 批的保护整体回退。
(4) reportStoreResult 在分支之前就收到非 nil 的 errPlayerRowStale（:2428），这正是设计 §4.1 明确要避免的。
(5) 用 FindOneByPKForUpdate 在显式事务里整行读（MEDIUMBLOB）再 Save，每次写多读一整行。proto2mysql 注释指出显式事务里并发首插可能 1213。
    **suggested_fix**: 交用户裁决这份在途实现：撤回，或作为过渡版本重做。按 v0.3 落码时至少要满足以下几点：错误一律套 storeerr.Tag；被拒结局以 (outcome, nil) 交回 handleTask 分类，older 先 confirm 再 ack，stale_owner / legacy_after_seq 走 quarantineOrderingConflict 进死信；被拒的写不调 markAppliedSeq / markAppliedOwnerEpoch；SQL 改用 SaveConditional（autocommit，只读围栏列）。在 go/db 的 ④ 与保留期落完之前，按交接 §3.4 的约定不改 internal/kafka。
  - [1]
    **id**: F2
    **severity**: high
    **title**: 在途 go/db 实现没有 Fencing.Mode，class-2 一律盖 (0,0)：滚动升级与 C++ 回滚时静默丢盘
    **location**: go/db/internal/kafka/player_row_fence.go:28-36（在途）
    **detail**: versioned := epoch != 0 && seq != 0；非 versioned 的写被盖成 (0,0)，只要行内围栏大于 (0,0) 就返回 errPlayerRowStale，再按 F1 被静默 ack。失败场景：同一 zone 新旧 scene 混跑。玩家先在新 C++ 节点落了 (E,s)，换到旧 C++ 节点后 epoch 变为 E'>E，旧节点发出 class-2 写 (E',0)，被盖成 (0,0) 后小于 (E,s)，此后在旧节点上的每次存盘都被丢，没有日志级告警（只有一行 INFO）。C++ 从新版回滚到旧版时，全服所有已落过版本的玩家都会这样丢盘。设计 §4.3 的 class-2 两步写、§8 的 1a→1c→1b 分期与 §8.6 的「dual → legacy 随时安全」回退，在这份实现里都不存在：新二进制一上线就等于强制 dual，而且没有退路。
    **suggested_fix**: 按 §4.7 加 Fencing.Mode（缺省 legacy），按 §4.3 实现 class-2 两步写和 class-3 的 AllowEqual 写法。上线顺序按 §8：新 go/db 以 legacy 上线，再上新 C++，最后逐 zone 切 dual。
  - [2]
    **id**: F3
    **severity**: high
    **title**: 在途 C++ 用每玩家计数器发 save_seq，且把围栏写进 body：与 §7 / §3.2 冲突，崩溃重启后撞号
    **location**: cpp/libs/services/scene/player/comp/player_ownership_comp.h:104-105；cpp/libs/services/scene/player/system/player_lifecycle.cpp:755-765、4128-4133、4367-4381、4313（均为未提交在途）
    **detail**: 在途实现把 lastSaveSeq 挂在 PlayerOwnerEpochComp 上，播种只取 max(父键 last_save_seq, 两张子表 fence_save_seq)，然后 ++。没有进程级水位、没有墙钟下限、也没有 Observe 上界，正是设计 §0.1 第 1 行和 §15 第 2 行否决的方案。§7.1 写明父键不是「已发出 DBTask」的上界。
失败场景：scene 崩溃前已发出 DBTask(E,s)，Redis 父键还停在 s-1；同 node_id 的新进程在 etcd 租约过期后接回玩家，scene_manager 走不铸造分支（§7.4 条件 1、2）；新进程播种得 s-1，下一笔又发 (E,s)，但 body 不同。在在途 go/db 下，这一笔返回「同版本载荷冲突」错误，耗预算进死信。父键若更旧，新写得到 (E,s'<s)，被判旧后静默 ack（叠加 F1），直到 epoch 推进前都丢盘。设计的进程级发号器在这里还有墙钟下限兜底，只有时钟回拨时才会出错。
另外三处不一致：在途实现把 (epoch, seq) 写进子表 body 和父键（:755-765、:4378-4381），违反 §3.2「C++ 存盘不写」和 §4.2「body 恒为 0」的前提；快路径改成调 IsPersistedPayloadCurrent（:4313），每次存盘深拷两份 PlayerAllData，热路径成本上升；同时顺带剥掉了 stress_test_probe，改变了 §7.3 记录的压测口径。
    **suggested_fix**: 按 §7.2 改成 SaveSeqAllocator（放在 tlsIdGeneratorManager 上，Next = max(last+1, nowMs<<10)，Observe 带上界），用纯函数 SaveSeqFloorFrom 播种。C++ 不写 body 围栏，按 §7.3 在 Replace 与 IsPersistedPayloadCurrent 里清 last_save_seq。如果用户决定保留在途方案，必须先回改设计 §0.1、§7、§15，并给出崩溃重启场景下的单调性证明。
  - [3]
    **id**: F4
    **severity**: medium
    **title**: §5.5 推进脚本仍用 tonumber，且先写游标、后校验 epoch：出错时留下部分写，破坏它要保护的不变量
    **location**: docs/design/db-write-fencing-mysql.md:359-384（§5.5）；对照 HEAD go/db/internal/kafka/key_ordered_consumer.go:619-678（luaDecimalGreaterFn / luaMarkAppliedEpoch）
    **detail**: (1) 契约冲突。go/db 已把 epoch 比较改成十进制串比较，并在注释里写明「条件写等后续脚本要比较 uint64 时直接拼这一段，不要再写 tonumber」（KOC:619-627）。§5.5 却仍写 tonumber，还说「同现有 luaMarkAppliedEpoch 的注释」，而那段注释现在的意思正好相反。epoch 超过 2^53 时（例如 debug_import 从别的环境带来大 epoch，或 epoch-raise 加了很大的余量），相邻值舍入后相等，e > cur 判假：游标前进而 epoch 没推进。tonumber 还会接受 '0x10'、' 5 '、'1e3' 这类非规范值，与 ReadAppliedEpoch（ParseUint）和 is_dec 的校验口径不一致。
(2) 部分写。脚本在 SET 游标（KEYS[2]）之后才校验 KEYS[3]，此时才可能返回 error_reply 或触发 Lua 运行期错误。Redis 不回滚脚本里已经执行的写，于是留下「游标已推进、epoch 未推进」，正是 §5.5 不变量禁止的状态。之后同 epoch 更旧的重试一旦走 §5.2 override，就会被放行。触发条件是守卫读与脚本之间 epoch 键被改坏，概率低，但脚本的唯一价值就是原子性。
    **suggested_fix**: 脚本改为 luaDecimalGreaterFn 加以下顺序：先核令牌；再用 is_dec 校验 ARGV[3] 和已存的 epoch（GET KEYS[3]），任何不合法值直接返回错误，此时还没有写任何键；最后才 SET 游标，并在 ARGV[3] ~= '0' 时，若键缺失或 dec_gt(e, cur) 则 SET epoch。删除「Lua double 精确」那句。§9.1 epoch-raise 的「只升不降 Lua」同样拼 luaDecimalGreaterFn。§11 第 2 步补用例：epoch 键非规范时脚本报错，且游标未被改动。
  - [4]
    **id**: F5
    **severity**: medium
    **title**: §4.6 的 ctx 方案与已落地的 G1 治标冲突（「Stop 时取消写 SQL」会把 G1 回档带回来）
    **location**: docs/design/db-write-fencing-mysql.md:254-258（§4.6）、:166；对照 HEAD go/db/internal/kafka/ordering_lease.go:5-12、beginSQL / finishSQL，key_ordered_consumer.go:927-931、950、2639-2685（stop 第④步）
    **detail**: §4.6 的现状描述已经过时：设计说 handleDBWriteOp「仍丢弃 ctx」，而 HEAD 已经是 store.WithContext(ctx).Save(msg)（KOC:950），ctx 来自 orderingLease.beginSQL，以 Background 为父，只在失租时取消。代码注释明确写着停机与 claim 收回都不取消在途写 SQL，失租取消后锁留到 TTL（sqlAbandoned）。§4.6 要求「Stop 时取消」。照做的话，Stop 第④步取消后，lease.sqlCtx 本身没被取消，finishSQL 不置 sqlAbandoned，releaseOrderingLock 立即删锁；服务端仍可能提交那条语句。legacy 模式没有围栏，别的实例写入新版本后，旧语句再提交就是回档。
另外，§5.5 / §6.2 的两条 Redis 脚本没写用哪个 ctx。用 w.ctx 的话，Stop 第④步先 cancel 再 wg.Wait：已落库的重试源任务在推进脚本处失败，被记为 redis_unavailable 延后并耗预算，每次滚动重启都会产生这类噪声。
    **suggested_fix**: 重写 §4.6：写 SQL（含 class-2 两步）一律用租约的 sqlCtx，不加 Stop 取消；§4.1 删掉「改为 WithContext」的工作项。SQL 之后的复核、缓存令牌脚本、推进脚本，改用 context.WithoutCancel(ctx) 加有界超时（同 releaseOrderingLock 的写法），ctx 取消不算失租。§11 第 2 步补 Stop 第④步与推进脚本交错的用例。
  - [5]
    **id**: F6
    **severity**: medium
    **title**: §6.1 / §6.2 / §5.4 没按 runDBOp 内的 verify 重写：令牌往返变成三次，失租记账口径未定义
    **location**: docs/design/db-write-fencing-mysql.md:405-428（§6.1、§6.2）、:343-351（§5.4）、:243-252（§4.5）；对照 HEAD key_ordered_consumer.go:2380-2437（runDBOp，verify 在 :2423）、ordering_lease.go 的 verify / setLost / releaseOrderingLock
    **detail**: §6.1 说「写路径的缓存回写在核锁之前」，这已过时：现在是复核 → verify（EXTEND 令牌续期）→ 普通 SET。§6.2 提议用令牌脚本代替 SET，却没说 verify 去留。照字面落码，每次写有三次令牌往返（verify、缓存脚本、推进脚本），而 §6.2 声称「往返数不变」。
脚本返回 0 时，设计只说包 errOrderingLeaseLost，没要求调 lease.setLost。结果是 db_ordering_lease_lost_total 不计数，释放阶段又按 not_owner 打「MySQL may hold an older snapshot」ERROR，db-task.ordering-lease 告警组的口径被打乱。verify 对「ctx 取消不算失租」有专门语义，脚本也需要对齐。
§4.5 写的顺序漏了 verify，§5.4 的现状描述（写完才核锁）也已过时。
    **suggested_fix**: 改为由缓存令牌脚本取代 verify：脚本里 GET 锁等于令牌 → EXPIRE 锁 TTL → SET 缓存 EX → 返回 1；成功后 extendValidUntil；返回 0 时 setLost(owner_changed)；ctx 取消按 verify 的口径处理。§4.5 的顺序改写为：beginSQL → SQL → finish → reportStoreResult → 复核 → 令牌发布（被拒结局在复核之前返回）。同步改 §5.4 的现状一段。
  - [6]
    **id**: F7
    **severity**: medium
    **title**: §5.2 / §10 把已存在的 db_ordering_guard_total 当成新指标；RedisSteps 的固定契约未处理
    **location**: docs/design/db-write-fencing-mysql.md:330、780、919；对照 HEAD go/db/internal/metrics/pipeline.go:10-19、117-121、123-132，key_ordered_consumer.go:525-572（orderingForTask 内计数），metrics_test.go:291-293，tools/scripts/stress_summarize.ps1:683、713、730
    **detail**: db_ordering_guard_total 已随 A3 入库，outcome 有 apply / apply_no_cursor / stale / incomparable / error 五个值，计数发生在 orderingForTask 内部。设计按新指标写成 apply / stale / stale_no_epoch / incomparable / epoch_override。照做的后果有三种：重复注册同名 Vec 直接 panic；新取值不在 orderingGuardOutcomes 里，启动时不预建序列；override 路径先在 orderingForTask 计 stale、再计 epoch_override，stale_no_epoch 与 stale 也重叠，导致重复计数。
§5.5 把 cursor_set / epoch_mark 合成一个脚本，§6.2 改动 lease_verify / cache_set，都会改变 11 个固定 step 的含义。stress_summarize.ps1 的 2e 段按精确名字取列（:730），TestRedisStepNamesPinned 会失败，压测对比表的口径也随之断裂。§12 没列这些文件。
    **suggested_fix**: §10 改为「扩展现有指标」：在 orderingGuardOutcomes 里加 stale_no_epoch 和 epoch_override，并写清计数规则（orderingForTask 照计 stale，override 路径额外计 epoch_override，或改为读完 epoch 再统一计一次）。定新的 step 名（例如 cache_publish 取代 lease_verify + cache_set，applied_advance 取代 cursor_set + epoch_mark），把 metrics/pipeline.go、metrics_test.go、stress_summarize.ps1 的 2e 段、test_stress_summarize.ps1 列入 §12 与 §11。
  - [7]
    **id**: F8
    **severity**: medium
    **title**: Stage 0 门禁与验证口径过时：「plan 只有 4 条 fence ADD COLUMN」现在不可能成立
    **location**: docs/design/db-write-fencing-mysql.md:148（§3.4 末句）、:609-617（§8.1）、:821-831（§11 第 1 步）、:864-872（§11 第 4 步）；对照 HEAD proto/common/database/mysql_database_table.proto:156-168、186-187，docs/design/guild-phase2/90-consistency.md:302-316
    **detail**: 18/19/3/4 已于 2026-10-15 在 HEAD 先行声明，收件箱的 20 号也已声明。落地恢复交接 §4 第 2 步要求在任何新版 go/db / scene 启动之前完成迁移。因此到条件写 Stage 0 时，plan 要么为空（已迁），要么包含 G-03 清单里的 6+2 列（profile / asset / settlement / fence×2 / inbox，以及 player_database_1 的 fence×2）。执行者照 §8.1 第 2 步、§11 第 4 步的「只有 4 条」核对，会在合法状态下停住，或误判漏迁。现在任何按当前 proto 编出的 go/db 都已经要求围栏列存在（启动闸），「Stage 0 → 1a」中换二进制这一步其实已经被列声明提前触发。
§3.4 写的「取号从 18 起改为从 20 起」已过时，90-consistency.md:302 已写「现从 21 起」。§11 第 1 步「diff 只出现 22 个生成物里的新增字段」也不再成立：fence 字段早已生成，在途改动又生成了 DBTask 9 和 PlayerAllData 7。
    **suggested_fix**: §8.1 改为：以 G-03 最新放行清单为准（player_database 六列、player_database_1 两列），plan 为空，或只含清单内的列；用 SHOW COLUMNS 逐列核对。§11 第 4 步同步。§3.4 末句改为「已由 G-03 登记，现从 21 起」。§11 第 1 步改为只核对 DBTask 9 / PlayerAllData 7 相关的生成物，并注明 fence 字段已在 2026-10-15 生成。
  - [8]
    **id**: F9
    **severity**: medium
    **title**: save_seq 出现了设计未登记的新消费者（在途的资产 SQL 提交证据）；older_seq / equal 的语义与它冲突
    **location**: go/db/internal/kafka/key_ordered_consumer.go:961-970（publishDBWriteResult，在途）；go/shared/assetop/sql_commit.go:17-43（在途）；docs/design/guild-phase2/04-asset-channel.md 在途新增节；设计 §4.4（:214-241）、§12、§14
    **detail**: 在途改动只在 (owner_epoch, save_seq) 都非 0 时发布 PlayerAssetSQLCommit，资产 outbox 的终结要求看到「同纪元同 seq」的 SQL 证据。按 v0.3 §4.4 有两条路径会让它卡住：
(a) 某笔带资产账本的存盘被判 older_seq 并 ack 丢弃（§7.4 时钟回拨、node_id 复用），这个 seq 的证据永远不会发布，outbox 一直等，最终可能被 180 天自动放弃。
(b) §4.4 已接受跨进程同 epoch 撞号时 equal 补发不同的 body；PublishSQLCommit 遇到同版本不同内容会返回错误，publishDBWriteResult 失败后延后，每次重试都得到 equal 并再次冲突，耗尽预算进死信。
§4.4、§12、§14 都没有这一消费方。
    **suggested_fix**: 在 §4.4 结局表加一列「资产提交证据」：applied / equal 才发布；older_seq 不发布，并规定 outbox 应以「行内更新版本已包含该账本」或别的方式收敛。在 §14 增加待拍板项：equal 撞号时证据冲突怎么处理（例如证据按 (E,s) 只升不降、内容冲突只告警不阻塞缓存发布）。与资产通道会话对齐后再落码。
  - [9]
    **id**: F10
    **severity**: low
    **title**: §4.2 与 §8.2 对 legacy 是否盖 (0,0) 说法矛盾
    **location**: docs/design/db-write-fencing-mysql.md:183-190（§4.2 表含 legacy 行）、:627（§8.2「只在 dual 生效：§4.2 盖章」）
    **detail**: §4.2 的表把 legacy 写成盖 (0,0)，§8.2 又说盖章只在 dual 生效。在设计版 C++ 下 body 恒为 0，两种读法结果相同。但只要 body 带了非 0 围栏（在途 C++ 正是这样，见 F3），legacy 不盖章时，PreserveFields 的 INSERT 会把 body 里的值写成新行的围栏；§4.2 的 body_mismatch 计数在 legacy 下是否生效也不确定。
    **suggested_fix**: 明确 legacy 也把两列置 (0,0)：只影响 INSERT 路径，UPDATE 路径反正保留行内值。db_fence_body_mismatch_total 在两种模式下都计数。
  - [10]
    **id**: F11
    **severity**: low
    **title**: 测试计划漏掉受 A3 影响的既有测试；④落地后 §8.3 的排空门禁要覆盖分区化队列
    **location**: docs/design/db-write-fencing-mysql.md:833-855（§11 第 2 步）、:648-653（§8.3 EpochOverride 门禁）、:319、:327（§5.2 关于「重试队列各实例共用」的论证）
    **detail**: handler 签名改成 (writeOutcome, error) 后，替换 dbOpHandlers 的测试不止 recordingHarness，还有 consumer_shutdown_test.go:238 与 run_db_op_test.go（后者固定 runDBOp 的顺序）。§11 没列它们，也没列 TestRedisStepNamesPinned、test_stress_summarize.ps1（见 F7）。
go/db 的 ④（重试只交给分区属主）落地后，重试键会变为 kafka:retry:queue:<topic>:p*。§8.3「ready 与各实例 processing 同时为 0」与 db_retry_queue_length 的口径需要覆盖 p* 键，否则门禁会漏看分区化的队列。§5.2 的 F2 论证（任意实例认领）在 ④ 之后只对旧二进制成立。WAITAOF 落地后，推进脚本是否需要在 ack 前确认 AOF，也未核实。
    **suggested_fix**: §11 第 2 步补上上述测试文件。§8.3 加一句：④ 落地后按 retry_routing.go 的键名覆盖 p* 队列。go/db 会话发来文件清单后，再核对一次 WAITAOF 与推进脚本、Kafka ack 的先后关系。
**fix_plan**: 本次只读，不改任何文件。下面是建议的修改，供用户裁决后由落码会话执行。

一、先裁决在途实现（F1 / F2 / F3 / F9）
1. go/db/internal/kafka/player_row_fence.go（未跟踪），以及 key_ordered_consumer.go 中 :23-27 import、:952-954、:961-970、:2431-2434 这几段在途改动：
   - 来源不明，违反交接 §3.4「不动 go/db/internal/kafka」的约定；
   - 请用户确认作者，并决定撤回，或改造成 v0.3 的过渡实现。
2. C++ 在途改动：
   - player_ownership_comp.h:104-105；
   - player_lifecycle.cpp:755-765、4128-4133、4313、4367-4381、4429。
   这组改动与资产通道的「SQL 提交证据」（go/shared/assetop/sql_commit.go、data_service/asset_sql_commit.go、04-asset-channel.md 在途新增节）是同一会话的产物，应与该会话对齐，二选一：
   - 改为按 §7.2 实现 SaveSeqAllocator；
   - 或由用户裁决改用每玩家计数器，同时回改设计。

二、修改 docs/design/db-write-fencing-mysql.md（v0.4）
1. §5.5（F4）
   - Lua 改为 luaDecimalGreaterFn 前缀。顺序：先核令牌 → 校验 ARGV[3] 与已存 epoch → 写游标 → 写 epoch（键缺失或 dec_gt 时）。
   - 删除「double 精确」的说法；§9.1 的 epoch-raise 同样复用。
   - 写明脚本用 context.WithoutCancel 加有界超时。
2. §4.6、§4.1（F5）
   - 现状改为「HEAD 已经用租约 sqlCtx，只在失租时取消」。
   - 删除「Stop 时取消」与「改为 WithContext」两个工作项。
   - SaveConditional（含 class-2 两步）沿用 sqlCtx。
3. §4.5、§5.4、§6.1、§6.2（F6）
   - 顺序改写为：beginSQL → SQL → finish → reportStoreResult → confirm → 缓存令牌脚本（取代 verify：GET==token → EXPIRE → SET EX）→ 推进脚本。
   - 缓存令牌脚本返回 0 时调 lease.setLost(owner_changed)；ctx 取消不算失租。
   - 新增「为什么取代 verify」一段，往返次数按实际重写。
4. §5.2、§10、§12（F7）
   - db_ordering_guard_total 改为「扩展现有指标」：orderingGuardOutcomes 加 stale_no_epoch 与 epoch_override，写清计数时机。
   - 定新的 RedisSteps 名字。
   - §12 B 批加入 internal/metrics/pipeline.go、metrics_test.go、tools/scripts/stress_summarize.ps1（2e 段）、tools/scripts/tests/test_stress_summarize.ps1。
5. §3.4、§8.1、§11 第 1 / 4 步（F8）
   - 放行口径改为 G-03 最新清单（player_database 六列、player_database_1 两列）；plan 为空也算合法。
   - 取号改为「从 21 起」。
   - 生成物核对只看 DBTask 9 与 PlayerAllData 7。
6. §4.4、§12、§14（F9）
   - 登记资产 SQL 提交证据这一消费方，以及它在 older_seq / equal 下的语义。
   - 新增待拍板项。
7. §4.2（F10）：legacy 也置 (0,0)；body_mismatch 两种模式都计数。
8. §11、§8.3（F11）
   - 补测试文件：consumer_shutdown_test.go、run_db_op_test.go、TestRedisStepNamesPinned。
   - 补 Stop 第④步交错用例，以及「epoch 键非规范时脚本不改游标」用例。
   - ④ 落地后门禁覆盖 p* 队列。
9. §3.1（顺带）：save_seq 已在在途改动中占 9 号，但注释没按 §3.1 写明「10 号预留给 reply_instance」，8 号字段注释也没订正；落码时补上。
   - **2026-10-18 更正**：10 号已由 `reply_instance` 使用（B0-3 第 1 批，`proto/db/db_task.proto` 已有 `string reply_instance = 10;`）。落码时 save_seq 注释直接写「10 号为 reply_instance」，不要再补「10 号预留」；8 号字段注释照旧订正。下方 verified_ok 第 [1] 条「10 号未被占用」是 10-16 的快照，10-18 起不再成立。

三、给 Codex 的验证（落码后）
- 工作目录 go/db：go test ./internal/kafka/... ./internal/metrics/... -count=1，重点看新增的脚本部分写、Stop 交错用例与 RedisSteps 固定用例。
- 工作目录 tools/scripts/tests：跑 test_stress_summarize.ps1。
- 本文未编译、未运行任何测试。
## verified_ok
  - [0]
    字段编号：player_database 已在 HEAD 声明 fence_owner_epoch=18、fence_save_seq=19，inbox_ledger=20 在其后（proto/common/database/mysql_database_table.proto:156-168）；player_database_1 为 3/4（:186-187）。与设计 §3.2 及收件箱占 20 一致。
  - [1]
    DBTask save_seq=9（proto/db/db_task.proto:21，未提交在途）与 PlayerAllData last_save_seq=7（player_cache.proto:37，未提交在途）的号位与设计 §3.1 / §3.3 一致；10 号未被占用。对应生成物（go/proto 与 cpp/generated）已在工作区重新生成。
  - [2]
    G-03 放行清单已同步：90-consistency.md:302-316、03-names.md §3.15a（:600-617、:715-717）、92-handoff.md:308-317 都登记了 fence×2+2 与 inbox_ledger，取号已改为从 21 起。G-03 也写明 plan 按字母序执行（fence_* 排在 inbox_ledger 之前，与 18/19/20 同序）。
  - [3]
    markAppliedSeq 在前、markAppliedOwnerEpoch 在后的调用顺序没有变（HEAD key_ordered_consumer.go:1906-1910；工作区 :1920-1924）。后者仍是「失败只记日志」（:811-822），§5.5 的现状描述成立。
  - [4]
    verify 已在 runDBOp 内，位置在 route.confirm 之后、publishDBOpResult 之前（HEAD key_ordered_consumer.go:2380-2437，verify 在 :2423）；ordering_lease.go 的 verify 是带令牌的 EXTEND。
  - [5]
    Stop 已是五步：撤拉取 → 等消费循环排空 → consumer.Close → cancel worker 并 wg.Wait → 归还重试收据与 closeHandoff（HEAD key_ordered_consumer.go:2639-2685）。第④步仍是先 cancel 再等待，§5.5 描述的 SET 与 EVAL 之间的窗口对重试源任务仍然存在。
  - [6]
    luaDecimalGreaterFn 已入库，并带「条件写复用、不再写 tonumber」的约定（KOC:619-646）；luaMarkAppliedEpoch 已用它（:651-678）。
  - [7]
    proto2mysql 在 D:\luyuan\proto2mysql，feat/conditional-save 分支 = e81a1e7，已读源码核对 conditional_save.go：①严格模式下同版本重放返回 SaveRejected，且 Stored 等于消息围栏；②AllowEqual 时条件为 ≤；③PreserveFields 只在 UPDATE 时跳过，INSERT 照写；④严格模式下围栏全 0 报 ErrInvalidSaveOptions；⑤栅栏列或保留列不在表中报 ErrInvalidSaveOptions；⑥SaveRejected 必带 Stored（全部栅栏列）；⑦驱动错误用 %w 保留；⑧状态查询带 FOR UPDATE；⑨Race、MatchMismatch 与 NULL 栅栏（ErrSchemaDrift）的判定，与设计 §4.3 / §4.4 的假设一致；⑩不修改传入的 message；⑪proto2mysql 的真库用例开了 InterpolateParams（proto2mysql_test.go:56），与 §11 第 3 步的说法一致。
  - [8]
    DBTask write 的生产者确实只有 C++ SavePlayerToRedisImpl（player_lifecycle.cpp:4422）与 go/db/cmd/data_stress（main.go:217）。
  - [9]
    排序锁、游标、已落库 epoch、子表缓存都用同一个 Redis 客户端（buildWorker 中 redisClient / appliedEpochs，NewRedisLocker(redisClient) 在 KOC:1074），§5.5 与 §6.2 的单脚本方案可行。
  - [10]
    go/db 注册了 9 张表（go/db/data/mysql_database_table_list.json），只有 player_database / player_database_1 带围栏字段，§4.1 的围栏表判定前提成立。
  - [11]
    tlsIdGeneratorManager 是独立的 thread_local（core/system/id_generator.h、id_generator.cpp:3），不受 node.cpp:1251 的 tlsEcs.Clear() 影响，§7.2 的放置方案可行。
  - [12]
    Replace 现有 3 个调用点（player_lifecycle.cpp:2242、2276、2330），都经过 PlayerLastPersistedSnapshotComp::Replace，§7.3「在 Replace 里清字段」能全部覆盖。
  - [13]
    go-zero options 写法有先例（config.go:122 的 AllowlistEnforcement），三个入口都用 conf.MustLoad（db.go:57、cmd/migrate:68、cmd/verifier:119）。
  - [14]
    player:{id}:owner_epoch 不会被任何代码删除（merge_zone 的 scene_hot_state.go:46 明确不删）；账号目录在 Redis（loginlogic.go 的 GetOrInitUserAccount 使用 account_data 键）。§9.1 关于 FLUSHALL 基本不触发的判断成立。
  - [15]
    errOrderingLeaseLost 已存在，且不在 retryBudgetExempt 里（store_failure.go:29-32、90）；quarantineOrderingConflict 已经带 label（KOC 的 deadLetterRetry / quarantineOrderingConflict）。现有死信载荷就是重试载荷格式，没有原因信封，所以 §8.3 第 3 条确实是新增工作。
  - [16]
    data_service 只读 PlayerAllData 父键（persisted_player_database.go），不写 MySQL 玩家表。
**notes**: 一、在途实现是这次最紧迫的事。
工作区有一组未提交改动，与资产通道「SQL 提交与实例托管（实施中）」同批，已经抢先实现了一版行围栏：
- go/db：go/db/internal/kafka/player_row_fence.go（未跟踪），以及 key_ordered_consumer.go 中的 hunk；
- C++：PlayerOwnerEpochComp.lastSaveSeq；
- 协议：DBTask 9、PlayerAllData 7，生成物已刷新。
它绕过了设计的两个落码前置（proto2mysql SaveConditional 的验证与合入，以及 go/db 文件清单），也违反了交接 §3.4 的协作约定。F1–F3、F9 的失败场景都是读代码推出来的，未运行。建议在任何人继续落 v0.3 之前，先请用户确认这组改动的归属与去留，避免两套围栏语义同时入库。

二、未核实的事项：
- TiDB 在 autocommit 下 SELECT ... FOR UPDATE 的加锁语义，影响 SaveConditional 状态读的 current read 假设；
- go/db 的 ④ 与 WAITAOF 落地后是否改变推进脚本或 Kafka ack 的先后；
- ④ 的 p* 队列键名与 db_retry_queue_length 的 label；
- 在途代码的作者；
- 重试源任务在 Stop 第⑤步被 releaseRetryOwnership 归还时，RetryCount 会不会回退（影响 F5 里「耗预算」的程度）。

三、其他：
- 没有重复 §17 已处置的意见。F4 的「先写游标后校验」与 F6、F7 都是 A3 入库之后才出现的新冲突，不在原 24 条里。
- 本次只读：没有新建或修改文件，没有把 git show 重定向成文件，没有运行任何编译或测试。
