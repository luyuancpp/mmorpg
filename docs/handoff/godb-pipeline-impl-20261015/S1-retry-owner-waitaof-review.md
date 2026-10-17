# S1 对抗复核意见（结论：needs_fix）

## [high] WAITAOF 在子 shard worker 里同步等待。存储故障或局部延后时，整个 zone 的消费吞吐会被压到约「子 shard 数 / fsync 周期」，规格没有量化，也没有验收标准
- 证据：KOC:2156-2166：Kafka 源失败时同步调用 saveToRetryQueue，返回后才 ackKafkaTask。runSubShard（KOC:1370-1400）每个子 shard 串行执行 processTaskBatch→handleTask。规格 durable_enqueue.go 写的是「push 返回 nil 才表示副本已确认」；current_state 自己写了「WAITAOF 在 everysec 下平均约 0.5s」。组提交会把各子 shard 的请求对齐到同一次 fsync，而 everysec 大约每秒 fsync 一次，所以每个失败中的子 shard 大约每秒只能处理 1 条。go/db/etc/db.yaml:19、22 是 PartitionCnt 10 × SubShardCount 8 = 80，即故障路径每个 zone 约 80 条/秒。Round 18 稳态入流约 66 task/s（附录 L14），λ_peak 按 2000/s 估。走这条路径的不只 MySQL 冷却：placement_route.go:275-283 的 frozen_deferred（relocate 期间）、errPlacementStoreUnavailable（单个非 zone 落点库挂掉）、排序锁忙也都走它。这些失败写会把子 shard 卡住约 1 秒，同一子 shard 上的健康玩家一起积压。rollout 和 risks 都没有这一项
- 修法：二选一。(1) 推荐：Kafka 源入队改成异步完成。deferFailedTask 只登记请求，确认后在回调里调 ackKafkaTask；未确认就调 failKafkaTask。claimAcker 带互斥锁，按连续前缀提交位点，fail-closed 语义不变；ConsumeClaim 排空本来就等 acker.pending。整盘快照是 last-write-wins，与合并器相同，同 key 后续更高 seq 先落库不影响正确性，要在设计文档 §7 写明这条论证。(2) 保持同步：在设计文档 §7 和 risks 写明故障路径吞吐上限的公式和数值；在 staging 演练里加一项，按峰值入流冻结或锁忙 10% 玩家，对比 lag 斜率和 p99，并写出通过标准

## [medium] B4 要求的告警「p* 有积压但没有 owner 实例在消费」在 B4 的原场景下不会触发。原场景是 shared 本身有缺陷、只能直接回滚到 S1 之前的镜像，规格只靠规程兜底，而复核明确说只靠 runbook 文字不够
- 证据：附录 L102（B4）：要配一条告警「p* 有积压、但没有 owner 模式实例在消费」，并说「只靠 runbook 文字兜底不够」。规格 A4-B4 和 risks 第 9 条自认实例全部回到旧镜像后告警哑火。DbRetryPartitionedNoProgress 依赖的 db_retry_queue_length{queue="partitioned"} 只有 S1 二进制会上报
- 修法：加一条不依赖存活新版实例的告警，例如 `max by (namespace) (last_over_time(db_retry_queue_length{queue="partitioned"}[2h])) > 0 unless on (namespace) (count by (namespace) (db_retry_routing_mode) > 0)`：已消失的 Pod 最后一次上报 p*>0，而当前该 namespace 没有任何 Pod 上报 routing_mode，就说明 p* 滞留且没有人在消费，回滚后 2 小时内能响。description 指向 retry_requeue。设计文档 §11 写明窗口长度，以及窗口过后只能靠 -check

