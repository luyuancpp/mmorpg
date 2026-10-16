// Package metrics exposes Prometheus instrumentation for the db service.
//
// Why this exists: Round 15 (45k×1zone) saw login-side
// dataloader_preload_callback_wait_seconds{ok} avg climb 11.8 ms → 31.4 ms
// (+166 %) over the 18-min window, while kafka_send stayed flat at ~3.3 ms.
// `cb_wait` is the gap between "last Kafka ack returned" and "dispatcher
// fan-in complete", i.e. **db consumer SELECT + Redis Pub/Sub roundtrip**.
// Up to Round 15 the db side was a black box. These histograms split the
// per-task budget so Round 16 can see exactly which sub-step regresses.
//
// Stages (per processed Kafka message, in time order):
//
//	op_total       — entire processTaskWithoutLock invocation
//	op_handler     — DB op handler (read = MySQL SELECT + cache write-back;
//	                 write = MySQL Save + cache write-back)
//	cache_write    — Redis SET of the write-back cache key (subset of
//	                 op_handler, recorded again standalone so we can see
//	                 cache contention separately)
//	result_publish — LPush task:result:{taskId} + Expire + Publish notify
//	                 (read ops only). The Publish is the trigger that wakes
//	                 the login dispatcher.
//
// Round 20 起补的四个 stage(stress_summarize.ps1 2d 段按这些名字读):
//
//	sql        — runDBOp 里 handler 的耗时(写 = 大字段闸 + Save,读 = SELECT),op_handler 的子集
//	task_total — handleTask 整段(含排序锁、游标 / epoch 守卫、选库、SQL、复核、核锁、缓存、游标推进、释放锁)
//	queue_wait — 任务进 worker 通道到 handleTask 开始
//	kafka_e2e  — 生产者 CreateTime 到 handleTask 结束,只统计 Kafka 来源(重试源不计);跨机有时钟偏差,
//	             负值丢弃;大于 5s 落进 +Inf 桶,但 sum / count 求出的均值仍然准确
//
// Redis 分步、游标守卫、子 shard 批量、排序锁租约与 go-redis 连接池的指标见 pipeline.go。
//
// Counter labels: op = read | write | unknown, result = ok | error.
package metrics

import (
	"net/http"
	"shared/sqlpoolstats"
	"sync"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/zeromicro/go-zero/core/logx"
)

const subsystem = "db"

var stageBuckets = []float64{
	0.0005, 0.001, 0.002, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2, 5,
}

