# S1 规格修订稿（优先于原规格与复核意见）

## 逐条裁决
### [accept] [high] WAITAOF 在子 shard worker 里同步等待，故障路径整 zone 吞吐被压到约「子 shard 数 / fsync 周期」，未量化、无验收标准
采用复核修法(1)：Kafka 源的三类入队点（deferFailedTask 的 Kafka 分支、quarantineOrderingConflict 的 Kafka 分支、毒消息的两处）全部改为「组提交 + 异步确认，ack 推迟到确认之后」。worker 把请求交给入队层就返回，接着处理下一条任务，不等 fsync。理由（数字）：设计峰值是 2000 task/s/zone，80 个子 shard 每条 35ms，利用率约 87%。同步 WAITAOF 下，每条失败任务要占住子 shard 约 0.5s（everysec 平均值，最坏约 1s，是正常任务的 14 到 28 倍）。峰值时只要 1% 的任务失败，就要多占约 12.5% 的容量，超过剩余余量，lag 只涨不落；失败率到 5%（搬库冻结一批玩家、某个非 zone 落点库不可用）就直接击穿。位点语义：claimAcker 只对连续完成的前缀调 MarkMessage（KOC:212-241）。未确认的消息 X 会把本分区的已提交位点停在 X；X 之后的消息照常执行，完成状态先存进 acker.completed。X 确认后，前缀一次推进到位。X 未确认时调 failKafkaTask：acker.failed 置位，claimCtx 取消，本 claim 不再标记任何位点；再平衡后从 X 开始重投。重投时，写任务按游标判 stale，丢弃并 ack；读任务重算（已没有等待方，回写的是 MySQL 当前值）；重试副本可能出现重复，按 seq 去重。位点提交的延迟上界是排队时间加单批预算 5s，小于 claimDrainTimeout 10s，所以正常再平衡和停机排空都等得到确认。Kafka 任务没有 acker（只有单测直接拼结构体时）就同步等待，免得 Sarama 的高水位 MarkMessage 越过未确认的 offset。代价：同一分区的位点提交最多晚约 5s；崩溃或未确认时重投窗口变大，由幂等吸收；新增 32MiB 的待确认字节上限；入队层吞吐上限约为每次 fsync 一批（≤256 条且 ≤8MiB），超出时背压回 worker，效果等同今天的同步写法，表现为 Kafka lag，不丢数据。接口、关闭协议、粘连连接、计数口径见修订稿 §1。不再采用选项(2)，改在 staging 验收：锁住 10% 测试玩家 60s，健康玩家的 w_task/w_queue 与基线相差 ≤20%，且 Kafka lag 不持续增长。

### [accept_modified] [medium] B4 原场景（S1 镜像在 shared 下也有缺陷，只能直接回滚到 S1 之前的镜像）下没有任何 Pod 上报 partitioned，告警哑火，只靠 runbook 兜底
新增告警 DbRetryPartitionedStranded（critical，for 5m）。它只用 Prometheus 里已退出的新版 Pod 留下的最后样本，不需要任何新版二进制在线。条件：namespace 内「最后一次上报时间最晚」的那个 Pod，其 db_retry_queue_length{queue=partitioned} 的最后值 >0，并且该 namespace 当前没有任何 db_retry_routing_mode 序列。PromQL 见修订稿 §6。取最晚上报的 Pod 而不是 max，是为了避免早先退出的 Pod 留下的旧非零值误报。窗口 6h，Prometheus 默认保留 15d，够用。窗口过后告警会自行消失，但这不代表已处理，只能以 retry_requeue -check 的结果为准；搬回之后，窗口内告警仍会响，按 -check 结果静默。另在 docs/ops/release-checklist.md 的回滚段追加一条前置：go/db 执行 rollout undo 或部署上一版本之前，先切 shared，并确认 -check 显示 p* 为 0；已经回滚的，立即执行 retry_requeue。不采用 redis_exporter：本仓库没有部署它，shared-redis-split-by-purpose.md 待决项第 6 条仍未拍板，属于新基础设施。不采用部署脚本前置检查，理由有三：release-checklist 的快速回滚是 kubectl rollout undo，根本不经过脚本；用当前脚本带 -GoSvcTag 部署旧版本时，脚本无法判断该 tag 是否早于 S1（镜像里没有能力标记）；k8s_deploy 目前也不连 Redis。

### [accept] [medium] 停机时 ctx 已取消，入队按 result=error 计数，每次正常滚动都可能误触 critical 告警 DbDurableEnqueueNotConfirmed
db_durable_enqueue_total 的 result 改为 ok、not_durable、error、canceled 四种，预建 2×4=8 个序列。canceled 的判定：submit 时调用方 ctx 已取消，或在背压等待中被取消，或入队层已 close，或 directEnqueue 在重试间隙或 LPUSH 失败时 ctx.Err()!=nil。以上情况都包成哨兵 errDurableEnqueueCanceled（取代原规格的 errDurableEnqueueStopped），result 计 canceled，canceled 不进 db_durable_enqueue_seconds。flush 内部预算耗尽一律是 errEnqueueNotDurable，不得被判成 canceled。告警只看 not_durable|error。三个调用方遇到 canceled 时打 INFO「shutdown: … not enqueued, offset left uncommitted for replay」，不打 DATA-LOSS PREVENTED 的 ERROR；照样 failKafkaTask，位点不提交。新增测试 TestDurableEnqueue_CanceledCtxCountsCanceled（direct 与 group 各一条，断言 result=canceled 加 1、error 不变）和 TestAOFGroupEnqueue_SubmitAfterCloseCountsCanceled；TestRetryRoutingSeriesPrecreated 同步改为 8 个组合。

### [accept] [medium] §7「搬移丢了只回到前一状态」与幂等依据「游标判 stale」漏了前提：游标与收据 ack 在同一个没有 WAITAOF 的 Redis 里，AOF 尾巴丢失时会一起回退
只改文档，不改代码。设计文档 §7 与风险段写明以下几点。WAITAOF 只保证 Kafka 位点提交之前，唯一副本已按配置落盘；它不保证游标 consumer:applied:*、owner epoch、收据 ack（LREM）、重试源搬移的持久性，这些仍是主池上的普通写，everysec 下 Redis 崩溃最多丢约 1 秒。丢失之后游标可能回退，已 ack 的收据可能重新出现在 processing；而 MySQL 与 Kafka 位点都已前进，旧 seq 的收据重放会把 MySQL 覆盖回旧档。这一残余只能由 MySQL 条件写封死。组提交在网络错误时重发、以及 Kafka 重投，都会制造更多同 seq 副本，使这个窗口更容易撞上。幂等依据补一句：前提是游标没有随 Redis 一起回退。全文不得出现「Redis 崩溃安全」一类表述。

### [accept_modified] [medium] owner 模式把重试绑在消费组健康上：没有 claim 就没有租约，重试完全停止；与 S2 交接闸的交互未写
不加 shared 兜底。理由：进程内无法区分「我是热备，分区都在别人手里」（owner 模式下的正常状态）和「全 zone 都没拿到分区」。兜底会在最需要单一属主的交接窗口和僵尸窗口里恢复非属主执行，还会破坏 S2「同一 topic 至多一个组在消费」的保证。明确接受停滞：Kafka 不可用、长时间再平衡、S2 交接闸等待期间，owner 模式下 p* 只进不出，停滞时长等于 Kafka 消费空窗，数据都在 Redis 里，不丢。新增告警 DbZoneNoPartitionClaims（critical，for 5m）：本 zone 所有 S1 版 db 的 db_owned_partitions 之和为 0，即存盘消费与 owner 重试同时停摆；混跑旧版 Pod 时抑制，表达式见修订稿 §6。处置写进设计文档 §6 与 §11：先修 Kafka 或交接闸；交接闸卡住、broker 可达且 MySQL 健康时，可以 kubectl set env deploy/db DB_RETRY_ROUTING=shared 让 shared 实例排空 p*；broker 不可达时不要这么做，新 Pod 在 NewConsumerGroup 处起不来。给 S2 和 S4 的挂点不变量：① lease 用最终定稿的 claimCtx 登记，acquire 放在 S2 的 sessionScope/yieldCtx 与 S4 的 WithoutCancel 加观察 goroutine 都派生完成之后、主循环之前；② 每条排空路径（drainAfterStop、S4 的 drainClaimForShutdown、消息通道关闭分支）的第一步都是 lease.close()，属主资格以 claim 自身生命周期为准，不依赖停机时 claimCtx 是否被取消；③ S2 交接闸等待期间不会有 claim，这是已知、可接受的停滞。切换顺序保持「该 zone 先完成 S2 切组，再切 owner」。owner 模式下做反向交接时允许停滞，不要求先切 shared。

