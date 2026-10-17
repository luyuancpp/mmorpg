<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**summary**: 这批来源不明的在途改动，是"全域到账续批（邮件 / 通用排行榜 / 交易拍卖 / 商城到账 / 跨区匹配）"会话为资产实例过户与托管做的"SQL 提交后才算到账"前置。它在 go/db 里用 proto2mysql 事务 + FOR UPDATE + Save 自建了一版行内围栏，在 C++ 里用每实体计数器发 save_seq，并新增了 PlayerAssetSQLCommit 证据键、data_service 的 require_sql_commit 读路径、assetop 终结与放弃改读 SQL 证据。它和 v0.3 不是同一条路，而且恰好把 v0.3 §15 明确否决的三种做法（被拒写直接 ack、每玩家序号组件、比对用的独立键）落了码，也没有 legacy/dual 分期和紧急开关。

主要问题有四条：
1. 被 SQL 拒绝的写一律当成功 ACK，同时推进游标和 applied epoch。这样一来，旧版 C++ 的写（class-2）、epoch=0 的写（class-3）、计数器倒退后的写都会被静默丢弃，C++ 滚动升级或回滚时 MySQL 会无声落后。
2. 同版本重放用 proto.Equal 比对库里读回的行。proto2mysql 把空子消息写成空串、读回为未置位，而 C++ 总是置位全部子消息，所以失租、缓存或证据发布失败之后的正常重放几乎必然判成"同版本载荷冲突"，最终进死信。
3. C++ 每实体计数器没有墙钟下限。同 epoch 内实体重建或进程重启时，序号会回退。
4. 新 SQL 路径没打 storeerr 标记，L-04 的存储不可用延后对两张玩家表失效。

另外，资产自动放弃改读可能落后于 Redis 的 SQL 证据，会产生"一物两得"路径；改动还违反了与 go/db 会话"④ 与保留期落完前不碰 go/db/internal/kafka"的约定。

结论：这批改动不宜合入。应按 v0.3 实现条件写，把 SQL 证据作为 v0.3 dual + class-1 上线之后的一个消费者重新接入。全部结论来自读代码，未编译、未运行。
## findings
  - [0]
    **id**: F1
    **severity**: high
    **title**: 被 SQL 拒绝的写一律当成功 ACK，并推进游标与 applied epoch；class-2、class-3 和计数器倒退后的写被静默丢弃
    **location**: go/db/internal/kafka/key_ordered_consumer.go:2435-2438（在途：runDBOp 遇到 errPlayerRowStale 返回 nil）→ :1924-1928（HEAD：handleTask 成功分支 markAppliedSeq / markAppliedOwnerEpoch 后 ack）；go/db/internal/kafka/player_row_fence.go:33-42、:55-57（未跟踪）；对照 HEAD 同文件 :786-791 guardOwnerEpoch 注释『推进反而把「已落库」写假』
    **detail**: (a) C++ 滚动升级或回滚。玩家在新版 scene（epoch 5）落过 (5,s) 后，被交接到尚未升级的旧版 scene（epoch 6，DBTask.save_seq=0）。player_row_fence.go:40-42 把它降成未版本化的 (0,0)，(0,0)<(5,s) 判 errPlayerRowStale；runDBOp 返回 nil，handleTask 写游标、把 applied_epoch 推到 6，然后 ACK。该玩家在旧版节点期间的 MySQL 写全部静默丢失，Redis 父键照常前进，只留一行 logx.Infof。新 C++ 整体回滚到旧版时，所有落过带版本行的玩家都会这样。v0.3 §4.3 的 class-2 两步写和 §8.4 并存矩阵正是为了让这一步安全。epoch=0（class-3、data_stress）同理，v0.3 的做法是进死信 legacy_after_seq。
(b) 计数器倒退（v0.3 §9.1）。Redis 从备份恢复或 AOF 丢尾、恢复时玩家离线，计数器与 applied_epoch 一起回退；新铸的 epoch ≤ 行内 fence_owner_epoch，写全部通过 Redis 守卫、在 SQL 判 stale，被静默 ACK，直到计数器追上。v0.3 §5.1 / §4.4 的做法是进死信 owner_epoch_regressed 并发 critical 告警，§15 明确不采用『stale_owner 直接 ack 丢弃』。
(c) 推进游标违反 v0.3 F-2，也违反 HEAD :790-791 的契约。时序：新主 B 的 (E2,12)@o6 因 1205 进 ready；applied_epoch 因 markAppliedOwnerEpoch 失败只记日志（HEAD :811-823）仍为 E1；被废黜的 A 迟到写 (E1,13)@o7 通过 Redis 守卫，在 SQL 判 stale，返回 nil，markAppliedSeq 把游标写成 o7；o6 的重试回来判 orderingStale 被丢，新主这一版永久缺失，之后快照相等跳过存盘时 MySQL 一直停在旧值。另外，stale 分支在 route.confirm（:2441）和 lease.verify（:2445）之前返回：不做 v0.3 §4.4 要求的 older_seq 复核，失租的僵尸实例也能不核令牌就 SET 游标。
    **suggested_fix**: 按 v0.3 §4.4 / §4.5 处理被拒结局，以 (outcome, nil) 交回 handleTask：