var (
	taskStageSeconds = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "task_stage_seconds",
		Help:      "Per-task sub-stage latency. stage: op_total | op_handler | cache_write | result_publish | sql | task_total | queue_wait | kafka_e2e (producer CreateTime to task end, Kafka-origin only, subject to cross-host clock skew). op: read | write | unknown.",
		Buckets:   stageBuckets,
	}, []string{"stage", "op"})

	taskResultTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "task_result_total",
		Help:      "Terminal outcome of each processed task. op: read | write | unknown; result: ok | error.",
	}, []string{"op", "result"})

	blobBytes = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "blob_bytes",
		Help:      "Stored (base64-encoded) byte size of each pb blob column on the write path. Read p99, not max.",
		Buckets:   blobBuckets,
	}, []string{"table", "column"})

	blobRowBytes = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "blob_row_bytes",
		Help:      "Sum of all stored pb blob column bytes per written row. Read p99, not max.",
		Buckets:   blobBuckets,
	}, []string{"table"})

	blobGuardTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "blob_guard_total",
		Help:      "Blob size gate hits. level: warn (>= WarnRatio of limit) | reject (over limit). column=__row__ means the per-row total.",
	}, []string{"table", "column", "level"})

	// ownerEpochGuardTotal 是落库前归属 epoch 守卫的全量分类计数(每条 write 任务记一次)。
	// 它回答的是「兼容窗口还剩多少旧版生产者(legacy_zero,收口时应归零)」「该 key 第一次带
	// epoch 落库(first)」「归属换主后的第一笔写(advance,应与传送 / 交接次数对得上)」这类
	// 部署期问题;真正的健康红线是下面的 staleOwnerWriteRejectedTotal。
	//
	// outcome 的取值**必须**与下面 OwnerEpoch* 常量一一对应 —— 改常量必须同改这里的 Help,
	// 否则按 Help 配出来的告警 / 看板表达式会匹配不到任何序列,而空表达式在 Prometheus 里
	// 不报错、只静默不告警,和「一切正常」长得一模一样。
	ownerEpochGuardTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "owner_epoch_guard_total",
		Help:      "Owner-epoch guard verdicts on write tasks. outcome: match | legacy_zero | first | advance | stale | read_error.",
	}, []string{"outcome"})

	// staleOwnerWriteRejectedTotal 是 cross-zone-scene-travel.md §6 不变量 3 点名的健康信号:
	// 被废黜节点的迟到 DBTask 被拒的次数,压测期应恒为 0。单独建一个无 label 的 counter,
	// 是为了让 Codex 验证清单与告警规则能按固定名字直接引用,不依赖 label 匹配。
	staleOwnerWriteRejectedTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "stale_owner_write_rejected_total",
		Help:      "Write tasks dropped because their owner_epoch is older than the max epoch already applied for this (topic,key,msg_type) — Redis key consumer:applied_epoch:* (reentry-barrier 6.3 / cross-zone-scene-travel R3). Must stay 0 under stress.",
	})

	// placementGuardTotal 是按落点选库(player-storage-placement.md §6.2)与落库后复核(§6.3)的
	// 全量分类计数:每条 read / write 任务选库时记一次,复核不通过或复核读失败时再记一次。
	// 上线观察口径见设计 §13:stale_topic 应恒 0(非 0 先查 C++ home_zone_unknown);
	// frozen_deferred 持续上涨 = 搬库工具冻结后没有切换 / 解冻(工具崩溃),应告警。
	//
	// outcome 取值**必须**与下面 Placement* 常量一一对应,改常量同改 Help —— 理由同 ownerEpochGuardTotal。
	placementGuardTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "placement_guard_total",
		Help:      "Player storage placement routing verdicts (player-storage-placement.md 6.2/6.3). op: read | write. outcome: placed | home | legacy | stale_topic | frozen_deferred | missing_required | lookup_error | recheck_moved.",
	}, []string{"op", "outcome"})

	// placementStoreOpenTotal 记每一次**真正发生**的按需打开落点库(§6.1);打开失败后的冷却期内
	// 直接复用上次错误的请求不重复计数,所以 error 的速率就是「每个冷却期一次」,不会被任务量放大。
	placementStoreOpenTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "placement_store_open_total",
		Help:      "On-demand placement store opens (player-storage-placement.md 6.1). result: ok | error (ping / schema gate / unknown database) | rejected (neither in the allow list nor admitted by AllowStoreFamilies).",
	}, []string{"result"})

	// placementOpenStores 是本进程当前常驻的落点库个数(含本 zone 库)。
	placementOpenStores = prometheus.NewGauge(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "placement_open_stores",
		Help:      "Placement stores (MySQL databases) currently open in this db process, including the zone's own database.",
	})

	// 以下四个是 L-04(docs/design/db-task-store-unavailable-deferral.md §3.7)的观测面:任务为什么没落库、
	// 多少进了死信、重试队列有多长(哪类库在冷却见 storeCooldownCollector)。label 全是封闭集合,不含 player_id(AGENTS.md §9)。

	// taskDeferredTotal 在 deferFailedTask 一处计数,覆盖所有延后原因。
	taskDeferredTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "task_deferred_total",
		Help:      "Tasks pushed back to the retry queue instead of completing. op: read | write | unknown. reason: conn_refused | conn_lost | store_readonly | store_busy | store_cooldown | store_open_failed | ordering_lock_busy | lease_lost | placement_frozen | placement_moved | expand_lock_busy | redis_unavailable | other.",
	}, []string{"op", "reason"})

	// taskDeadLetterTotal 在每个死信写入点成功写入后计数。
	taskDeadLetterTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "task_dead_letter_total",
		Help:      "Tasks moved into kafka:dead:queue:{topic}. reason: budget_exhausted | malformed_payload | legacy_payload | invalid_partition | ordering_incomparable | stale_topic | missing_required | poison.",
	}, []string{"reason"})

	// taskReadAbandonedTotal 记读任务被放弃的次数:读不改数据,不进死信。
	taskReadAbandonedTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "task_read_abandoned_total",
		Help:      "Read tasks answered with a failure or dropped instead of retried / dead-lettered. reason: store_unavailable (failure receipt sent at once) | budget_exhausted (dropped after the retry budget).",
	}, []string{"reason"})

	// retryQueueLength 是重试链路三个列表的长度,重试循环每拍采样一次。ready 与 dead 是同一 topic 所有实例共用的
	// 列表,每个副本报的是同一个数,告警里取 max;processing 是本实例自己的列表。
	retryQueueLength = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "retry_queue_length",
		Help:      "Length of the retry lists, sampled every retry tick. queue: ready (shared by the topic) | processing (this instance) | dead (shared by the topic).",
	}, []string{"queue"})

	// producerPartitionTotal 是 db_task 分区契约(docs/design/db-task-kafka-partition-contract.md「分区函数契约」)
	// 的消费端观测:ConsumeClaim 解码成功后,对每条消息按契约重算应在的分区并核对 Kafka key。只计数,不拒绝。
	// 写的 verdict!=ok 说明有生产者破坏契约(critical);读的不符说明还有旧版 login 在跑。
	// label 是封闭集合(3×4=12 条序列),不含 player_id(AGENTS.md §9)。
	producerPartitionTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "task_producer_partition_total",
		Help:      "Kafka db_task messages checked against the producer partition contract (CRC-32(key) mod N, docs/design/db-task-kafka-partition-contract.md). op: read | write | other. verdict: ok | partition_mismatch | key_mismatch | invalid. Observation only: no message is rejected.",
	}, []string{"op", "verdict"})

	registerOnce sync.Once
)