### [accept_modified] [medium] TestEtcYamlRetryKeysMatchDefaults 把 RetryRouting 钉成 shared，与 rollout 第 4 步改 owner 冲突
测试改名为 TestEtcYamlRetryKeysValid，断言改为以下几条：Normalize 之前，RetryRouting 原始值非空，ResolveRetryRouting(值, 空串) 不报错且 source=config；WaitAOFNumLocal 与 WaitAOFNumReplicas 两个指针非 nil、WaitAOFTimeoutMs>0（键必须显式写出，否则 Get-AuthoritativeScalar 镜像失败）；Normalize 之后 Validate() 为 nil，且 RedisClient.WaitsForAOF()==true（仓库自带的配置不得悄悄关掉 WAITAOF）。不再断言取值等于 shared 或默认常量。这样第 4 步改 owner、redis-ha 接入时把 NumReplicas 改成 1，都不需要改这条测试。ConfigMap 与 db.yaml 一致由现有契约测试守住。

### [accept] [low] ownedPartitions 的 lookup/release 未写明 nil 接收者安全，直接拼结构体的现有单测会空指针
acquire、release、live、lookup 四个方法对 nil 接收者都安全：acquire 返回一个未登记的 lease（可正常 enter/exit/close），不计数、不改指标；release 只调 l.close()，不碰 map 和指标（l 为 nil 时直接返回）；live 返回 nil；lookup 返回 nil。claimLease 的 enter/exit/close/live/idle/inflightCount 对 nil 接收者也安全：enter 返回 false，idle 返回已关闭的 channel，其余为空操作或零值。新增 TestOwnedPartitions_NilReceiverSafe 与 TestClaimLease_NilReceiverSafe。KOC 里所有 c.owned 调用不加额外判空，全靠方法自身的 nil 安全。

### [accept_modified] [low] 回滚到 S1 之前镜像时旧二进制读带新键的 ConfigMap，go-zero 是否忽略未知键未列为验证项
已用模块缓存里的源码确认：go-zero v1.10.0 会忽略未知键。core/mapping/unmarshaler.go:1023-1030 只按结构体字段逐个取值；core/conf/config.go 的 toLowerCaseKeyMap 对未知键原样透传，不报错。go/db/go.mod 钉的是 v1.10.0，S1 不升级。因此不需要 Codex 用旧二进制实测，回滚规程也不需要「先 zone-up 生成不含新键的 ConfigMap」。改为在 config_test.go 新增 TestConfigLoaderIgnoresUnknownKeys：读取 ../../etc/db.yaml，在顶层、ServerConfig.Kafka 段、ServerConfig.RedisClient 段各注入一个 Future*ForRollbackTest 未知键（先断言替换锚点确实存在），然后 conf.LoadFromYamlBytes 必须成功。以后升级 go-zero 若改成严格模式，这条测试会先红。

### [accept_modified] [low] 新告警误报漏报：NoProgress 在长冷却时误报且看不到单分区卡住；RevokedOutsideRebalance 用 changes(gauge) 会漏掉同数量再平衡
DbRetryPartitionedNoProgress 做两处改动：把 executed 与 executed_nonowner 都算作有进展；并追加 unless on(namespace) (sum by (namespace) (db_store_cooldown_stores) > 0)。DbRetryRevokedOutsideRebalance 改用新计数器 db_claim_acquired_total（无 label，ownedPartitions.acquire 对非 nil 接收者每次加 1，启动即以 0 注册）的 increase[10m]。按分区的深度 gauge 不做：同一分区的重试与新消息走同一个 worker，属主卡住时该分区的 Kafka 消费也会停，而新告警 DbZoneNoPartitionClaims 与 Kafka lag 能覆盖；为它加 PartitionCnt 个序列，再配一套按分区的 executed 计数，收益不抵复杂度。这一局限写进设计文档 §11。

### [accept] [low] 组提交批次只按条数限制，没有字节上限；未显式设 WriteTimeout
攒批条件：条数达到 durableEnqueueMaxBatch=256，或累计字节 ≥ durableEnqueueMaxBatchBytes=8MiB，或通道暂时为空，满足任一条就停止攒批。取下一条之前先判字节，所以一批最多超出上限一条；单条超过上限的载荷单独成一批。异步化之后再加入队层的待确认总字节上限 durableEnqueueMaxPendingBytes=32MiB（db Pod 内存 limit 是 512Mi，只靠 1024 的条数上限，最坏会有约 1GiB）。超出时 submit 阻塞（背压），pendingBytes==0 时任何单条都放行。专用客户端显式设置：WriteTimeout=3s，ReadTimeout=WaitAOFTimeoutMs+1s，PoolSize=2，MaxRetries=-1。三个上限都做成 aofGroupEnqueue 的字段，默认取常量，只给单测改小。新增测试 TestAOFGroupEnqueue_BatchByteCap（maxBatchBytes 改为 1KiB，3 条各 600B，假 WAITAOF 被调用 ≥2 次且全部 ok）和 TestAOFGroupEnqueue_PendingBytesBackpressure（maxPendingBytes 改小，第二条阻塞到第一条完成；ctx 取消即以 canceled 返回）。

### [accept_modified] [low] D5 修订写「两项都必须作为独立发布单元」，与同一镜像同在第 2 步生效相矛盾
修订块第 1 条改为：④ 与 WAITAOF 随同一个 S1 镜像发布，S1 整体作为一个独立发布单元，不得与 TiDB Phase 1 任何迁移批次同窗口，也不得在同一次滚动里发布；两者开关独立（RetryRouting；WaitAOFNumLocal/NumReplicas，改值后须 rollout restart），指标独立（db_retry_route_total；db_durable_enqueue_*），按指标归因。不拆成「先 0/0 发镜像、再开 1/0」两步，理由有三：ConfigMap 由契约测试强制镜像 db.yaml，拆步要改两次 db.yaml、多出一次发布，还要让 TestEtcYamlRetryKeysValid 临时放宽；在 S2 之前，每多一次滚动就多一轮全服 eager 再平衡；第 2 步 ④ 处于 shared 模式，行为基本等于今天，WAITAOF 的影响能单独从 db_durable_enqueue_* 看出来。