- older_seq：先 confirm，复核通过才 ack，不推进游标与 epoch；
- stale_owner、owner_epoch_regressed、legacy_after_seq：经 quarantineOrderingConflict 带 label 进死信；
- class-2：两步写；
- 加 Fencing.Mode=legacy 作为紧急开关，并补 db_fence_write_total 指标与 §10 告警。
  - [1]
    **id**: F2
    **severity**: high
    **title**: 同版本重放用 DB 读回行做 proto.Equal，正常重放几乎必然判『同版本载荷冲突』，恢复路径失效
    **location**: go/db/internal/kafka/player_row_fence.go:58-63（未跟踪）；proto2mysql v0.2.0 pbconv/convert.go:88-95（置位的空子消息写成空串）、:232-234 + :397-399（读回空串时 Clear 成未置位）；cpp/libs/services/scene/player/system/player_database_loader.cpp:215-243（HEAD：每个子消息都 mutable_xxx()->CopyFrom，无条件置位）
    **detail**: protobuf-go 的 Equal 按『已置位字段集合』比较，置位的空子消息与未置位不相等。从没有过资产操作、结算或收件箱的玩家，asset_op_ledger / settlement_ledger / inbox_ledger 都是『置位但为空』。
时序：(E,s) 首次写 SQL 已提交，随后 PublishSQLCommit 或 writeBackDBCache 因 Redis 抖动失败，或 lease.verify 判失租、route.confirm 发现落点变化。这时游标还没推进，deferFailedTask（多数原因耗预算）后同一 (E,s) 重试走到 SQL；FOR UPDATE 读回时这些列是空串，被 Clear；target 里是置位的空消息，Equal 返回 false，报『玩家 SQL 同版本载荷冲突』。这个错误没打 storeerr 标记，deferReason=other，每轮都失败，最终以 budget_exhausted 进死信。这一版的子表缓存和资产证据永远发不出去：若它是下线前的最后一笔，login 冷路径会拿旧缓存拼数据，资产待办一直 AwaitDurable 到玩家下次上线。顶层未知字段（C++ 的 proto 比 go/db 二进制新）同样必然不等。v0.3 §4.4 的 equal 只比围栏、不比内容，恰好是失租与发布失败的恢复路径（§5.4、§6.2）。
    **suggested_fix**: 去掉内容比较，按 v0.3 §4.4 的 equal 处理：复核后补发缓存与证据，再走 §5.5 推进。撞号检测属于 §14 第 12 条，请负责人另选方案（例如令牌脚本删缓存键、由读路径重建），不要用 DB 回读加 proto.Equal。
  - [2]
    **id**: F3
    **severity**: high
    **title**: C++ save_seq 是每实体计数器、没有墙钟下限，同 epoch 内实体重建或进程重启后序号回退
    **location**: cpp/libs/services/scene/player/comp/player_ownership_comp.h:103-106（在途）；cpp/libs/services/scene/player/system/player_lifecycle.cpp:4131-4133（播种）、:4370-4381（++lastSaveSeq）、:4429（DBTask.save_seq）（均在途）；批内合并 go/db/internal/kafka/key_ordered_consumer.go processTaskBatch（HEAD，按 offset 保留）
    **detail**: v0.3 §0.1 第 1 行、§7.1 和 §15 第 2 行已经否决『每玩家组件』，理由是父键不是已发出 DBTask 的上界：DBTask 在 Save() 入队后立即发出，不等 Redis 应答（PL:4382-4440）。
时序：进程 P1 中玩家 epoch=E、lastSaveSeq=10；存盘 11、12 的 DBTask 已进 Kafka，Redis 写还没落地时 P1 崩溃。接下来三种情况之一：同一 node_id 的新进程 P2 接回、scene_manager 走不铸造分支（v0.3 §7.4 条件 2，可达性未核实）；dev 旁路 AllowUnsafeCrossNodeHandoff=true（本地 yaml 开着）；或 Redis 从备份恢复后仍是同一 epoch。此时播种得 10，P2 发出的 (E,11)、(E,12) 内容与 P1 不同，go/db 判同版本冲突进死信；若 P1 的 (E,13) 已落库，P2 的 (E,11..13) 都判 stale，被 F1 静默 ACK。结果 MySQL 和资产证据停在 P1 崩溃前的旧内容，与 Redis 父键分叉。批内合并按 offset 保留，同一批里可能留下 seq 更小的那条，两条都没落。另外没有 Observe 上界：导入的超大 fence_save_seq 会让该玩家很快饱和，SavePlayerToRedisImpl 返回 kRejected，退出与交接都被拒。v0.3 用进程级发号 max(last+1, nowMs<<10) 并给 Observe 设上界，新进程的序号必然越过旧进程。
    **suggested_fix**: 改用 v0.3 §7.2 的 SaveSeqAllocator（进程级，放在 tlsIdGeneratorManager，带墙钟下限，Observe 有上界并配超界处置）；播种来源保留在途的三路 max（与 v0.3 的 SaveSeqFloorFrom 相同），从 PlayerOwnerEpochComp 里删掉 lastSaveSeq。
  - [3]
    **id**: F4
    **severity**: high
    **title**: 新 SQL 路径的错误没打 storeerr 标记，L-04 的『存储不可用延后不耗预算』对两张玩家表失效
    **location**: go/db/internal/kafka/player_row_fence.go:45-71（未跟踪，原样返回 RunInTransaction 的错误）；key_ordered_consumer.go:952-954（在途，绕过 HEAD :955-957 的 storeerr.Tag）；store_failure.go:46-61、:79-105，placement_route.go:386-396（HEAD）
    **detail**: MySQL 宕机、连接数满、1205 / 1213 / 只读时，HEAD 的 Save 路径会打标记：写任务延后、不耗预算，连接类错误还会触发探活和冷却。新路径返回的是裸错误，KindOf=KindNone，reportStoreResult 不报 conn_lost、不进冷却；retryBudgetExempt=false，每笔玩家写都按普通失败耗预算，几轮之后成批进死信（budget_exhausted），而死信目前没有回灌工具（v0.3 §8.3 第 2 条）。链上带 net.Error 的拨号错误会被 deferReason 误归成 redis_unavailable。L-04 的核心目标在最重要的两张表上被绕过。
    **suggested_fix**: 事务返回的错误除 errPlayerRowStale 等哨兵外一律套 storeerr.Tag。更推荐直接换成 v0.3 §4.1 的 SaveConditional 路径，它已经按 L-04 词表定好了去向。
  - [4]
    **id**: F5
    **severity**: medium
    **title**: 行不存在时 FOR UPDATE 的锁语义：RR 下两个并发首写死锁，RC 下围栏被绕过
    **location**: go/db/internal/kafka/player_row_fence.go:45-65（未跟踪）；proto2mysql v0.2.0 proto2mysql.go:3870-3893（FindOneByPKForUpdate）、:3742-3800（saveByPrimaryKey：UPDATE → INSERT → 1062 → UPDATE）；go/db/internal/logic/pkg/proto_sql/db.go:142-155（HEAD，未设隔离级别）
    **detail**: 并发首写的来源：失租的僵尸实例和新持锁实例，同时第一次写同一玩家的行，例如新角色首存、或者新落点库里还没有该行。