// blobBuckets 覆盖 256B ~ 8MiB(每档 ×4)。
//
// 之所以要 histogram 而不是 gauge/max:max 只会告诉你「有过一次 3MB 的写入」,
// 既定位不到是哪条业务线在涨,也分不清是常态还是孤例。p99 才是「设计上限该
// 定在哪」以及「本周有没有变胖」的唯一可读信号。上界给到 8MiB 是为了在
// MEDIUMBLOB 的 16MiB 天花板之前还能看见分布,而不是全挤在 +Inf 桶里。
var blobBuckets = []float64{
	256, 1024, 4096, 16384, 65536, 262144, 1048576, 4194304, 8388608,
}

func register() {
	registerOnce.Do(func() {
		prometheus.MustRegister(taskStageSeconds, taskResultTotal, blobBytes, blobRowBytes, blobGuardTotal,
			ownerEpochGuardTotal, staleOwnerWriteRejectedTotal,
			placementGuardTotal, placementStoreOpenTotal, placementOpenStores,
			taskDeferredTotal, taskDeadLetterTotal, taskReadAbandonedTotal, retryQueueLength)
		// 计数器序列从无到 1 时 increase() 会漏掉第一次(deploy/k8s/owner-epoch-alerts.yaml 记过这条经验),
		// 所以告警要用到的取值先以 0 建出序列。
		for _, op := range []string{"read", "write"} {
			for _, reason := range deferReasons {
				taskDeferredTotal.WithLabelValues(op, reason).Add(0)
			}
		}
		for _, reason := range deadLetterReasons {
			taskDeadLetterTotal.WithLabelValues(reason).Add(0)
		}
		for _, reason := range []string{ReadAbandonedStoreUnavailable, ReadAbandonedBudgetExhausted} {
			taskReadAbandonedTotal.WithLabelValues(reason).Add(0)
		}
		// db_task 分区契约观测:12 条序列全部以 0 建出,写不符的 critical 告警才能抓到第一次。
		prometheus.MustRegister(producerPartitionTotal)
		for _, op := range producerPartitionOps {
			for _, verdict := range producerPartitionVerdicts {
				producerPartitionTotal.WithLabelValues(op, verdict).Add(0)
			}
		}
		// 消费组按 zone 拆的交接闸(docs/design/db-consumer-group-per-zone.md):告警按 result / outcome 的具体取值与
		// conflict 计数写,先以 0 建出全部序列,第一次出现才抓得到。
		prometheus.MustRegister(consumerGroupRebalanceGapSeconds, consumerGroupHandoffTotal, consumerGroupHandoffGateTotal,
			consumerGroupHandoffWaitingSeconds, consumerGroupHandoffWaitSeconds, consumerGroupHandoffConflictTotal)
		for _, result := range handoffSeedResults {
			consumerGroupHandoffTotal.WithLabelValues(result).Add(0)
		}
		for _, outcome := range handoffGateOutcomes {
			consumerGroupHandoffGateTotal.WithLabelValues(outcome).Add(0)
		}
		consumerGroupHandoffConflictTotal.Add(0)
	})
}