### [accept_modified] [low] 同类入口遗漏：Redis 拆分工具键清单、横向扩展设计键表、DbTaskDeferredByStore 描述、L-04 设计稿 §9 两行
三处随本批修改：db-task-retry-alerts.yaml 中 DbTaskDeferredByStore（约 :53）与 DbTaskRetryQueueGrowing（约 :109）的描述，写成「kafka:retry:queue:{topic} 与按分区的 kafka:retry:queue:{topic}:p{N}」；shared-redis-split-by-purpose.md 的 dbctl 键清单那一行（工作区现约 :612，该文件另有他会话未提交的 +6 行，只改这一行），在 kafka:retry:queue:<topic> 之后追加「以及按 SCAN 列出的 kafka:retry:queue:<topic>:p*（topic 先做 glob 转义；p* 不能按精确键拼，否则搬迁时滞留存盘）」。另外两处不改，以控制文件数并避免与他人改动冲突，在设计文档 §2「需同步的同类入口」与 PROGRESS 条目里点名交给对应负责人：no-single-node-horizontal-scaling-20261001.md:113「ready,所有实例共享(不变)」已过时；db-task-store-unavailable-deferral.md:277-278 的 WAITAOF 与 ④ 两行已被 db-retry-owner-routing.md 取代。

### [accept_modified] [low] 重试协程不在 c.wg 里，Stop 的 release 可能早于它的最后一次认领，认领脚本会把实例重新登记回去
把重试协程纳入 c.wg，而不是新增 done channel：StartRetryConsumer 在 go 之前调 c.wg.Add(1)，协程内 defer c.wg.Done()。这样无论现行 Stop（cancel → wg.Wait → release）还是 S4 的五步 Stop（第 4 步 cancel+wg.Wait，第 5 步 release），都由构造保证 release 晚于重试协程退出，不需要再加等待点。协程的所有阻塞点都带 c.ctx.Done 分支（派发 select、Lua/EVAL 用 c.ctx），S4 合入后还要带 pullStop 分支，所以 Wait 不会死锁。新增 TestStop_RetryLoopJoinsWaitGroup：测试内实现一个最小 fake sarama.ConsumerGroup（Close 返回 nil），用 go-redis Hook 阻塞重试协程的认领 EVAL，然后在 goroutine 里调 Stop()。断言 100ms 内 Stop 未返回，且实例仍在 instances 登记表；放行 Hook 后 2s 内 Stop 返回，登记表里没有本实例，processing 为空，被认领的收据回到共享收件箱。

### [accept] [low] 规格引用与工作区状态描述不准（复验清单节号、compose 路径、契约测试他人改动、k8s_deploy 行号、README 锚点）
更正如下。① rollout 第 0 步 a) 改为：复验清单 §2「第 12 步」第 3 条「本机双 db 实例」，加上 §5「已知覆盖缺口」第 1、4、7 条（第 7 条由本修订的 TestStop_RetryLoopJoinsWaitGroup 覆盖）。② compose 文件是 deploy/docker-compose.yml。③ 工作区变化很快：2026-10-15 10:36 之后又有 676c64789..526780b4e 等快照提交；k8s_deploy_contract.tests.ps1 现有他会话未提交的 +26 行，shared-redis-split-by-purpose.md 有 +6 行。动手前对每个共享文件重新 git status/git diff，只追加或只改指定块。④ 一律按锚点定位，不按行号。k8s_deploy.ps1 当前锚点：Get-AuthoritativeScalar 约 :630，Get-ManifestDeploymentReplicas 约 :1248，Assert-MysqlPoolMirror 约 :1530，$dbSubShardCount 约 :3899，db heredoc 的 GroupID 约 :4334、SubShardCount 约 :4337，Apply-OneGoSvc 约 :5747，写路径入口末尾的 Assert-MysqlPoolMirror 与 Write-MysqlConnectionLedger 两行约 :7024-7025，新门禁调用接在后者之后。⑤ docs/README.md 新行追加在 :146（db-write-fencing-mysql.md）之后。KOC 锚点见 implementer_notes。

### [accept_modified] [low] F9 要求 stress_summarize/stress_snap 输出新指标，整体转给 S4 却无 S4 已接收的证据
已核对：tools/scripts/stress_summarize.ps1 的「2f. DB retry routing」段（:740-783）和 test_stress_summarize.ps1（:204-218 等）已随快照 592d7e718 提交。它读取的指标名与 label 取值和 S1 完全一致：db_retry_routing_mode{mode=owner|shared}；db_retry_queue_length{queue=ready|partitioned|processing|dead}；db_retry_route_total 的 action 为 executed、executed_nonowner、forwarded、requeued_revoked、misrouted、dead，mode 为 shared|owner，client_golang 输出按 label 字母序（action 在 mode 之前），与脚本正则一致。S1 不改这两个脚本。约束：metrics.go 的常量取值必须正好是这些字符串，RetryQueuePartitioned 必须等于 partitioned，由 TestRetryRoutingSeriesPrecreated 钉住；在常量注释里写明「改名须同改 stress_summarize.ps1 2f 段与 test_stress_summarize.ps1」。已知缺口：2f 段没有 db_owned_partitions 与 db_durable_enqueue_* 列。压测通常不触发入队失败，owner 校验在 E2E 步骤里直接抓 /metrics，所以本批不补；在设计文档 §13 与 PROGRESS 里登记为「下次改 stress_summarize 时补 owned、aof_bad、aof_ms 三列」。stress_snap.ps1 整页抓取 :9160，不需要改。

### [accept_modified] 漏掉的阻断项 A4-B4：B4 原场景下告警不会触发，只靠规程兜底
与第 2 条合并处理：新增不依赖新版二进制在线的告警 DbRetryPartitionedStranded（基于 Prometheus 里最后上报的样本，窗口 6h）；release-checklist 回滚段增加前置；回滚规程与 retry_requeue 工具保留。窗口过后只能靠 -check，这一残余在设计文档 §10、§11 与风险段如实写明。

### [accept_modified] 漏掉的阻断项 A4-F9（部分）：stress_summarize 新指标在 S1 范围内未落实
与第 15 条合并：S4 脚本阶段已提交的 2f 段与 S1 指标名逐一核对一致，S1 不改脚本；owned 与 aof 列登记为已知缺口，等下次改 stress_summarize 时补。

## 修订后的设计

# S1 修订稿（优先级高于原规格与复核意见；只写差异，没写到的部分按原规格执行）

## 0. 范围与文件数
- 目标不变：④ 重试只交给分区属主（a）（c），加上重试、死信、毒消息入队的 WAITAOF，再加 D5 修订。
- 核心改动：WAITAOF 从「同步等待」改为「组提交 + 异步确认、ack 推迟到确认之后」（§1）。重试协程纳入 c.wg（§2）。租约与 S2、S4 的挂点规则（§3）。告警重做（§6）。
- 本仓库文件数从 28 改为 27，其中新增 6 个；另有 Java 仓库 PARITY.md 1 行。清单见 file_list_changes。实现中如果需要再加文件、导致总数 ≥30，按 AGENTS §10.2 停下报告。

## 1. WAITAOF：组提交 + 异步确认（裁决 high）

### 1.1 为什么不保持同步
设计峰值按 2000 task/s/zone 计，80 个子 shard 每条 35ms，利用率约 87%。同步 WAITAOF 下，每条失败任务要占住子 shard 约 0.5s（everysec 平均值，最坏约 1s），是正常任务的 14 到 28 倍。峰值时 1% 的任务失败（排序锁忙、搬库冻结、单个非 zone 落点库不可用），就要多占约 12.5% 的容量，超过剩余余量。异步化以后，失败任务在 worker 上的成本只剩一次内存入队，这个队头阻塞（HOL）就消失了。