- REPEATABLE READ：两边都拿到同一个间隙锁（间隙锁互相兼容），随后各自 INSERT 的插入意向锁互等，形成 1213 死锁，一方回滚。错误没打标记，按 other 耗预算；L-04 本来把 1213 归为 store_busy、不耗预算。正确性靠重试时读到行再判。
- READ COMMITTED：无匹配行时 FOR UPDATE 不加锁，两边都判『无行』。T1 INSERT 并提交；T2 的 Save 先 UPDATE，等 T1 提交后命中该行，无条件覆盖。若 T2 是旧版本（僵尸），首写围栏被绕过，行内围栏回退。
- TiDB：悲观事务对不存在的键做点查会加锁；乐观事务在提交时报 9007。基本安全。
生产 MySQL 的隔离级别未核实。
    **suggested_fix**: 采用 SaveConditional：一条条件 UPDATE，影响 0 行时由 P2M 判定 insert 或 race。若暂留事务方案，ErrNoRowsFound 分支改用 Insert，撞 1062 时重读再判围栏，并在连接参数里显式固定隔离级别。
  - [5]
    **id**: F6
    **severity**: medium
    **title**: 资产终结与自动放弃改读 SQL 证据，证据可能落后于 Redis：180 天后误放弃造成一物两得；Ledger 从可选变必需
    **location**: go/shared/assetop/ledger_dataservice.go:52-62（在途）；go/shared/assetop/reconcile.go:821-853 verifySQLCommit、:855- resultWithReceipt（在途）；go/shared/assetop/abandon.go:335-350（HEAD 调用点，语义被在途改动改变）；go/guild/internal/svc/asset_op.go:317-334、go/trade/internal/svc/assetchannel.go:273（HEAD，注释仍写『离线读账本是优化』）
    **detail**: (a) 时序：玩家在旧版 scene 上、或计数器倒退期间完成一笔帮会兑换 seq=N，scene 回 APPLIED 并写进 Redis 父键；对应的 DBTask 被 F1 静默 ACK，或被 F2 送进死信，SQL 证据停在 N 之前的版本，之后玩家不再上线。180 天后 judgeAbandon 判定：location 不在、last_send 超过安全间隔，ReadPersistedLedger 读到的是 SQL 证据，ClassifySeq=SeqUnseen，于是放弃并退还帮会侧限购与帮贡。玩家下次上线从 Redis 父键加载，资产仍在，一笔两得。HEAD 读的是 Redis blob，会判 SeqApplied，不会放弃。
(b) Loop.Ledger 为 nil（没配 DataServiceRpc）时，verifySQLCommit 直接返回 Durable=false，所有在线终结永远停在 AwaitDurable。
(c) 旧版 data_service 不认识 require_sql_commit，回 sql_committed=false，ReadPersistedLedger 报错，同样无法终结。新版 data_service 配旧版 go/db 时证据键不存在，回 found=false。上线顺序必须是 data_service、go/db、C++ 都先于 guild / trade，任何文档都没写。已离线玩家在新链路上线前没有证据，待办要等他再上线才能终结。
    **suggested_fix**: 自动放弃必须同时证明 Redis blob 与 SQL 证据都『未见』，或者要求证据版本 ≥ 父键的 last_save_seq。Ledger 改为启动断言，并更新两处注释。按 zone 加开关：只有该 zone 已是 dual 且 C++ 是新版时，才要求 SQL 证据。上线与回退顺序写进 04-asset-channel 与 runbook。
  - [6]
    **id**: F7
    **severity**: medium
    **title**: 证据键 player:{id}:asset_sql_commit 的生命周期、归属与比较方式
    **location**: go/shared/assetop/sql_commit.go:13-26、:38-46（未跟踪）；go/data_service/internal/logic/asset_sql_commit.go:22-24（未跟踪）；tools/merge_zone/player_rows.go:695-710（HEAD，不含该键）；key_ordered_consumer.go:629-650 luaDecimalGreaterFn 注释（HEAD）
    **detail**: (1) 没有 TTL：每个玩家常驻一份完整账本（含托管收据里的完整 ItemComp 字节），放在 noeviction 的共享 Redis 上。该键族没有登记进 shared-redis-split-by-purpose.md §2.2，也不在 merge_zone 的失效清单和回档 / PITR 的 runbook 里。