// 延后原因(db_task_deferred_total 的 reason)。前四个与 storeerr.Kind.Reason() 的取值一一对应。
const (
	DeferConnRefused      = "conn_refused"       // 连接数满(1040 / 1203 / 1226):服务端在应答,只拒新连接
	DeferConnLost         = "conn_lost"          // 连接断开或连不上
	DeferStoreReadOnly    = "store_readonly"     // 库只读(主从切换期间)
	DeferStoreBusy        = "store_busy"         // 锁等待超时、死锁、TiDB 写冲突
	DeferStoreCooldown    = "store_cooldown"     // 库在冷却期内(已确认不可达),选库时直接挡回
	DeferStoreOpenFailed  = "store_open_failed"  // 按需打开落点库失败
	DeferOrderingLockBusy = "ordering_lock_busy" // 排序锁被上一任持有者占着
	DeferLeaseLost        = "lease_lost"         // 排序锁失租(SQL 前 / SQL 中被取消 / 写完核锁时发现)
	DeferPlacementFrozen  = "placement_frozen"   // 搬库冻结中
	DeferPlacementMoved   = "placement_moved"    // 落库后复核发现落点变了
	DeferExpandLockBusy   = "expand_lock_busy"   // 扩容锁被占
	DeferRedisUnavailable = "redis_unavailable"  // Redis 侧的网络错误或超时(守卫、选库、复核、缓存)
	DeferOther            = "other"              // 其它:SQL 的非存储侧错误、编程错误等,照旧耗预算
)

var deferReasons = []string{
	DeferConnRefused, DeferConnLost, DeferStoreReadOnly, DeferStoreBusy, DeferStoreCooldown, DeferStoreOpenFailed,
	DeferOrderingLockBusy, DeferLeaseLost, DeferPlacementFrozen, DeferPlacementMoved, DeferExpandLockBusy,
	DeferRedisUnavailable, DeferOther,
}

// 进死信的原因(db_task_dead_letter_total 的 reason)。
const (
	DeadLetterBudgetExhausted      = "budget_exhausted"      // 写任务的重试次数用完
	DeadLetterMalformedPayload     = "malformed_payload"     // 重试载荷解不出来
	DeadLetterLegacyPayload        = "legacy_payload"        // 旧格式重试载荷,没有可信的原始分区
	DeadLetterInvalidPartition     = "invalid_partition"     // 原始分区越界或为负
	DeadLetterOrderingIncomparable = "ordering_incomparable" // 游标不可比(跨分区等)
	DeadLetterStaleTopic           = "stale_topic"           // 写所在 topic 的 zone 不是玩家的 home_zone
	DeadLetterMissingRequired      = "missing_required"      // Placement.Required=true 且无落点记录的写
	DeadLetterPoison               = "poison"                // Kafka 消息解不出 DBTask
)

var deadLetterReasons = []string{
	DeadLetterBudgetExhausted, DeadLetterMalformedPayload, DeadLetterLegacyPayload, DeadLetterInvalidPartition,
	DeadLetterOrderingIncomparable, DeadLetterStaleTopic, DeadLetterMissingRequired, DeadLetterPoison,
}

// 读任务被放弃的原因(db_task_read_abandoned_total 的 reason)。
const (
	ReadAbandonedStoreUnavailable = "store_unavailable" // 库打不开或在冷却期内:立即回失败回执
	ReadAbandonedBudgetExhausted  = "budget_exhausted"  // 重试次数用完:丢弃,不进死信
)

// 重试链路的三个列表(db_retry_queue_length 的 queue)。
const (
	RetryQueueReady      = "ready"
	RetryQueueProcessing = "processing"
	RetryQueueDead       = "dead"
)

// CountTaskDeferred 记一次延后。op 为 read / write / unknown,reason 用 Defer* 常量。
func CountTaskDeferred(op, reason string) {
	register()
	taskDeferredTotal.WithLabelValues(op, reason).Inc()
}

// CountTaskDeadLetter 记一次进死信,reason 用 DeadLetter* 常量。
func CountTaskDeadLetter(reason string) {
	register()
	taskDeadLetterTotal.WithLabelValues(reason).Inc()
}

// CountReadAbandoned 记一次读任务被放弃,reason 用 ReadAbandoned* 常量。
func CountReadAbandoned(reason string) {
	register()
	taskReadAbandonedTotal.WithLabelValues(reason).Inc()
}

// SetRetryQueueLength 更新一个重试列表的长度,queue 用 RetryQueue* 常量。
func SetRetryQueueLength(queue string, n int64) {
	register()
	retryQueueLength.WithLabelValues(queue).Set(float64(n))
}

// db_task_producer_partition_total 的 op 取值:DBTask.op 为 read / write 原样记,其余一律 other。
const (
	ProducerPartitionOpRead  = "read"
	ProducerPartitionOpWrite = "write"
	ProducerPartitionOpOther = "other"
)

// db_task_producer_partition_total 的 verdict 取值。
const (
	ProducerPartitionOK          = "ok"                 // Kafka key == DBTaskKey(task.key),且分区 == CRC-32(key) mod N
	ProducerPartitionMismatch    = "partition_mismatch" // key 合规,但消息所在分区不是契约分区
	ProducerPartitionKeyMismatch = "key_mismatch"       // Kafka key 不是 DBTaskKey(task.key)(含 nil / 空 key)
	ProducerPartitionInvalid     = "invalid"            // 无法判定:消息或任务为空,或本消费者的分区数非正
)