### 1.2 接口（durable_enqueue.go，替换原规格第 1 条）
```go
// 只用于「Kafka 位点提交之前、任务唯一副本首次写进 Redis」的入队点。
type durableEnqueue interface {
	// submit 把 payload LPUSH 进 queueKey,并按配置确认落盘。done 恰好调用一次:
	//   nil   = 副本已按配置确认(关闭 WAITAOF 时 = LPUSH 成功);
	//   非 nil = 未确认,调用方不得 ack;errors.Is(err, errDurableEnqueueCanceled) 表示停机或 ctx 取消。
	// done 可能在 submit 返回之前同步执行(directEnqueue 一律如此;其它实现在失败分支也可能如此),
	// 也可能在写协程里执行。done 必须快、不得阻塞、不得再调 submit。
	// ctx 只约束「交给入队层」这一步(含背压等待),不约束确认过程。
	submit(ctx context.Context, target, queueKey string, payload []byte, done func(error))
	start(ctx context.Context) error // 启动探针;aofGroupEnqueue 同时起写协程(写协程生命周期只由 close 结束)
	close()                          // 幂等;返回时,所有已 submit 请求的 done 都已调用过
}
```
错误哨兵：errEnqueueNotDurable、errWaitAOFRejected、errUnexpectedWaitAOFReply、errDurableEnqueueCanceled（取代原规格的 errDurableEnqueueStopped）、errDurableEnqueueNotStarted。
计数：metrics.ObserveDurableEnqueue(target, result, d)，result 的判定规则是：errors.Is(err, errDurableEnqueueCanceled) 为 canceled，errors.Is(err, errEnqueueNotDurable) 为 not_durable，其它非 nil 为 error，nil 为 ok。d 从 submit 开始计到 done。canceled 不进直方图。

### 1.3 directEnqueue（关闭 WAITAOF 时用，也是 c.durable==nil 时的默认实现）
submit 同步执行，逻辑同今天的 pushPayloadDurably：退避序列 0/100ms/500ms/1s，LPUSH 用调用方 ctx。只要 ctx.Err()!=nil（重试之前、退避等待中、或 LPUSH 失败时），就用 fmt.Errorf("%w: %w", errDurableEnqueueCanceled, ctx.Err()) 回调。done 在 submit 返回前调用。现有测试依赖同步 ack，这个语义保持不变。

### 1.4 aofGroupEnqueue
- 字段：rc *redis.Client（专用客户端）；ack aofAck；reqs chan *enqueueReq（容量 durableEnqueueQueueSize=1024）；maxBatch=256、maxBatchBytes=8<<20、maxPendingBytes=32<<20（字段，默认取常量，只给单测改小）；mu、pendingBytes、spaceFreed chan struct{}（每次释放字节时 close 并换新）、stopped、started；submitters sync.WaitGroup；ctx/cancel（内部生命周期，只由 close 取消，**不从 c.ctx 派生**）；runDone chan struct{}；closeOnce。
- 专用客户端参数：Addr/Password/DB 同 cfg；DisableIndentity: true；PoolSize: 2；MaxRetries: -1；ReadTimeout = WaitAOFTimeoutMs + 1s；WriteTimeout = 3s。
- submit 的步骤：
  1. ctx.Err()!=nil 时，以 canceled 完成后返回。
  2. 加锁。stopped 时以 canceled 完成；!started 时以 errDurableEnqueueNotStarted 完成（计 error，防止单测忘了 start 导致死锁）。否则 submitters.Add(1)。解锁。之后 defer submitters.Done()。
  3. 预留字节 n=len(payload)：pendingBytes==0 或 pendingBytes+n≤maxPendingBytes 时放行；否则在 spaceFreed、ctx.Done（canceled）、q.ctx.Done（canceled）三者之间 select 等待。
  4. 在 `reqs <- req`、ctx.Done、q.ctx.Done 三者之间 select；后两者先释放已预留的字节，再以 canceled 完成。
- run 写协程：`for first := range reqs`。q.ctx 已取消时，读到的请求一律以 canceled 完成。否则开始攒批：每取下一条之前先判断，条数达到 maxBatch、或累计字节 ≥ maxBatchBytes、或通道暂空，任一成立就停。然后 flush(batch)。
- flush 使用一条粘连连接 conn := rc.Conn()，写协程长期持有，出网络错误时关闭重建。截止时间 = 现在 + 预算（budget，默认 5s）。按 0/100ms/500ms/1s 退避，在截止之前循环：
  - 每轮 waitMs, ok := waitAOFBudgetMs(now, deadline, true, ack.timeout, 100ms)。ok=false 时，剩余请求以 errEnqueueNotDurable 完成，不发命令（timeout 为 0 表示无限阻塞）。
  - needPush=true 时，同一个 Pipelined 里先对每个待确认请求做 LPUSH，最后跟 Do("WAITAOF", numLocal, numReplicas, waitMs)；needPush=false 时只发 WAITAOF。
  - 逐条判 LPUSH：nil 表示已推送；Redis 错误回复（isRedisErrorReply）只让这一条以 error 完成；出现网络错误时整条 pipeline 结果未知，关闭 conn 重建，needPush=true，下一轮把未完成的请求全部重推（可能重复，可以接受）。
  - 判 WAITAOF：错误回复时，全部已推送请求以 errWaitAOFRejected 完成，不重试；回复形状错时以 errUnexpectedWaitAOFReply 完成，不重试；网络错误时重建 conn，needPush=true（新连接的 woff 覆盖不到旧写）；份数不足时 needPush=false，下一轮**只重发 WAITAOF**（同一连接，woff 不变，不产生重复 LPUSH）；份数满足时全部成功。
  - q.ctx 被取消时，剩余请求以 canceled 完成。
  - 每个请求完成时都按序执行：释放字节（close 并替换 spaceFreed）→ 计数 → 调 done。
- close：closeOnce 内依次执行：加锁置 stopped → q.cancel() → submitters.Wait() → close(reqs) → <-runDone → 关闭 conn 与客户端。这个顺序保证 done 恰好调用一次：请求要么从未进入 reqs，由 submit 自己的失败分支完成；要么被 run 读到，由 run 完成。close(reqs) 发生在所有 submitter 退出之后，不会向已关闭的 channel 发送。
- start(ctx)：先 probeAOFAck（带 WaitAOFTimeoutMs+2s 超时，用专用客户端的池连接，不用粘连连接），失败返回 error，Start 据此拒启。成功后置 started、go run()。
- 吞吐上限：每次 fsync 一批（≤256 条且 ≤8MiB），单实例最坏约 256 条/s。超出时 submit 背压阻塞 worker，效果等同今天的同步写法，表现为 Kafka lag，不丢数据。写进设计文档 §7。

### 1.5 调用点（key_ordered_consumer.go，替换原规格第 16 到 19 条中关于入队的部分）
- 新增 helper（中文注释写清原因）：
```go
// persistBeforeAck 把 Kafka 源任务的唯一副本交给入队层,onDone 负责 ack 或 failKafkaTask。
// task.acker==nil 时(只有直接拼结构体的单测)同步等 done:Sarama 的 MarkMessage 是高水位语义,
// 异步乱序 ack 会越过未确认的 offset。
func (w *worker) persistBeforeAck(task *workerTask, target, queueKey string, payload []byte, onDone func(error))
```
  acker==nil 时，submit 之后等一个容量为 1 的 channel 拿到结果，再调 onDone。否则直接 submit(w.ctx, target, queueKey, payload, onDone)。