## [medium] 每次正常滚动都可能误触 critical 告警 DbDurableEnqueueNotConfirmed：停机时 ctx 已取消，此时入队按 result=error 计数
- 证据：Stop（KOC:2274-2276）先 c.cancel() 再 wg.Wait()。之后仍在跑的 processTaskBatch 会继续调 handleTask：acquireOrderingLock(w.ctx)（KOC:1590-1597）失败，进 deferFailedTask，再用已取消的 w.ctx 入队。规格里 directEnqueue 和 aofGroupEnqueue 都按 ok|error 计 ObserveDurableEnqueue。告警 `increase(db_durable_enqueue_total{result=~"not_durable|error"}[5m]) > 0`，for 0m，级别 critical
- 修法：调用方 ctx 已取消或超时（errors.Is(err, context.Canceled/DeadlineExceeded) 且 ctx.Err()!=nil）时单独计 result="canceled" 或不计数，告警只看 not_durable 和真正的 error；指标预建和 TestRetryRoutingSeriesPrecreated 同步改；补一条单测：已取消的 ctx 不计 error

## [medium] 设计文档 §7 的论证「重试源搬移丢了只回到前一状态」以及幂等依据「游标判 stale」有前提没写：游标和收据 ack 也在同一个没有 WAITAOF 的 Redis 里，Redis 丢失 AOF 尾巴时两者会一起回退，而 MySQL 和 Kafka 位点已经前进
- 证据：markAppliedSeq（KOC:1670）和 ackRetryReceipt（KOC:1677-1683）都是普通 Redis 写。K8s Redis 是 appendonly yes，appendfsync 默认 everysec（redis.yaml:95），崩溃时最多丢约 1 秒。场景：seq95 的重试已落库、已 ack，seq100 落库、游标推到 100，随后 Redis 丢了这 1 秒，游标回到 95 以下，95 的收据重新出现在 processing。seq100 的 Kafka 位点已经提交，不会重投，95 重放后把 MySQL 覆盖回旧档。规格的组提交重发和 Kafka 重投会制造更多同 seq 副本，让这个窗口更容易被撞上
- 修法：设计文档 §7 和 risks 如实写明：WAITAOF 只解决「Kafka ack 后唯一副本丢失」，不解决游标或收据 ack 丢失导致旧写重放；这一残余由 MySQL 条件写封死。不得表述为「Redis 崩溃安全」。幂等依据补一句「前提是游标没有随 Redis 一起回退」

## [medium] owner 模式把重试执行绑在 Kafka 消费组健康上：没有 claim 就没有租约，重试完全停止。规格的混跑矩阵和 risks 没写，与 S2 交接闸的交互也没写
- 证据：规格 consumeRetryQueueOwner 中「leases 为空即热备直接 return」；租约只在 ConsumeClaim 存续期间存在（KOC:1724-1733）。Start 的消费循环出错时每秒重试一次（KOC:1040-1048）。附录 ③ 选定方案 2(a)：S2 交接闸在 Setup 里阻塞，等待期间不会有 ConsumeClaim。所以 Kafka 不可用、长时间再平衡、S2 切组或反向切组的等待期间，owner 模式下本 zone 的 p* 只进不出；shared 模式下这些重试照常排空。S2 还会把 claimCtx 改成从 yieldCtx 派生
- 修法：risks 和设计文档 §6 加一行：owner 模式下重试排空依赖 claim 存活，Kafka 故障或交接期间重试停滞。S2 切组、反向切组前先切 shared，或明确接受停滞。加告警：p* 增长且 sum(db_owned_partitions)=0 持续 N 分钟。ordering_and_overlap 里补一条给 S2 的不变量：lease 必须用最终定稿的 claimCtx 登记，acquire 放在 claimCtx 派生完成之后

## [medium] TestEtcYamlRetryKeysMatchDefaults 把 db.yaml 的 RetryRouting 钉死为 "shared"，rollout 第 4 步改成 owner 时测试会变红，而第 4 步没有计划改测试
- 证据：规格 config_test.go：「RetryRouting==\"shared\"」。rollout 第 4 步：「db.yaml 改 RetryRouting: owner」
- 修法：测试只断言取值 ∈ {shared, owner}，并断言 k8s ConfigMap 的镜像值与 db.yaml 一致（契约测试已有这一项）；或者在 rollout 第 4 步写明同时改这条测试，并计入那次发布的文件数