(2) MySQL 回档（v0.3 §9.2）之后，证据仍指向已被回滚的提交。若父键也一起回档，新写的版本会低于证据版本，脚本 :20 返回 0，而 PublishSQLCommit 只把 n<0 当错误（:43-45），旧证据被静默保留，verifySQLCommit 会拿错误的账本去终结跨服务的对侧账。
(3) 写在 go/db 的 redisClient，读走 data_service 的 Router.ClientForPlayer（home_zone 集群或 DevRedis）。本地与 K8s 现在指向同一个共享实例（k8s_deploy.ps1:1750-1754 注释），启用 Regions 集群或 G4a 拆分后两边可能分家（未核实）；合服改了 home_zone 后，读侧会换到另一个集群，证据缺失。
(4) Lua 里用字符串 `>` 比较定宽十进制，依赖 Redis 的 strcoll / locale；仓库约定（luaDecimalGreaterFn）要求按字节比较。另外每次都用 Eval，而不是 Script.Run。
    **suggested_fix**: 按 v0.3 §6.2 把证据发布并进写路径的令牌脚本（同一次 EVAL 里核锁令牌、SET 子表缓存、按字节比较版本后 HSET 证据）；n==0 时计数告警；登记该键族，并把它加进合服、回档与 PITR 的清理步骤；明确 TTL 或回收策略。
  - [7]
    **id**: F8
    **severity**: medium
    **title**: 违反与 go/db 会话『④ 与保留期落完前不碰 go/db/internal/kafka』的约定，并与 S1 / S4 的前提冲突
    **location**: go/db/internal/kafka/key_ordered_consumer.go:22-30（import）、:952-954、:962-972、:2435-2438（在途）；go/db/internal/kafka/player_row_fence.go（未跟踪）；docs/handoff/handoff-landing-recovery-and-fencing-20261016.md §3.4；docs/handoff/godb-pipeline-impl-handoff-20261015.md §2-§4；docs/handoff/godb-pipeline-impl-20261015/S1-retry-owner-waitaof-spec.md:271
    **detail**: R2（④ + WAITAOF）和 R3（保留期）都没落，约定继续有效。
- 文本冲突面小：S1 改 deferFailedTask / quarantineOrderingConflict / handleTask 入口 / processTaskBatch 的 superseded 分支，在途改 handleDBWriteOp / publishDBWriteResult / runDBOp 与 import。
- 语义冲突：S1 规格预期『条件写会话在 S1 上 rebase，stale_owner 走 quarantineOrderingConflict，Kafka 源分支会自动走 WAITAOF』，在途反过来直接 ACK，失去持久化死信；S1 / S4 都以『markApplied 之前一定已落库』为前提，在途多出一条『没写库也 markApplied』的成功路径。
- verify 已在 HEAD 挪进 runDBOp（:2445），在途的 stale 分支在它之前返回。
- Stop 五步没有直接冲突：事务的 ctx 来自租约，停机不取消。
- luaDecimalGreaterFn 没有被复用（见 F7）。
    **suggested_fix**: 先找 go/db 接手会话要文件清单，确认 R2 / R3 的落地顺序，再按 S1 的锚点 rebase；被拒结局走 quarantineOrderingConflict，以继承 WAITAOF。
  - [8]
    **id**: F9
    **severity**: medium
    **title**: 性能与压测口径：每次写都读整行加事务、多一次带账本的 EVAL；C++ 快路径每次深拷贝两份；STRESS_TEST_PROBE 下快路径行为改变
    **location**: go/db/internal/kafka/player_row_fence.go:45-65；key_ordered_consumer.go:962-972；cpp/libs/services/scene/player/system/player_lifecycle.cpp:814-828、:4313（均在途）
    **detail**: - go/db：每次玩家写多一条 SELECT ... FOR UPDATE，读回整行全部 MEDIUMBLOB 列，再加上一整段事务（BEGIN / SELECT / UPDATE，必要时 INSERT、1062、UPDATE、match，再 COMMIT），行锁持有跨多次往返；每次 player_database 写还多一次带整份账本的 EVAL。v0.3 是一条条件 UPDATE，只有影响 0 行时才多一次状态查询。