- deferFailedTask 的 Kafka 分支：payload, err := encodeRetryPayload(dbTask, task.seq, partition, true)。编码出错时照旧 fatal 并 failKafkaTask。否则调 persistBeforeAck(task, DurableTargetRetry, w.retryReadyKeyFor(partition), payload, onDone)。onDone 中：nil 时 ackKafkaTask(task)；canceled 时打 INFO「shutdown: retry enqueue canceled; offset left uncommitted for replay: taskID=… partition=… seq=…」后调 failKafkaTask；其它错误照旧打 DATA-LOSS PREVENTED 的 ERROR 并 failKafkaTask。重试源分支（moveRetryReceipt，目的地改为 retryReadyKeyFor(originPartition)）保持同步，不走入队层。
- quarantineOrderingConflict 的 Kafka 分支：成功时在 onDone 里执行 CountTaskDeadLetter(label)、打原有的「fresh Kafka task durably quarantined」ERROR、ackKafkaTask；失败时 canceled 打 INFO，其它情况照旧打 ERROR，然后 failKafkaTask。
- persistPoisonMessage 的签名改为 (ctx, q durableEnqueue, deadQueueKey string, msg *sarama.ConsumerMessage, decodeErr error, done func(error))。encodePoisonMessage 出错时同步 done(err)。否则 q.submit(ctx, DurableTargetDead, …, func(err){ if err==nil { CountTaskDeadLetter(DeadLetterPoison) }; done(err) })。
  - ConsumeClaim 分发前解码失败的分支：调 persistPoisonMessage(h.consumer.ctx, h.consumer.enqueuer(), …, func(dlqErr){ 成功打原 ERROR 并 acker.ack(msg)；失败时 canceled 打 INFO，其它打 DATA-LOSS PREVENTED，然后 acker.fail(msg, fatalErr) })，接着 continue。acker.fail 从写协程调用是安全的：它带互斥锁，failureCh 非阻塞发送。
  - quarantinePoisonTask：acker==nil 时同步等待，规则同 persistBeforeAck。
- 删除 pushPayloadDurably，逻辑移入 directEnqueue。从 KOC **删除** saveToRetryQueue，生产代码不再用它；同名同签名 `saveToRetryQueue(ctx context.Context, redisClient redis.Cmdable, retryQueueKey string, task *db_proto.DBTask, seq uint64, partition int32) error`（实现为 encodeRetryPayload 加 LPush）移到 durable_enqueue_test.go，作为测试种子 helper。这样 placement_route_test.go **不用改**。
- handleTask 的 panic 恢复里调用的 deferFailedTask 走同一条路径，不需要特殊处理。排序锁由 handleTask 的 defer 释放，可能早于副本确认，这没有问题：重试稍后仍在锁加游标之下执行，与今天的同步路径一样，旧 seq 的重试会被更新的写判成 stale。

### 1.6 位点语义（写进代码注释与设计文档 §7）
- claimAcker 只 MarkMessage 连续完成的前缀（KOC:212-241）。未确认的 X 把本分区已提交位点停在 X；X 之后的消息照常执行，完成状态存在 acker.completed，确认后前缀一次推进。
- X 未确认时：failKafkaTask → acker.failed、claimCtx 取消、本 claim 不再标记任何位点 → ConsumeClaim 收到 failureCh 后排空返回 → 再平衡后从 X 起重投。重投时，写按游标判 stale，丢弃并 ack；读重算（没有等待方，缓存回写的是 MySQL 当前值）；重试副本可能重复，按 seq 收敛。
- 提交延迟上界 = 排队时间 + 单批预算 5s，小于 claimDrainTimeout 10s。正常再平衡与停机排空时，acker.pending 包含异步请求，这时入队层仍然活着，能等到确认后再提交。
- 进程崩溃：从最早未确认的 offset 起重投，幂等。
- acker.completed 的内存上界约为「吞吐 × (5s + 排队)」。

### 1.7 停机顺序
- 入队层生命周期独立于 c.ctx。Stop 的**最后一步**是 c.enqueuer().close()：在现行顺序里排在 consumer.Close() 之后；S4 合入后排在第 5 步 releaseRetryOwnership 之后，作为第 6 步。
- 现行顺序下，c.cancel() 之后还在执行的 handleTask 用已取消的 w.ctx 提交，得到 canceled，位点不提交，之后重投。ConsumeClaim 排空时，入队层仍在为已提交的请求做确认。

### 1.8 canceled 计数
见 1.2 与 1.3。告警只看 not_durable|error。

## 2. 重试协程纳入 c.wg
StartRetryConsumer 在 go 之前调 c.wg.Add(1)，协程内 defer c.wg.Done()。协程里所有阻塞点都带 c.ctx.Done 分支，S4 合入后还要带 pullStop 分支，与 S4 的 J 条一致：派发时遇到 pullStop，按 c.ctx.Done 处理，收据留在 processing，调 finishRetry 或 lease.exit。原规格「c.ctx.Done 时收据留在 processing、Stop 的 release 归还」这个出口的前提由此成立。

## 3. 租约挂点与 owner 模式对 claim 的依赖
- acquire 放在 ConsumeClaim 里 claimCtx **最终派生完成之后**、主 for 循环之前，即在 S2 的 sessionScope/yieldCtx 与 S4 的 WithCancel(WithoutCancel(session ctx)) 加观察 goroutine 之后。defer release(lease)。
- 每条排空路径的第一步都是 lease.close()：drainAfterStop（在 acker.stop() 之后）、S4 的 drainClaimForShutdown、消息通道关闭分支（等待之前）。等待用 claimDrained(acker, lease)。属主资格 = !closed && claimCtx.Err()==nil，以 claim 自身生命周期为准。在 S4 下，停机排空期间 claimCtx 不会被取消，已派发的重试照常跑完，idle() 会等它们。
- handleTask 里对重试的两次复核仍然看 task.claimCtx.Err()，判断的是「属主被撤销」；lease.closed 只表示「不再接新认领」。
- 不加 shared 兜底。owner 模式下 Kafka 不可用、长时间再平衡、S2 交接闸等待期间，重试停滞，时长等于 Kafka 消费空窗。告警 DbZoneNoPartitionClaims 兜底（§6）。设计文档 §6 混跑矩阵加一行「owner 模式：重试排空依赖 claim 存活」，并写处置：先修 Kafka 或交接闸；交接闸卡住、broker 可达且 MySQL 健康时，可 `kubectl -n mmorpg-zone-<z> set env deploy/db DB_RETRY_ROUTING=shared` 触发滚动，由 shared 实例排空 p*；broker 不可达时不要这么做。

## 4. ownedPartitions / claimLease
- acquire、release、live、lookup 以及 claimLease 的全部方法对 nil 接收者安全，语义见 rulings 第 7 条。
- acquire（非 nil 接收者）每次调 metrics.CountClaimAcquired()。

## 5. 指标（metrics.go，替换原规格的对应条目）
- db_durable_enqueue_total{target=retry|dead, result=ok|not_durable|error|canceled}，预建 2×4。
- db_durable_enqueue_seconds{target}：从 submit 到 done，canceled 不观测。
- 新增 db_claim_acquired_total（Counter，无 label，Help「Partition claims registered by ConsumeClaim (one per claim start); used to tell rebalance-time revocations from anomalies.」）与 CountClaimAcquired()。
- 其余按原规格：db_retry_route_total 2×6、db_owned_partitions、db_retry_routing_mode 2、db_ordering_lock_busy_total 2、db_retry_queue_length 增加 queue=partitioned。
- 常量注释写明：取值改名时，必须同改 tools/scripts/stress_summarize.ps1 的 2f 段与 test_stress_summarize.ps1（已核对，当前名字一致）。
- TestRetryRoutingSeriesPrecreated：retry_route 12 个、ordering_lock_busy 2 个、durable_enqueue 8 个、retry_routing_mode 2 个，db_claim_acquired_total 存在且为 0。

## 6. 告警（deploy/k8s/db-task-retry-alerts.yaml，完整清单；下面 PromQL 中的单引号，写进 yaml 时按文件惯例改成双引号）
文件头「来源」加 docs/design/db-retry-owner-routing.md。DbTaskDeferredByStore（约 :53）与 DbTaskRetryQueueGrowing（约 :109）的描述改为「kafka:retry:queue:{topic} 与按分区的 kafka:retry:queue:{topic}:p{N}」。
1. DbTaskRetryQueueGrowing（改 expr，warning，for 10m）：`max by (namespace) (sum by (namespace, pod) (db_retry_queue_length{queue=~'ready|partitioned'})) > 1000`
2. DbRetryPartitionedNoProgress（warning，for 0m）：
```
(min_over_time((max by (namespace) (db_retry_queue_length{queue='partitioned'}))[15m:1m]) > 0)
unless on (namespace) (sum by (namespace) (increase(db_retry_route_total{action=~'executed|executed_nonowner'}[15m])) > 0)
unless on (namespace) (sum by (namespace) (db_store_cooldown_stores) > 0)
```
   描述写明：按 namespace 聚合，看不到单个分区卡住，单分区属主卡住时同时看 Kafka lag 与 DbZoneNoPartitionClaims。