## [low] ownedPartitions 的 lookup/release 没有写明 nil 接收者安全。直接拼结构体、owned 为 nil 的现有单测在派发成功后会触发空指针，与「key_ordered_consumer_test/placement_route_test 不改即通过」的承诺矛盾
- 证据：placement_route_test.go:128-147 的 claimRetry 构造 KeyOrderedKafkaConsumer 时没有 owned，并调用 consumeOneRetryTask。规格第 10 条：shared 派发成功后调用 `c.owned.lookup(partition)==nil`。规格 retry_routing.go 第 2 条只写了 acquire 和 live 对 nil 接收者安全
- 修法：规格明确 acquire/release/live/lookup 四个方法对 nil 接收者都安全（lookup 返回 nil，release 只 close 不碰 map 和指标），并在 TestOwnedPartitions_* 里补 nil 接收者用例

## [low] 回滚到 S1 之前的镜像时，旧二进制要读带新键（RetryRouting、WaitAOF*）的 ConfigMap。能否启动取决于 go-zero 是否忽略未知键，规格没有列为验证项
- 证据：Apply-OneGoSvc 先 apply ConfigMap 再 apply Deployment（k8s_deploy.ps1 工作区约 :5574-5585）；rollout undo 只回退 Pod 模板，ConfigMap 保留新键。附录 ③ 复核（L80 附近）把「go-zero v1.10.0 是否忽略未知键」列为需 Codex 复核的项。S1 rollout 第 2 步回滚只写了「回旧镜像」
- 修法：rollout 第 0 或第 1 步加一项 Codex 验证：用 S1 之前的 db.exe 加载含新键的 db.yaml，必须正常启动。若 go-zero 拒绝未知键，回滚规程改为先 zone-up 生成不含新键的 ConfigMap，再回滚镜像

## [low] 新增告警有误报和漏报：DbRetryPartitionedNoProgress 在 MySQL 长时间冷却时误报，且按 namespace 聚合，看不到单个分区卡住；DbRetryRevokedOutsideRebalance 用 changes(gauge)，抓取间隔内完成的同数量再平衡会被漏掉
- 证据：规格 consumeRetryQueueOwner 在 homeStoreCoolingDown 为真时只分拣不执行，不计 executed，冷却超过 15 分钟就会触发「查 owner 实例是否还在」这类误导性告警。partitioned 是全部 p* 之和，某个 p{P} 卡住、其它分区仍在 executed 时告警被压住。db_owned_partitions 是 gauge，抓取周期 15–30 秒
- 修法：NoProgress 的 unless 子句加上 `or on(namespace) (sum by (namespace)(db_store_cooldown_stores) > 0)`，并把 executed_nonowner 也算作有进展；可选加按分区的深度 gauge（label partition，基数 ≤ PartitionCnt）。RevokedOutsideRebalance 改用新计数器 db_claim_acquired_total 的 increase

## [low] 组提交批次只按条数（256）限制，没有字节上限。大载荷批次可能超过 go-redis 写/读超时，触发整批重发，最后连续 fail-closed
- 证据：玩家行最大 1 MiB（config.go DefaultBlobMaxRowBytes），载荷是整份 DBTask。规格 durableEnqueueMaxBatch=256，ReadTimeout=WaitAOFTimeoutMs+1s，没有显式设 WriteTimeout（go-redis 缺省 WriteTimeout=ReadTimeout）。一批可达上百 MiB
- 修法：run() 攒批时同时限制累计字节（例如 ≤8 MiB，单条超限就单独成批），显式设 WriteTimeout，并补一条大载荷单测

## [low] D5 修订写「两项都必须作为独立发布单元」，但 ④ 和 WAITAOF 同一个镜像、同在第 2 步一起生效，措辞矛盾，出问题时也难以归因
- 证据：规格 global-data-layer-tidb-decision.md 修订块第 1 条；rollout 第 2 步「发布 S1 镜像，RetryRouting=shared、WaitAOF 1/0/1500」
- 修法：修订改为「S1（④+WAITAOF）作为一个独立发布单元」。或者分步：第 2 步先以 WaitAOF 0/0 发镜像观察，再改 ConfigMap 开启 1/0，并执行 rollout restart，单独观察 db_durable_enqueue_* 与 lag