- C++：快路径原来零拷贝，现在每次存盘尝试都深拷贝两份 PlayerAllData。
- 口径：IsPersistedPayloadCurrent 同时剥离了 stress_test_probe，STRESS_TEST_PROBE=1 时快路径从『永不命中』变成『可命中』（v0.3 §7.3 末段、§14 第 11 条待核项），压测跳过率与 R20 基线不可比。
以上都没有压测数据（AGENTS §6、§11.3）。
    **suggested_fix**: 改用单条条件 UPDATE；快照在 PlayerLastPersistedSnapshotComp::Replace 时剥离一次（v0.3 §7.3），比较时不再深拷贝；stress_test_probe 剥离这一项请负责人单独拍板，压测前后按 AGENTS §6 出对比表。
  - [9]
    **id**: F10
    **severity**: low
    **title**: C++ 把围栏写进 body 与父键子消息，与 v0.3 §3.2 / §4.2『C++ 只读不写、go/db 盖章』相反
    **location**: cpp/libs/services/scene/player/system/player_lifecycle.cpp:758-764（MarshalPlayerForSave）、:4377-4381（均在途）
    **detail**: HEAD 的 go/db 二进制已经认识 18 / 19 号字段（字段在 HEAD 的 proto 里）。新旧 go/db 混跑时，旧二进制的 Save 会把 body 里的围栏无条件写进行。epoch==0 分支的 body 是 (0, lastSaveSeq>0)（:758-764 取的是组件里的 epoch=0），旧 go/db 会写出 (0, s>0) 这样的行；之后新 go/db 对该行的未版本化写 (0,0) 判 stale，静默丢弃。旧版僵尸实例还能把围栏写回退。body 与信封不一致时也没有 v0.3 的 db_fence_body_mismatch_total 计数。
    **suggested_fix**: C++ 只设置 PlayerAllData.last_save_seq，两张子表的 fence 字段保持 0，由 go/db 按 DBTask 盖章，不一致时计数。
  - [10]
    **id**: F11
    **severity**: low
    **title**: 可观测性、启动断言与测试缺失；表名硬编码
    **location**: go/db/internal/kafka/player_row_fence.go:21-29；key_ordered_consumer.go:2436（在途，只有 logx.Infof）；go/shared/assetop/sql_commit_test.go（未跟踪，miniredis）
    **detail**: - 被拒写只打 INFO，没有 db_fence_write_total，也没有 §10 的告警，不符合 AGENTS §11.3『fail-open 必须配日志、指标、测试』。
- 表名硬编码：新表成为写目标时不会自动进入围栏；围栏字段缺失时逐条报错，而不是 v0.3 §4.1 的启动即拒。
- go/db 与 C++ 都没有新测试（v0.3 §7.5、§11）。
- sql_commit_test 用 miniredis（gopher-lua 按字节比较），测不到真实 Redis 的 strcoll。
    **suggested_fix**: 按 v0.3 §4.1 在启动时按描述符判定围栏表并断言；按 §10 补指标与告警；按 §7.5、§11 补用例，另加：空子消息的同版本重放、class-2 / class-3、计数器倒退、证据并发发布与回档。
  - [11]
    **id**: F12
    **severity**: nit
    **title**: 契约注释与组件职责
    **location**: proto/db/db_task.proto:21（在途）；cpp/libs/services/scene/player/comp/player_ownership_comp.h:92-106（在途）
    **detail**: - db_task.proto：save_seq 的注释没写 10 号预留给 reply_instance（v0.3 §3.1 负责人已定），8 号字段的注释也没订正；『同一 owner_epoch 内严格递增』这一承诺，在实现上跨实体重建时并不成立。
- PlayerOwnerEpochComp：文档写『写入口只有两个』，现在存盘路径也会写 lastSaveSeq，归属与存盘序号混在同一个组件里，违反 AGENTS §11.1 的单一职责。
    **suggested_fix**: 随 v0.3 落码一起订正：序号移出该组件，交给进程级发号器。
**fix_plan**: 本方案只给取舍与合并建议，不直接改他人文件。

0. 先停合入并对齐归属
- 这批改动属于『全域到账续批』会话。先与该会话及负责人对齐：它要的『SQL 提交后才算到账』作为 v0.3 的消费者重新接入，不再自带一套围栏。
- 动 go/db/internal/kafka 之前，先找 go/db 接手会话要 R2 / R3 的文件清单，再按 S1 的锚点 rebase。

1. proto（保留号，补注释）
- 保留 DBTask.save_seq=9、PlayerAllData.last_save_seq=7，两者与 v0.3 同号同义。在 db_task.proto 注明 10 号预留给 reply_instance，同时订正 8 号字段的注释。
- PlayerAssetSQLCommit、require_sql_commit=2、sql_committed=3 可以保留号，但读路径要等第 4 步的开关就绪后才启用。

2. go/db：放弃 player_row_fence.go，改按 v0.3 §4–§6 实现
- handleDBWriteOp 改为 (writeOutcome, error) 签名，内部用 SaveConditional。
- 启动时按描述符判定围栏表并断言。
- 加 Fencing.Mode=legacy|dual 与 EpochOverride。
- class-1 用严格模式；class-2 两步写；class-3 只在行内围栏为 (0,0) 时写。
- SQL 错误一律套 storeerr.Tag。
- 结局：
  - equal：不比内容，补发缓存；
  - older_seq：先 confirm 再 ack，不推进游标；
  - stale_owner、owner_epoch_regressed、legacy_after_seq：经 quarantineOrderingConflict 进死信，自动继承 S1 的 WAITAOF。
- §5.1：dual 下做计数器探测。
- §5.5：用一条推进脚本原子推进游标与已落库 epoch。epoch 比较复用 luaDecimalGreaterFn，不用 v0.3 原稿里的 tonumber；这一处要按交接 §3.4 回改 v0.3。
- §6.2：写路径用令牌脚本，读路径用 SET NX。
- SQL 证据发布并进 §6.2 写路径令牌脚本，同一次 EVAL 完成：KEYS 为锁、子表缓存、证据；核令牌后 SET 缓存；按字节比较十进制版本，更大才 HSET 证据。只在 dual、class-1、结局为 applied 或 equal 时发布。返回 0 时延后（耗预算）；证据版本更高时计数并告警。
- 若 SaveConditional 迟迟未验证、确需过渡，可暂留事务方案，但至少做到：
  - 错误打标记；
  - 被拒结局交回 handleTask，不 markApplied、不 ACK，进死信；
  - 删掉 proto.Equal；
  - 无行分支用 Insert，撞 1062 时重读再判。