var producerPartitionOps = []string{ProducerPartitionOpRead, ProducerPartitionOpWrite, ProducerPartitionOpOther}

var producerPartitionVerdicts = []string{
	ProducerPartitionOK, ProducerPartitionMismatch, ProducerPartitionKeyMismatch, ProducerPartitionInvalid,
}

// CountProducerPartition 记一次分区契约核对结果。op 用 ProducerPartitionOp* 常量,verdict 用 ProducerPartition* 常量。
func CountProducerPartition(op, verdict string) {
	register()
	producerPartitionTotal.WithLabelValues(op, verdict).Inc()
}

// ── 消费组按 zone 拆与不停服交接(docs/design/db-consumer-group-per-zone.md;go/db/internal/kafka/group_handoff.go)──
//
// 六个指标都不带 player_id,label 是封闭集合(4 + 8 个取值),每个 Pod 一份。告警见 deploy/k8s/db-task-retry-alerts.yaml
// 的 db-task.consumer-group-handoff 组。result / outcome 的取值**必须**与下面 HandoffSeed* / HandoffGate* 常量一一对应,
// 改常量同改 Help 与告警,理由同 ownerEpochGuardTotal(空表达式不报错、只静默不告警)。
var (
	// consumerGroupRebalanceGapSeconds 从一个 session 的 Cleanup 到下一个 session 的 Setup 完成,含交接闸前的等待与
	// Setup 失败后的重新入组;回答「再平衡让本 zone 的存盘消费停了多久」。进程第一个 session 没有前驱,不记。
	consumerGroupRebalanceGapSeconds = prometheus.NewHistogram(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "consumer_group_rebalance_gap_seconds",
		Help:      "Time from a consumer group session Cleanup to the next session Setup completing, including the group handoff gate wait. The first session of a process is not observed.",
		Buckets:   []float64{0.5, 1, 2, 3, 5, 10, 15, 20, 30, 45, 60, 120},
	})

	// consumerGroupHandoffTotal 是 Setup 里逐分区的播种结果:每个分区每个 session 至多记一次;已落定(settled)的分区
	// 不再查询,也不计数。
	consumerGroupHandoffTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "consumer_group_handoff_total",
		Help:      "Per-partition group handoff seeding results in session Setup. result: seeded (marked the peer group's committed offset, not +1) | own_present (this group already has a committed offset) | no_offsets (neither group has one; starts from Offsets.Initial) | aborted (offset query failed; Setup fails and the member rejoins).",
	}, []string{"result"})

	// consumerGroupHandoffGateTotal 是第一次入组前交接闸的每次判定(轮询 1s,所以 ≤1/s/Pod)。
	consumerGroupHandoffGateTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "consumer_group_handoff_gate_total",
		Help:      "Group handoff gate verdicts before joining the consumer group, one per poll. outcome: no_peer_offsets | peer_released (pass) | wait_peer_holding | wait_peer_rebalancing | wait_peer_state_unknown | wait_own_behind_peer | wait_peer_unavailable | wait_own_unavailable (wait).",
	}, []string{"outcome"})

	// consumerGroupHandoffWaitingSeconds 是当前在交接闸前已经等了多久;没在等时为 0。告警按它的阈值写(45s / 300s)。
	consumerGroupHandoffWaitingSeconds = prometheus.NewGauge(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "consumer_group_handoff_waiting_seconds",
		Help:      "How long this db process has been waiting at the group handoff gate so far; 0 when not waiting.",
	})

	// consumerGroupHandoffWaitSeconds 在交接闸放行时记一次本次等待的总时长。
	consumerGroupHandoffWaitSeconds = prometheus.NewHistogram(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "consumer_group_handoff_wait_seconds",
		Help:      "Total time spent at the group handoff gate, observed once when the gate releases.",
		Buckets:   []float64{0.5, 1, 2, 5, 10, 15, 30, 45, 60, 120, 300, 600, 1800},
	})

	// consumerGroupHandoffConflictTotal 记守望发现对方组正在消费本 topic(Stable、持有本 topic 分区、已提交位点前进)
	// 并让出本 session 的次数;非 0 多半是旧发布物被误部署。单独无 label,告警按固定名字引用。
	consumerGroupHandoffConflictTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "consumer_group_handoff_conflict_total",
		Help:      "Times the group handoff watcher saw the peer group consuming this topic (Stable, holding its partitions, committed offsets advancing) and yielded this session.",
	})
)