3. DbRetryPartitionedStranded（新增，critical，for 5m）：
```
(
  max by (namespace) (
    last_over_time(db_retry_queue_length{queue='partitioned'}[6h])
    and on (namespace, pod)
    topk by (namespace) (1, last_over_time(timestamp(db_retry_queue_length{queue='partitioned'})[6h:1m]))
  ) > 0
)
unless on (namespace) (count by (namespace) (db_retry_routing_mode) > 0)
```
   描述写明：最后一个上报的新版 db 退出时 p* 还有 {{ $value }} 条，而当前 namespace 没有任何能处理 p* 的实例（全是 S1 之前的镜像，或 db 全停），这些存盘不会自动落库。处置：确认全部实例是旧镜像或 shared 后，执行 go/db/cmd/retry_requeue（先 -check，再搬回）。搬回之后，6h 窗口内告警仍会响，以 -check 为准并静默；窗口过后告警自行消失，不代表已处理。
4. DbZoneNoPartitionClaims（新增，critical，for 5m）：
```
(sum by (namespace) (db_owned_partitions) == 0)
unless on (namespace)
  (count by (namespace) (db_retry_queue_length{queue='ready'}) > count by (namespace) (db_retry_routing_mode{mode='owner'}))
```
   unless 子句的作用：仍有 S1 之前的 Pod 时抑制（旧 Pod 上报 ready、不上报 routing_mode）。描述写明：存盘消费停止，冷登录读会超时；owner 模式下重试也停滞；查 broker、消费组 describe、S2 交接闸日志；处置见 §3。
5. DbRetryNonOwnerExecutionInOwnerZone：照原规格。
6. DbRetryRevokedOutsideRebalance（info）：
```
sum by (namespace) (increase(db_retry_route_total{action='requeued_revoked'}[10m])) > 0
unless on (namespace) (sum by (namespace) (increase(db_claim_acquired_total[10m])) > 0)
```
7. DbDurableEnqueueNotConfirmed（critical，for 0m）：`sum by (namespace, target, result) (increase(db_durable_enqueue_total{result=~'not_durable|error'}[5m])) > 0`。描述写明：canceled（停机）不计入；未确认时 Kafka 位点不提交、该 claim 失败并重投，存盘不丢，但该分区停滞；查 Redis AOF/fsync；不要靠关掉 WAITAOF 来止血，除非负责人同意。

## 7. 配置与配置测试
- config.go：按原规格。
- config_test.go：TestResolveRetryRouting、TestRedisWaitAOFDefaultsWhenMissing、TestRedisWaitAOFExplicitZeroDisables、TestConfigValidateRejects 照原规格；TestEtcYamlRetryKeysMatchDefaults 改为 **TestEtcYamlRetryKeysValid**（断言见 rulings 第 6 条）；新增 **TestConfigLoaderIgnoresUnknownKeys**（见 rulings 第 8 条）。
- 回滚兼容性已确认：go-zero v1.10.0 忽略未知键，旧二进制读带新键的 ConfigMap 能正常启动；回滚规程不需要先改 ConfigMap。

## 8. 文档
- docs/design/db-retry-owner-routing.md：
  - §2 增加「需同步的同类入口」，列出已改的与未改的：未改的是 no-single-node-horizontal-scaling-20261001.md:113 与 db-task-store-unavailable-deferral.md:277-278，交给对应负责人。
  - §6 增加 owner 模式对 claim 的依赖、S2 交接与 Kafka 故障时的停滞和处置。
  - §7 改写为异步组提交：接口、位点语义（§1.6）、吞吐上限、粘连连接只重发 WAITAOF、重复副本来源只剩网络错误与 Kafka 重投；增加「WAITAOF 不覆盖游标、收据 ack、搬移；AOF 尾丢失会让旧写重放，残余由 MySQL 条件写封死；不得表述为 Redis 崩溃安全；幂等前提是游标没有随 Redis 回退」。
  - §10 回滚：release-checklist 前置；Stranded 告警的 6h 窗口；go-zero 未知键已确认。
  - §11 列出全部 7 条告警及其窗口与局限。
  - §13 写 Codex 命令（见 §10），以及 stress_summarize 2f 已核对、owned 与 aof 列的缺口。
- global-data-layer-tidb-decision.md 的 D5 修订块第 1 条改为：「retry/dead 队列全不动」改为允许两项改动：④（新增键族 `kafka:retry:queue:{topic}:p{N}`，开关 `ServerConfig.Kafka.RetryRouting`）与重试/死信/毒消息入队补 WAITAOF（`ServerConfig.RedisClient.WaitAOF*`）。两项随同一个 S1 镜像发布，S1 整体作为一个独立发布单元，不得与 TiDB Phase 1 任何迁移批次同窗口，也不得在同一次滚动里发布；两项的开关与指标各自独立，按 db_retry_route_total / db_durable_enqueue_* 归因。第 2 到 4 条照原规格。
- docs/design/shared-redis-split-by-purpose.md：只改 dbctl 键清单那一行，内容见 rulings 第 12 条。
- docs/ops/release-checklist.md：在「快速回滚单个 Deployment」条目（约 :165）之后追加一条：「- [ ] **go/db 回滚到 S1（重试只交给分区属主）之前的版本**：先按 [db-retry-owner-routing.md](../design/db-retry-owner-routing.md) §10 切回 RetryRouting=shared 并 rollout restart，用 `go run ./cmd/retry_requeue -check` 确认所有 `kafka:retry:queue:{topic}:p*` 为 0，再回滚镜像。旧版本看不到 p*，直接回滚会让其中的存盘滞留；已经回滚的，立即执行 retry_requeue 搬回（告警 DbRetryPartitionedStranded）。」
- docs/PROGRESS.md：在原规格条目基础上追加以下内容：WAITAOF 改为异步确认（偏离原规格的理由）；新增两条告警；未同步的同类入口；stress_summarize 2f 缺 owned/aof 列；文件数 27；「未编译、未测试，待 Codex 验证」；Java 对应「无（不适用）」。
- 其余文档（db-task-kafka-partition-contract.md、merge-zone-runbook.md、README.md）照原规格。
- manifests/go-svc/db.yaml **不再修改**：热备语义与「kubectl scale 会绕过门禁」写进设计文档 §8 和 Assert-DbReplicasWithinPartitionContract 的函数注释。S4 也改这个 manifest，不改能减少冲突。