3. C++
- 删掉 PlayerOwnerEpochComp.lastSaveSeq，改用 v0.3 §7.2 的 SaveSeqAllocator，作为 tlsIdGeneratorManager 的成员。Next=max(last+1, nowMs<<10)；Observe 设上界，超界处置请负责人选（§14 第 13 条）。
- 播种复用在途已有的三路 max。
- MarshalPlayerForSave 与 SavePlayerToRedisImpl 只写 last_save_seq，不写两张子表的 fence 字段。
- 剥离改在 PlayerLastPersistedSnapshotComp::Replace 里做一次；IsPersistedPayloadCurrent 继续剥离 last_save_seq，但不要在快路径上每次深拷贝两份。
- stress_test_probe 是否在快路径剥离，单列给负责人拍板，它影响压测口径。
- SavePlayerForAssetCommit 可以保留：资产需要强制写，注明 DBTask 量会上升。
- 补 v0.3 §7.5 的单测。

4. 资产通道（go/shared/assetop、data_service）
- verifySQLCommit 加开关：只有 zone 已是 dual 且 C++ 已是新版时才要求 SQL 证据。
- Ledger 必填改为启动断言，并更新 guild / trade 两处『离线读账本是优化』的注释。
- judgeAbandon 改为『Redis blob 与 SQL 证据都未见』才放弃，或要求证据版本 ≥ 父键的 last_save_seq。
- data_service 的 GetSQLCommittedAssetLedger 保留，并写明『证据键与 go/db 的 Redis 同实例』这个前提；G4a 拆分稿登记这条跨服务依赖。
- 上线顺序：data_service → go/db（legacy）→ C++ → 逐 zone 切 dual → 打开 SQL 证据开关 → guild / trade。回退顺序相反。

5. 运维与文档
- 证据键族登记进 shared-redis-split-by-purpose.md §2.2。
- 把证据键加进 merge_zone 的 invalidatePlayerCaches，以及 zone_data_rollback 与 PITR runbook（v0.3 §9.2）。
- 04-asset-channel 末段注明依赖 v0.3 的 dual。
- 按 AGENTS §12，在 Java 仓 PARITY 登记：条件写对 Java 不适用；资产实例过户与 SQL 证据的契约两版同批。

6. 请负责人显式拍板（这批改动隐含替用户做了决定）
- §14 第 12 条：equal 是否比内容；
- §14 第 13 条：Observe 超界的处置；
- §14 第 9 条：是否用『不进死信』绕过 L-04 第 2 批死信工具前置；
- §14 第 8 条：进程换代是否必铸 epoch；
- §14 第 11 条：压测探针剥离；
- 新问题：SQL 证据是否作为资产终结与放弃的硬前提，以及它在 dual 之前的退化行为。
## verified_ok
  - [0]
    表名判断可用：mysql_database_table.proto 没有 package（HEAD），所以 FullName 就是 player_database / player_database_1，C++ 用 full_name() 填 msg_type，两侧一致（player_row_fence.go:21、key_ordered_consumer.go:962）。
  - [1]
    class-1 的字典序判定写法正确（player_row_fence.go:55），epoch 是高位。
  - [2]
    errPlayerRowStale 经 RunInTransaction / Transaction 原样返回，或回滚失败时以 %w 包装（proto2mysql.go:4337-4372），errors.Is 能识别。
  - [3]
    RunInTransaction 继承 WithContext 的 ctx（proto2mysql.go:378-393、:417-435），事务语句受租约 ctx 约束，失租取消时事务回滚；ctx 用法与 HEAD 注释『只随失租取消、停机不取消』一致。
  - [4]
    证据只在 SQL 成功且 confirm、verify 都通过之后发布（key_ordered_consumer.go:2441-2452 → :962-972）；stale 分支不发布证据，也不写缓存。
  - [5]
    证据版本串 %020d:%020d 能容纳 uint64 全范围，按字节序比较时与 (epoch, seq) 字典序一致；同版本不同内容返回 -1，PublishSQLCommit 报错，不伪装成功（sql_commit.go:38-46）。
  - [6]
    data_service 解析证据时校验 player_id 一致、epoch 与 seq 非 0（asset_sql_commit.go:27-30），缺证据回 found=false，不回落到 Redis blob。
  - [7]
    成功分支 markAppliedSeq → markAppliedOwnerEpoch 的顺序未变（key_ordered_consumer.go:1924-1928），HEAD 已把 verify 放在缓存发布之前（:2445）；dbOpHandler 的签名没改。
  - [8]
    C++ 快路径跳过存盘不消耗序号；marshal 失败、交接已请求这类 kRejected 在 ++ 之前返回；同一实体重连、同节点换图、采纳新 epoch（EnterScene 取 max、B5 采纳）时 lastSaveSeq 不回退；epoch==0 时不发号，DBTask.save_seq=0（player_lifecycle.cpp:4366-4429）。
  - [9]
    两张子表共用同一个 save_seq，各自有独立围栏，与 v0.3 §7.2 一致；player_database_1 同样走 savePlayerRow（受同等保护，也同样受 F1–F5 影响）；证据只为 player_database 发布，资产只在这张表里。
  - [10]
    读快照的其他系统（战斗结算 player_battle.cpp:358-411、资产通道 asset_op_system.cpp:210-216、收件箱 player_inbox_system.cpp:413-428）只读账本字段，不受围栏字段与 last_save_seq 影响；所有对快照做相等比较的调用点（PL:2245、:3417、:4313）都改走 IsPersistedPayloadCurrent。
  - [11]
    证据键用 {player_id} 作 hash tag，与负责人已定的『Redis 按 player_id 分片』一致。
  - [12]
    本地与 K8s 现状：go/db 的 RedisClient 与 data_service 的 DevRedis 都指向同一个共享实例（k8s_deploy.ps1:1750-1754 注释），所以证据现在能被 data_service 读到。