// db_consumer_group_handoff_total 的 result 取值。
const (
	HandoffSeedSeeded     = "seeded"      // 本组没有位点、对方有:MarkOffset(对方原值,不加 1)
	HandoffSeedOwnPresent = "own_present" // 本组已有位点:不动
	HandoffSeedNoOffsets  = "no_offsets"  // 两组都没有:从 Offsets.Initial(oldest)起步;从未写入的分区属正常
	HandoffSeedAborted    = "aborted"     // 位点查询失败:Setup 报错,成员 1s 后重新入组
)

var handoffSeedResults = []string{HandoffSeedSeeded, HandoffSeedOwnPresent, HandoffSeedNoOffsets, HandoffSeedAborted}

// db_consumer_group_handoff_gate_total 的 outcome 取值(修订稿 R5 / R12 后的 8 个;peer_unreachable_own_complete 已删除)。
const (
	HandoffGateNoPeerOffsets        = "no_peer_offsets"         // 放行:对方在本 topic 上没有位点,也没有成员持有分区
	HandoffGatePeerReleased         = "peer_released"           // 放行:对方 Empty / Dead,或 Stable 且不持有本 topic
	HandoffGateWaitPeerHolding      = "wait_peer_holding"       // 等待:对方 Stable 且持有本 topic 的分区
	HandoffGateWaitPeerRebalancing  = "wait_peer_rebalancing"   // 等待:对方 PreparingRebalance / CompletingRebalance
	HandoffGateWaitPeerStateUnknown = "wait_peer_state_unknown" // 等待:对方状态为空串或未知(只凭正面证据放行)
	HandoffGateWaitOwnBehindPeer    = "wait_own_behind_peer"    // 等待(R7):两组在同一分区都有位点且对方更大
	HandoffGateWaitPeerUnavailable  = "wait_peer_unavailable"   // 等待:对方位点或 DescribeGroups 查询失败
	HandoffGateWaitOwnUnavailable   = "wait_own_unavailable"    // 等待:本组位点查询失败,或 inspector 建不出来
)

var handoffGateOutcomes = []string{
	HandoffGateNoPeerOffsets, HandoffGatePeerReleased, HandoffGateWaitPeerHolding, HandoffGateWaitPeerRebalancing,
	HandoffGateWaitPeerStateUnknown, HandoffGateWaitOwnBehindPeer, HandoffGateWaitPeerUnavailable, HandoffGateWaitOwnUnavailable,
}

// ObserveRebalanceGap 记一次「上一个 session 结束 → 本 session Setup 完成」的间隔。
func ObserveRebalanceGap(d time.Duration) {
	register()
	consumerGroupRebalanceGapSeconds.Observe(d.Seconds())
}

// CountHandoffSeed 记一个分区的播种结果,result 用 HandoffSeed* 常量。
func CountHandoffSeed(result string) {
	register()
	consumerGroupHandoffTotal.WithLabelValues(result).Inc()
}

// CountHandoffGate 记一次交接闸判定,outcome 用 HandoffGate* 常量。
func CountHandoffGate(outcome string) {
	register()
	consumerGroupHandoffGateTotal.WithLabelValues(outcome).Inc()
}

// SetHandoffWaiting 更新当前在交接闸前已等待的时长;放行或退出等待时传 0。
func SetHandoffWaiting(d time.Duration) {
	register()
	consumerGroupHandoffWaitingSeconds.Set(d.Seconds())
}

// ObserveHandoffWait 在交接闸放行时记一次本次等待的总时长。
func ObserveHandoffWait(d time.Duration) {
	register()
	consumerGroupHandoffWaitSeconds.Observe(d.Seconds())
}

// CountHandoffConflict 记一次守望让出(对方组正在消费本 topic)。
func CountHandoffConflict() {
	register()
	consumerGroupHandoffConflictTotal.Inc()
}

// storeCooldownCollector 在每次抓取时现算 db_store_cooldown_stores:此刻仍在库级冷却期内(探活确认整库不可达)的库个数,
// 按连接池类别分。不用 Gauge 在进出冷却时设值:冷却到期后要等下一次选库探活才删记录,某个库到期后再也没有流量时
// 设值式的指标会一直挂着,告警跟着常亮。
type storeCooldownCollector struct {
	source func() map[string]int
	desc   *prometheus.Desc
}

func (c *storeCooldownCollector) Describe(ch chan<- *prometheus.Desc) { ch <- c.desc }

func (c *storeCooldownCollector) Collect(ch chan<- prometheus.Metric) {
	counts := c.source()
	for _, pool := range []string{PoolHome, PoolExtraZone, PoolExtraNonZone} {
		ch <- prometheus.MustNewConstMetric(c.desc, prometheus.GaugeValue, float64(counts[pool]), pool)
	}
}