## 9. 测试（只列差异）
- retry_routing_test.go 新增 TestOwnedPartitions_NilReceiverSafe、TestClaimLease_NilReceiverSafe、TestOwnedPartitions_AcquireCountsClaimAcquired、TestStop_RetryLoopJoinsWaitGroup（见 rulings 第 13 条）。原规格放在 retry_ownership_test.go 的 TestRetryOwnership_ClaimFromUsesGivenListAndRenewsLease 与 TestRetryOwnership_ClaimDelegatesToReadyKey **移到 retry_routing_test.go**，retry_ownership_test.go 不改。
- durable_enqueue_test.go：
  - 保留原规格的用例，并全部按异步语义断言：用容量为 1 的 channel 接 done，或用 acker.wait() 限时 2s。
  - TestAOFGroupEnqueue_StoppedReturnsError 改名为 TestAOFGroupEnqueue_SubmitAfterCloseCountsCanceled。
  - 新增：TestDurableEnqueue_CanceledCtxCountsCanceled（direct 与 group 各一）、TestAOFGroupEnqueue_CloseCompletesEveryRequestOnce（32 个 goroutine 并发 submit 与 close 赛跑，每个 done 恰好一次，总数一致，-race 下干净）、TestAOFGroupEnqueue_BatchByteCap、TestAOFGroupEnqueue_PendingBytesBackpressure、TestAOFGroupEnqueue_NotDurableResendsOnlyWaitAOF（假 WAITAOF 先回 [0,0] 再回 [1,0]，列表里每条载荷恰好一份）、TestAOFGroupEnqueue_SubmitBeforeStartFails。
  - 新增两条确定性异步用例，用一个实现 durableEnqueue 的 fake，持有 done 不立即调用：TestDeferFailedTask_AsyncEnqueueDoesNotBlockNextTask（同一 acker 跟踪 offset 0、1；0 失败后被 fake 持有；1 成功、handler 被调用、session.marked() 为空；放行 done(nil) 后 marked 为 [0,1]）；TestDeferFailedTask_ConfirmFailureFailsClaimAndBlocksLaterOffsets（done(errEnqueueNotDurable) 后 marked 为空、claimCtx 被取消）。
  - 这里放测试种子 helper saveToRetryQueue（签名不变）。
- config_test.go 见 §7。metrics_test.go 见 §5。placement_route_test.go、key_ordered_consumer_test.go、retry_ownership_test.go 都不改，必须无新增失败。

## 10. 上线与回滚（只列差异）
- 第 0 步 a)：复验清单 §2「第 12 步」第 3 条（本机双 db 实例），以及 §5 第 1、4、7 条。compose 文件是 deploy/docker-compose.yml。
- Codex (1) 在 go/db 下执行：
  - `go test -count=1 -v -run 'TestClaimLease|TestOwnedPartitions|TestRetryRouting|TestRequeuePartitioned|TestRetryQueueDepths|TestConsumeClaim|TestRetryOwnership|TestAOFGroupEnqueue|TestDirectEnqueue|TestDurableEnqueue_|TestProbeAOFAck|TestCheckWaitAOFReply|TestWaitAOFBudgetMs|TestDeferFailedTask_|TestQuarantine_|TestPoison_|TestStop_|TestConcurrentWorkers|TestRetryConsumer' ./internal/kafka/`
  - `go test -count=1 -v -run 'TestResolveRetryRouting|TestRedisWaitAOF|TestConfigValidateRejects|TestEtcYamlRetryKeysValid|TestConfigLoaderIgnoresUnknownKeys' ./internal/config/`
  - -race 的正则加上 TestAOFGroupEnqueue|TestDeferFailedTask_|TestStop_。
  - 可选：本机有 promtool 时，把 db-task-retry-alerts.yaml 的 spec 抽成规则文件跑 `promtool check rules`；没有就报告「告警语法未验证」。
  - 其余命令与通过标准照原规格。
- 第 1 步（kind/staging）追加两项验收：
  - 异步不阻塞：稳态负载下，锁住 10% 合成测试玩家的排序锁 60s。健康玩家的 w_task/w_queue（S4 已合入时看 stress_summarize 2d 段，否则看 db_task_stage_seconds{stage=op_total}）与不锁时相差 ≤20%；Kafka lag 不持续增长；db_durable_enqueue_seconds 均值 ≤1.5s；not_durable/error 为 0。
  - 滚动重启：db_durable_enqueue_total{result=canceled} 可以增长，not_durable/error 必须为 0，DbDurableEnqueueNotConfirmed 不响。
- 生产第 2 步的验收改为：not_durable|error 不增长（滚动期间 canceled 允许增长），DbZoneNoPartitionClaims 不响。
- 回滚到 S1 之前的镜像：照原规格，并按 release-checklist 新增的条目执行。紧急回滚之后，告警 DbRetryPartitionedStranded 会在约 10 分钟内响起（序列过期 5m 加 for 5m），处置用 retry_requeue。

## 11. 风险（只列差异）
- 原第 9 条改为：实例全部回到 S1 之前的镜像后，DbRetryPartitionedStranded 只在 6h 窗口内有效；窗口过后只能靠 retry_requeue -check 与回滚规程。
- 新增：异步确认让同分区位点提交最多晚约 5s；未确认或崩溃时，重投窗口从最早未确认的 offset 开始，由游标与 seq 吸收。入队层内存上界 32MiB 加 1024 条。单实例吞吐上限约每次 fsync 一批，超出背压成 Kafka lag。
- 新增：owner 模式下没有 claim 时（Kafka 故障、交接闸卡住）重试停滞，由 DbZoneNoPartitionClaims 兜底。
- 新增：WAITAOF 不覆盖游标与收据 ack，AOF 尾巴丢失可能让旧写重放，这一残余靠 MySQL 条件写封死。
- 新增：网络错误时组提交重推，仍可能产生重复副本；死信里的重复按 taskID 或 (topic,partition,offset) 人工去重。
- 文件数 27，余量 2。

## 12. 与其它会话的接缝（在原规格「顺序与重叠」基础上追加）
- S4：Stop 五步之后追加第 6 步 c.enqueuer().close()。重试协程已入 c.wg，由第 4 步覆盖。S1 新增的两个派发 select（consumeOneRetryFrom、consumeOneOwnedRetry）都要保留或加上 pullStop 分支。drainClaimForShutdown 的第一步是 lease.close()。
- S2：acquire 放在 sessionScope/yieldCtx 派生之后；交接闸等待期间没有 claim，owner 模式下重试停滞，这是已知、可接受的。S2 的 Setup/Cleanup 里 S1 没有挂点，S1 只在 ConsumeClaim 里登记。
- S3：KOC 里 ConsumeClaim 解码后的 observeProducerPartition 那一行与 metrics.go 的 producerPartitionTotal 是 S3 的改动（已进快照提交），保留，不要动。


## 文件清单变化
与原规格（28 个，其中新增 6 个）相比，修订后本仓库共 27 个，其中新增 6 个；另有 Java 仓库 PARITY.md 1 行。

移出清单（3 个）：
- go/db/internal/kafka/placement_route_test.go：不再改动。saveToRetryQueue 从 KOC 删除后，以同名同签名 (ctx, redisClient redis.Cmdable, retryQueueKey, task, seq, partition) error 放进 durable_enqueue_test.go，现有 4 处调用不用改。
- go/db/internal/kafka/retry_ownership_test.go：不再改动。claimFrom 的两条用例移到 retry_routing_test.go。
- deploy/k8s/manifests/go-svc/db.yaml：不再改动。注释内容移到设计文档 §8 与 k8s_deploy.ps1 中 Assert-DbReplicasWithinPartitionContract 的函数注释；S4 也改这个 manifest，不改可减少冲突。

加入清单（2 个，修改）：
- docs/design/shared-redis-split-by-purpose.md：只改 dbctl 键清单那一行（工作区现约 :612），补上 kafka:retry:queue:<topic>:p*（经 SCAN，topic 先做 glob 转义）。该文件有他会话未提交的 +6 行，动手前重新 git diff，只改这一行。
- docs/ops/release-checklist.md：在「快速回滚单个 Deployment」条目（约 :165）之后追加一条，内容是 go/db 回滚到 S1 之前版本的前置（切 shared、retry_requeue -check p*=0，或回滚后立即搬回）。