**notes**: 【一】与 v0.3 的逐节差异

§3 数据契约
- DBTask.save_seq=9：号一致。v0.3 的语义是『按玩家只增不减』；在途注释写『同 epoch 内严格递增』，但实现跨实体重建时不保证（F3）。没注明 10 号预留，也没订正 8 号字段的注释。
- 表字段 18/19、3/4：HEAD 已有，一致。
- 写入方：v0.3 规定只由 go/db 按 DBTask 写，C++ 只读来播种。在途 C++ 同时写 body 和父键的子消息（F10）。
- PlayerAllData.last_save_seq=7：一致。剥离位置不同：在途只在 IsPersistedPayloadCurrent 两侧剥离，v0.3 是 Replace 加 IsPersistedPayloadCurrent。
- v0.3 没有的新契约：PlayerAssetSQLCommit、require_sql_commit / sql_committed、证据键 player:{id}:asset_sql_commit。

§4 条件写
- 原语：v0.3 用 SaveConditional，一条条件 UPDATE；在途用事务、FOR UPDATE 读整行、再 Save。
- 签名：v0.3 是 (writeOutcome, error)；在途对外仍是 error，内部多一个 handled 布尔。
- 围栏表：v0.3 启动时按描述符判定并断言；在途按名字硬编码。
- class-1：一致（严格字典序）。
- class-2：v0.3 两步写、不退围栏；在途降级为 (0,0)，行上有围栏就静默丢。
- class-3：v0.3 进死信；在途静默丢。
- equal：v0.3 不比内容、补发缓存；在途用 proto.Equal 比内容（F2）。
- older_seq：v0.3 先复核再 ack、不推进；在途不复核直接 ack，还推进游标和 epoch。
- stale_owner / regressed / legacy_after_seq：v0.3 进死信并发 critical 告警；在途静默 ACK。
- 错误：v0.3 套 storeerr.Tag，新增 fence_race；在途不打标记（F4）。
- 模式与开关：v0.3 有 legacy / dual / EpochOverride；在途没有。
- ctx：一致，都用租约 ctx。

§5 Redis 守卫
- §5.1 计数器探测：在途没有。
- §5.2 EpochOverride（G3）：在途没有。而且在途的 stale-ACK 推进游标，在 SQL 拒绝路径上给 G3 类丢失新增了一种触发方式。
- §5.3 批内合并按 save_seq：在途没有，仍按 offset。
- §5.4 失租：未改，仍不豁免，与 v0.3 的缺省一致。
- §5.5 原子推进：在途没有；stale 路径甚至不核令牌就推进。
- §5.6 G5：同 HEAD。

§6 子表缓存
- §6.2 写路径令牌脚本、读路径 NX：在途都没有，仍是普通 SET（HEAD 已把 verify 前移，但不是原子的）。
- §6.3 缓存值带围栏：在途有（msg 被替换成 target）。
- §6.4 / §15：v0.3 否决『独立键』，理由是两键必须同生同灭。在途新增的证据键用途不同，但正是这类键：它与父键、子表缓存、MySQL 各自有独立的生命周期（F7）。

§7 C++
- 发号器：v0.3 是进程级、带墙钟下限、Observe 有上界；在途是每实体 ++，没有墙钟、没有上界（F3）。
- 播种来源：一致（三路 max）。
- 饱和：两边都显式拒绝。
- §7.3：在途只在 IsPersistedPayloadCurrent 剥离，快路径改用它（每次深拷贝两份），还顺带剥离了 stress_test_probe。
- §7.5 单测：在途没有。
- v0.3 没有的新增：SavePlayerForAssetCommit（资产操作强制整份写）。

§8 分期与兼容
- v0.3：Stage 0 → 1a（legacy）→ 1c（新 C++）→ 1b（逐 zone 切 dual，门禁包括死信工具、告警演练、class-3 观察窗、金丝雀）→ EpochOverride。
- 在途：没有分期。新 go/db 一上线就对任何版本的 C++ 生效，C++ 回滚不安全；回退只能换二进制，换回 HEAD 的 go/db 后，旧 Save 会把 body 里的围栏原样写进行。资产通道还新增了一条上线顺序依赖（F6）。