// NewStoreCooldownCollector 只构造不注册(单测用自己的注册表)。
func NewStoreCooldownCollector(source func() map[string]int) prometheus.Collector {
	desc := prometheus.NewDesc(prometheus.BuildFQName("", subsystem, "store_cooldown_stores"),
		"Databases currently cooling down after a probe confirmed they are unreachable. pool: home | extra_zone | extra_non_zone.",
		[]string{"pool"}, nil)
	return &storeCooldownCollector{source: source, desc: desc}
}

// RegisterStoreCooldown 把落点库注册表的冷却计数接到默认注册表。进程内调一次;注册失败只记日志,不挡落库。
func RegisterStoreCooldown(source func() map[string]int) {
	register()
	if err := prometheus.Register(NewStoreCooldownCollector(source)); err != nil {
		logx.Errorf("[metrics] register store cooldown collector failed (db_store_cooldown_stores unavailable): %v", err)
	}
}

// 落点选库的 outcome label 取值(player-storage-placement.md §6.2 表的 outcome 列 + §14 的 recheck_moved)。
// 集合有界(8 个),op 只有 read / write,不含 player_id 等高基数维度(AGENTS.md §9)。
const (
	PlacementPlaced          = "placed"           // 有落点记录:落记录指向的库(读任务的冻结记录也算这一格)
	PlacementHome            = "home"             // 无记录、有 home_zone:落 home 库(写任务此时 home 必然 == 本进程 zone)
	PlacementLegacy          = "legacy"           // 无记录、无 home_zone:落本进程 zone 库(旧语义)
	PlacementStaleTopic      = "stale_topic"      // 写任务所在 topic 的 zone ≠ 玩家 home_zone:进死信,不落库(P-6)
	PlacementFrozenDeferred  = "frozen_deferred"  // 记录处于搬库冻结:写延后重试,不耗重试次数
	PlacementMissingRequired = "missing_required" // Placement.Required=true 且无记录:写进死信,读回失败
	PlacementLookupError     = "lookup_error"     // MGET 失败或值畸形:按可重试错误处理(耗重试次数),fail-closed
	PlacementRecheckMoved    = "recheck_moved"    // 落库后复核发现路由变化(或写任务遇到冻结):不标记游标、不写共享缓存,重试
)

// 按需打开落点库的 result label 取值。
const (
	StoreOpenOK       = "ok"
	StoreOpenError    = "error"
	StoreOpenRejected = "rejected"
)

// 连接池类别(mmorpg_sql_pool_*{sql_client="db",pool=...},G2b);封闭集合,不含库名(落点库数不设上限)。
// deploy/k8s/mysql-connection-alerts.yaml 的 DbExtraStorePoolsBeyondPlan 按 extra_zone / extra_non_zone 两个取值写死,
// 改常量必须同改告警。
const (
	PoolHome         = "home"           // 本 zone 库(InitDB 打开,Database.MaxOpenConn)
	PoolExtraZone    = "extra_zone"     // 额外 zone 家族库 zone_{1..999999}_db(本 zone 库除外)
	PoolExtraNonZone = "extra_non_zone" // 非 zone 落点库 player_store_{>=1000000}_db
)

// NewSQLPoolCollector 只构造不注册(单测用自己的注册表)。sql_client 取本包的 subsystem("db")。
func NewSQLPoolCollector(source sqlpoolstats.Source) prometheus.Collector {
	return sqlpoolstats.NewCollector(subsystem, []string{PoolHome, PoolExtraZone, PoolExtraNonZone}, source)
}

// RegisterSQLPools 把连接池快照接到默认注册表(Start 暴露的那个)。进程内调一次;注册失败只记日志:
// 指标是观测面,不能挡住落库(与 Start 同口径)。MetricsListenAddr 留空时调用也无害(没人抓)。
func RegisterSQLPools(source sqlpoolstats.Source) {
	register()
	if err := prometheus.Register(NewSQLPoolCollector(source)); err != nil {
		logx.Errorf("[metrics] register sql pool collector failed (pool metrics unavailable): %v", err)
	}
}

// CountPlacementGuard 记一次选库 / 复核判定。op 为 "read" / "write",outcome 用上面的 Placement* 常量。
func CountPlacementGuard(op, outcome string) {
	register()
	placementGuardTotal.WithLabelValues(op, outcome).Inc()
}

// CountPlacementStoreOpen 记一次按需打开落点库的结果,result 用 StoreOpen* 常量。
func CountPlacementStoreOpen(result string) {
	register()
	placementStoreOpenTotal.WithLabelValues(result).Inc()
}