## [low] 同类入口有遗漏（AGENTS §11.4）：未来 Redis 拆分工具的键清单、横向扩展设计里的键表、一条现有告警的描述、L-04 设计稿 §9 两行都没有同步 p*
- 证据：shared-redis-split-by-purpose.md:596：dbctl 只按精确键 `kafka:retry:queue:<topic>` 搬，按它实现会漏掉 p*，搬迁时滞留存盘（该文件有他人未提交改动）。no-single-node-horizontal-scaling-20261001.md:113 写「ready,所有实例共享(不变)」。db-task-retry-alerts.yaml:53 的 DbTaskDeferredByStore 描述仍写「留在 kafka:retry:queue:{topic}」。db-task-store-unavailable-deferral.md:277-278 的两行只在 PROGRESS 里注明被取代
- 修法：告警第 53 行描述随本文件一起改，补上 p*。其余文件有他人改动或会超出文件数：在设计文档 §2 和 PROGRESS 条目里点名这几处需要同步（尤其 dbctl 键清单必须加 `kafka:retry:queue:<topic>:p*`），交给对应负责人

## [low] 停机时重试协程不在 c.wg 里，release 可能早于它的最后一次认领。规格「c.ctx.Done→收据留 processing、Stop 时 release 归还」这一出口的前提因此不成立
- 证据：StartRetryConsumer 的协程（KOC:1057-1075）不计入 c.wg；Stop（KOC:2274-2283）在 wg.Wait() 之后就 releaseRetryOwnership。认领脚本会先 ZADD 续租（retry_ownership.go:117-121），把刚被 release 摘掉的实例重新登记回去。S1 又在这个协程里加了分拣和多源认领
- 修法：给重试协程一个 done channel（或计入独立的 WaitGroup），Stop 在 releaseRetryOwnership 之前等它退出；补一条 Stop 顺序单测（复验清单 §5 第 7 条也列了这项缺口）

## [low] 规格里有几处引用和工作区状态描述不准，按它动手会改错位置或覆盖他人改动
- 证据：(1)「复验清单第 3 节缺口项」：清单 §3 是「清理」（L569），覆盖缺口在 §5（L599）。(2) compose 文件实际是 deploy/docker-compose.yml。(3) tools/scripts/tests/k8s_deploy_contract.tests.ps1 有他人未提交改动 +206 行，规格的「工作区他人改动」没有列它；:80-97 现在约 :89，:1873-1883 现在约 :1926-1946。(4) k8s_deploy.ps1 的行号在工作区整体后移约 8 行（例如 :4100→:4108，:6776→:6786）。(5) docs/README.md 的锚点现为 :146
- 修法：按工作区现状更正以上引用；把 k8s_deploy_contract.tests.ps1 列入「动手前重新 git diff、只追加或只改指定块」的名单

## [low] F9 要求 stress_summarize/stress_snap 输出新指标，规格整体转给了 S4，S1 范围内没有落实，也没有 S4 已接收的证据
- 证据：附录 L113（F9）；规格 ordering_and_overlap ④「db_retry_* 列交给 S4 的测量批」
- 修法：交付前核对 S4 规格确实包含 stress_summarize DB 段的 db_retry_route_total、db_owned_partitions、db_durable_enqueue_* 列；如果没有，S1 自己补（届时文件数 29，仍低于 30）

## 漏掉 / 似是而非的阻断项
- A4-B4：复核要求的告警「p* 有积压但没有 owner 实例在消费」在 B4 原场景下不会触发。原场景是 S1 二进制在 shared 下也有缺陷、只能直接回滚到 S1 之前的镜像，此时没有任何 Pod 上报 partitioned。规格只靠规程（先切 shared 并跑 -check、紧急时跑搬回工具）兜底，复核明确说只靠 runbook 文字兜底不够
- A4-F9（部分）：stress_summarize/stress_snap 输出新指标被整体转给 S4，S1 范围内没有落实，也没有 S4 规格已接收的证据