内容有变化、仍在清单里的：
- go/db/internal/kafka/durable_enqueue.go（新增）：改为异步 submit/done 接口、字节预算、粘连连接、关闭协议、canceled 哨兵。
- go/db/internal/kafka/durable_enqueue_test.go（新增）：用例按异步语义重写，新增 6 条，并放入 saveToRetryQueue 种子 helper。
- go/db/internal/kafka/retry_routing.go（新增）：四个方法对 nil 接收者安全，acquire 计 db_claim_acquired_total。
- go/db/internal/kafka/retry_routing_test.go（新增）：加入 nil 安全、claim 计数、Stop 与 wg 的用例，以及从 retry_ownership_test 移来的 claimFrom 用例。
- go/db/internal/kafka/key_ordered_consumer.go：三类 Kafka 源入队改为 persistBeforeAck 异步确认；删除 pushPayloadDurably 与 saveToRetryQueue；StartRetryConsumer 纳入 c.wg；Stop 最后一步是 enqueuer.close()；canceled 时打 INFO。
- go/db/internal/metrics/metrics.go 与 metrics_test.go：result 加 canceled（预建 8 个），新增 db_claim_acquired_total。
- go/db/internal/config/config_test.go：TestEtcYamlRetryKeysMatchDefaults 改为 TestEtcYamlRetryKeysValid；新增 TestConfigLoaderIgnoresUnknownKeys。
- deploy/k8s/db-task-retry-alerts.yaml：共 7 条，其中新增 DbRetryPartitionedStranded、DbZoneNoPartitionClaims；改写 NoProgress、RevokedOutsideRebalance、NotConfirmed；修改两条现有告警的描述。
- docs/design/db-retry-owner-routing.md（新增）、global-data-layer-tidb-decision.md（D5 第 1 条措辞）、docs/PROGRESS.md（追加的内容变多）。

完全不变、仍在清单里的：retry_ownership.go、store_failure.go、config.go、db.go、go/db/etc/db.yaml、go/db/cmd/retry_requeue/main.go、tools/merge_zone/preflight.go、tools/merge_zone/merge_unit_test.go、tools/scripts/k8s_deploy.ps1、tools/scripts/tests/k8s_deploy_contract.tests.ps1、docs/design/db-task-kafka-partition-contract.md、docs/ops/merge-zone-runbook.md、docs/README.md。

明确不改：tools/scripts/stress_summarize.ps1 与 test_stress_summarize.ps1（2f 段已由 S4 脚本阶段提交，指标名已核对一致）；docs/design/no-single-node-horizontal-scaling-20261001.md 与 db-task-store-unavailable-deferral.md（在设计文档 §2 与 PROGRESS 里点名，交给对应负责人）。

## 给实现者的说明
1. 阅读顺序：先读原规格，再读复核，最后读本修订稿；冲突时以修订稿为准。全部注释、文档、提交说明用中文，日志与错误文本用英文（同原规格）。按 AGENTS §10.1，不跑任何构建或测试，交付说明写「未编译、未测试，待 Codex 验证」。

2. 工作区变化很快：2026-10-15 10:36 之后又有 676c64789、9b8b902da、41789f474、9f320e485、526780b4e 等快照提交，S3 对 KOC 与 metrics.go 的改动也已进入提交（observeProducerPartition 那一行与 producerPartitionTotal 都保留）。动手之前，对每个目标文件重新 git status 和 git diff HEAD。已知他会话未提交的改动：tools/scripts/tests/k8s_deploy_contract.tests.ps1 有 +26 行，docs/design/shared-redis-split-by-purpose.md 有 +6 行，docs/PROGRESS.md 有 +7 行。这些文件只追加，或只改指定的块与行；不得回退、不得格式化他人内容。PROGRESS 只在末尾追加（merge=union）。

3. 一律按锚点定位，不按行号。KOC 当前锚点（工作区）：claimAcker :174-287；waitClaimPending :291（删除）；ackKafkaTask/failKafkaTask :302-320；NewKeyOrderedKafkaConsumer :873；buildWorker :955；Start :1013（retryOwner.start 之前插入 enqueuer().start）；StartRetryConsumer :1060（加 c.wg.Add/Done）；claimRetryReceipt :1082；consumeRetryQueue :1157；consumeOneRetryTask :1184；worker.start :1256；routeToSubShard :1311；processTaskBatch :1437（superseded 分支在 :1533-1547）；handleTask :1557；Setup :1711；ConsumeClaim :1743（毒消息分支 :1796-1810，S3 的 observeProducerPartition 在 :1814）；deferFailedTask :2127（Kafka 分支 :2171-2189）；quarantineOrderingConflict :2213；persistPoisonMessage :2241；quarantinePoisonTask :2254；pushPayloadDurably :2270；saveToRetryQueue :2289（从 KOC 删除，移入测试文件）；Stop :2297；releaseRetryOwnership :2311。k8s_deploy.ps1 的锚点见 rulings「规格引用不准」一条。

4. 合入顺序仍建议 S3 → S2 → S4 → S1。S1 若在 S4 之前合，代码结构要方便 S4 rebase：Stop 的最后一行就是 c.enqueuer().close()；派发 select 预留 pullStop 分支的位置，在注释里写明「S4 合入后加 case <-c.pullStop，处理同 c.ctx.Done」；ConsumeClaim 里 acquire 紧挨在 claimCtx 派生语句之后，drainAfterStop 的第一步是 lease.close()。

5. 异步入队的几个硬性约束（评审会逐条查）：
   - done 恰好调用一次。close 的顺序必须是「置 stopped → cancel 内部 ctx → submitters.Wait() → close(reqs) → 等 run 退出」。
   - 入队层内部 ctx 不得从 c.ctx 派生。
   - acker==nil 的 Kafka 任务必须同步等待。
   - flush 内部预算耗尽必须是 errEnqueueNotDurable，不能被判成 canceled。
   - 份数不足时只重发 WAITAOF，不重复 LPUSH，粘连连接不换。
   - done 回调里不得调用 submit，也不得阻塞。
   - 重试源路径（moveRetryReceipt、deadLetterRetry、ackRetryReceipt、分拣与搬回脚本）不走入队层，不加 WAITAOF。
   - checkWaitAOFReply、waitAOFBudgetMs、isRedisErrorReply 的语义逐字照抄 go/data_service/internal/routing/route_publish.go:439-478，并在注释里写明「两处要改一起改」。

6. 不变量照原规格：不改 markAppliedSeq / markAppliedOwnerEpoch 的调用顺序与位置，不改 dbOpHandler 签名，不碰 processDBTask、expand status、luaMarkAppliedEpoch（这些归 S4）。重试的 processCtx 保持 w.ctx。

7. metrics 常量取值必须正好是 stress_summarize.ps1 2f 段在读的字符串：partitioned、executed、executed_nonowner、forwarded、requeued_revoked、misrouted、dead、shared、owner。不要改名。

8. 告警 yaml 里的 PromQL 字符串按文件惯例用双引号（修订稿为了少转义写成了单引号）。DbRetryPartitionedStranded 里的 timestamp() 子查询与 topk by (namespace) 要原样保留，不要简化成 max（会被早先退出 Pod 的旧值误报）。

9. 文件数是 27。若实现中发现必须再改别的文件（例如 key_ordered_consumer_test.go 的 newTestWorker 因新字段需要调整），先确认零值语义能否避免改动；确需改动就计入总数，达到 30 即停止并报告（AGENTS §10.2）。

10. Java 仓库 D:\luyuan\xuanming-server-mmo\PARITY.md 追加一行「不适用」，不计入本仓库文件数；PROGRESS 条目写「对应 Java 版本号：无（不适用）」。

11. 交付说明里如实写出以下几点：WAITAOF 偏离原规格、改为异步确认，以及理由；stress_summarize 2f 缺 owned/aof 列；no-single-node 文档与 L-04 设计稿 §9 未同步；告警语法是否经过 promtool 验证以 Codex 结果为准。