【二】隐含替用户做了的 v0.3 §14 决定
按交接 §3.4 的 13 项列：
1. equal 的缓存处理（§14#12）：选了 v0.3 之外的第三种——比内容，不同即失败；内容相同时补发缓存和证据。
2. Observe 超界（§14#13）：没有上界、没有个人下限、也没有墙钟下限，比两个候选方案都弱。
3. 失租豁免（§14#14）：未动，沿用不豁免。
4. Stage 1b 依赖死信工具（§14#9）：用『根本不进死信、直接 ACK』绕过了这条前置，实质上改写了不变量 F-3。
5. 冷登录与在途写竞态（§14#15）：未处理。
6. epoch-raise 入口（§14#5）：未处理。静默 ACK 让计数器倒退变成无声丢盘，反而更依赖这个入口。
7. proto 生成（§14#6）：工作区已有 db_task / player_cache / asset_op / data_service 的 C++ 与 Go 生成物改动，说明有人跑过生成；是否写了客户端仓未核实。
8. 告警重复（§14#16）：一条新告警都没加。
9. 进程换代是否必铸 epoch（§14#8）：未处理。在途没有墙钟下限，这一条从『多主机切 dual 的前置』变成『单机也会撞号』。
10. G4a 拆分后 go/db 的 Redis 客户端（§14#19）：新增了一条跨服务 Redis 依赖（go/db 写、data_service 读），拆分稿未登记。
11. 铸造的持久下限（§14#7）：未处理。
12. 两个可选项（§14#17、#18）：未做。
13. 两项待核（§14#10、#11）：#11 被顺带改变（剥离 stress_test_probe 后，压测下快路径会生效），没有登记。

另外，它还推翻了 v0.3 已定或已否决的几项：缺省 legacy 加逐 zone 切换（§14#4）、§15 的『每玩家组件』『stale 直接 ack』『独立键』。

【三】与 go/db 会话的冲突
见 F8。要点：
- 『④ 与保留期落完前不碰 go/db/internal/kafka 与 proto_sql』的约定仍有效，在途改了 KOC 并新增同包文件；proto_sql 没动。
- verify 已在 HEAD 挪进 runDBOp，在途 stale 分支在 confirm 与 verify 之前返回。
- Stop 五步没有直接冲突。
- 没有复用 luaDecimalGreaterFn。
- S1（④）规格的 rebase 前提被反转：它预期被拒结局进死信，在途改成了 ACK。
- markApplied 的调用顺序本身没变，但新增了一条『没写库也 markApplied』的路径。按交接 §3.4，v0.3 落码前还要按这三点回改 §5.4、§5.5、§6，其中 §5.5 的 tonumber 要换成 luaDecimalGreaterFn。

【四】来源判断：全域到账续批，用于资产实例过户与托管
最直接的证据是 docs/design/guild-phase2/04-asset-channel.md 在途追加的一段『2026-10-15 全域到账续批：SQL 提交与实例托管（实施中）』，授权范围写的是：邮件、通用排行榜、交易拍卖、商城到账、跨区匹配。这一段逐条描述了本批改动：
- C++ 生成并继承 save_seq；
- go/db 在同一个 SQL 行锁事务里比较 (owner_epoch, save_seq)，同版本不同载荷必须失败；
- SQL 成功后才发布 PlayerAssetSQLCommit；
- 强一致调用显式带 require_sql_commit=true；
- 已有装备以完整 ItemComp、宠物以完整 PetInstance 的字节过户。

消费方：
- go/shared/assetop：ledger_dataservice.go:52（RequireSqlCommit:true）、reconcile.go 的 verifySQLCommit 与 resultWithReceipt（托管原件 snapshot）、receipt_ack.go（未跟踪，AckReceipt）；
- C++：asset_op_system.cpp:134、:1132（g_persistFn=SavePlayerForAssetCommit）、:480-705（item_instances / pet_instance 过户），asset_op_auth.cpp:166-179（v2 签名段），scene_node_service.cpp:257、:500（AssetAckReceipt）；
- 未跟踪的新服务与新包：go/shared/mailgrant、go/mail、go/rank；
- proto/trade/jubaozhai.proto 在途新增官方 SKU、P2P 交易、寄售、订单簿、拍卖等 RPC；
- Java 仓 PARITY.md 在途行『完整邮箱与通用排行榜（2026-10-15进行中）』；
- build/commerce-delivery-java-20261015：Java 商城到账的打包与回归日志。shared-delivery-package.log 时间 22:21；asset-receipt-regression-exit.txt=1，失败在 game-guild 的 getClientRequestId 编译错误，与本批无关。

时间也对得上：player_row_fence.go 与 sql_commit.go 22:21、asset_sql_commit.go 22:22，与那份日志同一时段。

最可能的用途：交易拍卖、寄售与商城到账的『资产实例过户 + 托管』。卖方扣出（带完整实例）必须先在 MySQL 落定，才能给买方或对侧入账，避免 Redis 丢失后一物两得。围栏是为证据的可信度服务的附带实现。

【五】未核实
- 生产 MySQL 的事务隔离级别（F5 的 RC 分支）；
- 同 node_id 新进程走不铸造分支的可达性（F3，沿用 v0.3 §7.4 条件 2）；
- Redis 的 LC_COLLATE 设置；
- 启用 Regions 集群或 G4a 拆分后，证据键的读写两侧是否分家；
- protobuf-go Equal 对空子消息的行为按 APIv2 语义推断，没有运行；
- 客户端仓是否已被 proto 生成改写。

【六】协作现状
分析过程中 sql_commit.go、player_row_fence.go 与 KOC 的在途 hunk 被他人 gofmt 重排过（内容未变），说明来源会话仍在活动。文中行号以重排后为准，引用前请重新定位。本任务没有修改、也没有新建任何文件。