// SetPlacementOpenStores 更新常驻落点库个数。
func SetPlacementOpenStores(n int) {
	register()
	placementOpenStores.Set(float64(n))
}

// 归属 epoch 守卫的 outcome label 取值。与 internal/kafka 的守卫判定一一对应;
// 集合有界(6 个),不含 player_id 等高基数维度(AGENTS.md §9)。
const (
	OwnerEpochMatch      = "match"       // 任务 epoch == 已落库的最大 epoch:同一持有者的后续写,放行
	OwnerEpochLegacyZero = "legacy_zero" // 任务 epoch == 0:旧版生产者,兼容窗口内放行不比对
	OwnerEpochFirst      = "first"       // 该 key 还没有带 epoch 的写落过库,放行
	OwnerEpochAdvance    = "advance"     // 任务 epoch > 已落库的最大 epoch:归属变更后新主的第一笔写,放行
	OwnerEpochStale      = "stale"       // 任务 epoch < 已落库的最大 epoch:新主落库之后才到的老 epoch 写,拒绝
	OwnerEpochReadError  = "read_error"  // Redis 读失败:按可重试错误处理,不放行也不丢
)

// CountOwnerEpochGuard 记一次守卫判定。outcome 用上面的 OwnerEpoch* 常量。
func CountOwnerEpochGuard(outcome string) {
	register()
	ownerEpochGuardTotal.WithLabelValues(outcome).Inc()
}

// CountStaleOwnerWriteRejected 记一次「老 epoch 写被拒」。与 CountOwnerEpochGuard(OwnerEpochStale)
// 同时调用是有意的:前者是固定名字的健康红线,后者是全量分类。
func CountStaleOwnerWriteRejected() {
	register()
	staleOwnerWriteRejectedTotal.Inc()
}

// BlobObserver 把 dbguard 的量测结果接到 prometheus 上。
//
// label 基数是有界的:table 来自 mysql_database_table_list.json(9 张;player_snapshot /
// rollback_audit_log 只在全局库 data_service,已从 zone 清单摘掉),
// column 来自 proto 声明(每表 ≤ 10 列)—— 不含 player_id 之类的高基数维度
// (见仓库 CLAUDE.md §9)。
type BlobObserver struct{}

// ObserveBlobBytes 记录单列入库字节。
func (BlobObserver) ObserveBlobBytes(table, column string, bytes int64) {
	register()
	blobBytes.WithLabelValues(table, column).Observe(float64(bytes))
}

// ObserveRowBytes 记录整行 blob 入库字节之和。
func (BlobObserver) ObserveRowBytes(table string, bytes int64) {
	register()
	blobRowBytes.WithLabelValues(table).Observe(float64(bytes))
}

// CountGate 记录一次 warn / reject。
func (BlobObserver) CountGate(table, column, level string) {
	register()
	blobGuardTotal.WithLabelValues(table, column, level).Inc()
}

// ObserveStage records one sub-stage latency. Use one of the StageXxx
// constants for stage. op should be "read" / "write" / "unknown".
func ObserveStage(stage, op string, d time.Duration) {
	register()
	taskStageSeconds.WithLabelValues(stage, op).Observe(d.Seconds())
}

// ObserveResult records one terminal task outcome.
func ObserveResult(op, result string) {
	register()
	taskResultTotal.WithLabelValues(op, result).Inc()
}

// Stage labels — keep in sync with the call sites in
// internal/kafka/key_ordered_consumer.go.
const (
	StageOpTotal       = "op_total"
	StageOpHandler     = "op_handler"
	StageCacheWrite    = "cache_write"
	StageResultPublish = "result_publish"
	// 以下四个的含义见包注释;名字被 tools/scripts/stress_summarize.ps1 的 2d 段按精确 key 读取,改名同改脚本。
	StageSQL       = "sql"
	StageTaskTotal = "task_total"
	StageQueueWait = "queue_wait"
	StageKafkaE2E  = "kafka_e2e"
)

// Start exposes /metrics over HTTP. Failures log but do not block service
// startup — metrics are observability, not critical path. Empty addr
// disables the endpoint (production default; opt-in via config).
func Start(addr string) {
	if addr == "" {
		logx.Info("metrics endpoint disabled (MetricsListenAddr empty)")
		return
	}
	register()
	// pipeline.go 的指标走独立的 sync.Once;在暴露端点之前注册,告警要用的取值才会以 0 出现在第一次抓取里。
	registerPipeline()
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	srv := &http.Server{
		Addr:              addr,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	go func() {
		logx.Infof("db /metrics listening on %s", addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logx.Errorf("db /metrics ListenAndServe failed: %v", err)
		}
	}()
}
