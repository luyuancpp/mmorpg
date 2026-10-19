// Package metrics exposes Prometheus counters / histograms for data_service
// cross-server observability. The intent is to surface SavePlayerData /
// LoadPlayerData / per-player lock / rollback / cross-scene transition
// behavior so operators can tell at a glance whether the data plane is
// healthy.
//
// Style mirrors scene_manager/internal/metrics — Observe* helpers that lazy-
// register on first use. Keep label cardinality low: never label by player_id.
package metrics

import (
	"net/http"
	"strconv"
	"sync"
	"time"

	"shared/placement"
	"shared/sqlpoolstats"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/zeromicro/go-zero/core/logx"
)

const subsystem = "data_service"

var (
	// ── SavePlayerData / SetPlayerField — write path ──────────────
	savePlayerDataTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "save_player_data_total",
		Help:      "SavePlayerData outcomes (ok|version_mismatch|lock_conflict|redis_error).",
	}, []string{"outcome"})

	saveLatencySeconds = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "save_latency_seconds",
		Help:      "End-to-end SavePlayerData latency including lock acquire + version check + Redis pipeline.",
		Buckets:   prometheus.ExponentialBuckets(0.001, 2, 12), // 1ms .. ~4s
	}, []string{"outcome"})

	// ── Per-player lock — Layer 2 of consistency defense ──────────
	playerLockTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "player_lock_total",
		Help:      "Per-player lock outcomes (acquired|conflict|error).",
	}, []string{"outcome"})

	// ── Version mismatch — optimistic lock conflicts ──────────────
	versionMismatchTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "version_mismatch_total",
		Help:      "SavePlayerData / SetPlayerField optimistic-lock mismatches by op.",
	}, []string{"op"})

	// ── Cross-scene transition — Single Writer enforcement ────────
	// Reports the SceneManager-orchestrated old-Scene-release → new-Scene-load
	// sequence. data_service is the data plane that backs both steps; phase
	// labels record where time goes so we can spot Scene nodes that are
	// slow to flush vs slow to load.
	crossSceneTransitionLatency = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "cross_scene_transition_latency_seconds",
		Help:      "Per-phase cross-scene transition latency (release|save|load|total).",
		Buckets:   prometheus.ExponentialBuckets(0.005, 2, 12), // 5ms .. ~20s
	}, []string{"phase"})

	crossSceneTransitionTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "cross_scene_transition_total",
		Help:      "Cross-scene transition outcomes (ok|release_timeout|save_failed|load_failed|aborted).",
	}, []string{"outcome"})

	// ── Rollback — snapshot / zone / full-server ──────────────────
	rollbackTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "rollback_total",
		Help:      "Rollback attempts by scope (player|zone|server) and outcome (ok|partial|failed).",
	}, []string{"scope", "outcome"})

	// rollbackPlayersAffected accumulates the count of players touched so
	// operators can dashboard "how big was this rollback" without scraping logs.
	rollbackPlayersAffectedTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "rollback_players_affected_total",
		Help:      "Total players whose data was restored by a rollback, by scope.",
	}, []string{"scope"})

	rollbackOrphansCleanedTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "rollback_orphans_cleaned_total",
		Help:      "Orphan characters (created after target_time) cleaned during zone/server rollback.",
	}, []string{"scope"})

	// ── Kafka 落库消费者(transaction_log / player_snapshot)──────
	// up=0 且进程还活着 = 消费者因 DB 故障停下(宁可积压不丢审计),必须告警:
	// 积压超过 topic 保留期(默认 30 天)就真丢了。
	kafkaConsumerUp = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_up",
		Help:      "1 while the consumer loop is running, 0 once it stopped (DB failure or shutdown).",
	}, []string{"consumer"})

	kafkaConsumerMessagesTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_messages_total",
		Help:      "Consumed messages by consumer (transaction_log|player_snapshot) and outcome (inserted|duplicate|decode_error|invalid|oversize|db_error).",
	}, []string{"consumer", "outcome"})

	// ── 审计管道观测(G2a)────────────────────────────────────────
	// label 只有 consumer / result,全部是封闭集合;**不要**按分区或 player_id 出序列(AGENTS.md §9)。
	// 告警规则见 deploy/k8s/data-service-audit-alerts.yaml。

	// 单次 DB 尝试的耗时:拿到落库名额之后到 sink 返回,不含名额排队与重试退避;ctx 取消导致的失败不计。
	// 所以它慢 = 库慢(全局库开始排队),不会被本实例自己的并发上限或一次重试带偏。
	kafkaConsumerFlushSeconds = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_flush_seconds",
		Help:      "Latency of one database attempt of a consumer flush (after acquiring the flush slot; excludes slot wait and retry backoff), by consumer and result (ok|error).",
		Buckets:   []float64{0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, 30},
	}, []string{"consumer", "result"})

	// 每次 flush 的有效行数(纯坏消息的批不计,免得 0 拉低均值)。
	kafkaConsumerBatchRows = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_batch_rows",
		Help:      "Rows written per consumer flush (flushes carrying only skipped messages are not observed).",
		Buckets:   []float64{1, 2, 4, 8, 16, 32, 64, 128, 256},
	}, []string{"consumer"})

	// 每条消息 value 的字节数(含坏消息)。1000000 那一档对着 C++ 生产者 librdkafka 默认的 message.max.bytes:
	// 超过它的快照在生产端就被拒了,这里看不到 —— 所以 p99 贴近这一档就该查 scene 日志的 "Kafka send failed"。
	kafkaConsumerMessageBytes = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_message_bytes",
		Help:      "Size of each consumed message value in bytes (bad messages included).",
		Buckets:   []float64{1 << 10, 16 << 10, 64 << 10, 128 << 10, 256 << 10, 512 << 10, 768 << 10, 1000000, 2 << 20, 4 << 20, 8 << 20, 16 << 20},
	}, []string{"consumer"})

	// broker 上这个消费组在这个 topic 上还差多少条(全部分区求和)。每个实例报同一个数,告警取 max。
	kafkaConsumerLagMessages = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_lag_messages",
		Help:      "Consumer group lag summed over all partitions, as seen by the broker (every replica reports the same group value; alert on max).",
	}, []string{"consumer"})

	// 同一个组在这个 topic 上**单个分区**的最大积压。broker 的 retention.bytes 按分区执行:积压集中在少数分区时
	// (持有它的实例卡住 / 毒丸行卡住所在分区)合计值还远没到线,那个分区已经开始被删 —— 「离删除线还有多远」的告警比的是它,
	// 阈值因此与分区数无关。每个 consumer 一条序列,基数恒定;**不是**按分区出序列。每个实例报同一个数,告警取 max。
	kafkaConsumerLagMaxPartitionMessages = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_lag_max_partition_messages",
		Help:      "Largest single-partition lag of the consumer group on its topic, as seen by the broker (retention.bytes is enforced per partition; every replica reports the same group value; alert on max).",
	}, []string{"consumer"})

	// 保留期已经删掉、而这个组还没消费到的消息条数:> 0 = 审计数据已经丢了。
	// 消费者越过缺口并提交之后它会自行归零 —— 「丢了多少」以日志为准(kafka-go v0.4.47,已按源码核对,见 kafka.newReader 的说明):
	// 运行中被保留期追上是 [kafka-go] 的 `skipping from offset X to Y (N messages)` 行(精确);reader (重新)初始化时
	// (进程重启 / 停机冷却后重建 / rebalance / 断线重连)kafka-go 静默跳到日志起点,看 [kafka-go] 的 `starting at offset` 与
	// `seeking to offset` 两行之差;lag 探针的 `AUDIT DATA LOST` 行是 30s 采样值,只是下界。
	kafkaConsumerRetentionGapMessages = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_retention_gap_messages",
		Help:      "Messages deleted by topic retention before this consumer group consumed them (sum over partitions of log-start minus committed). > 0 means audit data is lost.",
	}, []string{"consumer"})

	kafkaConsumerLagProbeTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_lag_probe_total",
		Help:      "Consumer group lag probe outcomes by consumer and result (ok|error).",
	}, []string{"consumer", "result"})

	kafkaConsumerIsolationTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "kafka_consumer_isolation_total",
		Help:      "Row-by-row fallback after a failed batch insert, by consumer and result (recovered: every row landed | stopped: a row could not be inserted and the consumer stopped).",
	}, []string{"consumer", "result"})

	// ── AllocateIdSegment(号段发号)───────────────────────────────
	idSegmentAllocateTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "id_segment_allocate_total",
		Help:      "AllocateIdSegment outcomes by biz_tag (ok|invalid|exhausted|unknown_tag|db_error).",
	}, []string{"biz_tag", "outcome"})

	// ── 玩家名字注册表(设计 docs/design/guild-phase2/03-names.md §3.6)────
	//
	// 指标名在设计里写死成 data_service_player_name_*;Subsystem="data_service" 加
	// 下面的 Name 拼出来正是那三个名字,不需要也不应该再写一遍前缀。
	//
	// 【为什么不放在 logic 里用 go-zero 的 core/metric】go-zero 的 metric.Inc /
	// ObserveFloat 内部都先问 prometheus.Enabled(),那个开关只有 metric agent
	// (yaml 里的 Prometheus 段 → StartAgent)才会打开。data_service 没有那一段,
	// 也不该加——加了会再起一个 /metrics 端口,与 MetricsListenAddr 上已有的这套并存。
	// 用 core/metric 在本服务里的实际效果是:指标永远是 0,而且不报任何错。
	playerNameOpsTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "player_name_ops_total",
		Help: "Player name registry outcomes by op (reserve|release|batch_get) and result " +
			"(ok|already_owned|taken|invalid|conflict|outside_window|error). already_owned is the " +
			"idempotent reserve retry: a rising rate means callers are retrying CreatePlayer.",
	}, []string{"op", "result"})

	// 桶按"一次本地 SQL"的量级铺:1ms 是理想值,25ms 以内算健康,超过 250ms 说明
	// 全局库开始排队(建角是同步路径,reserve 的 p99 直接进 CreatePlayer 的耗时预算)。
	playerNameOpSeconds = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "player_name_op_seconds",
		Help:      "Player name registry op latency in seconds, cache + SQL included.",
		Buckets:   []float64{0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1},
	}, []string{"op"})

	playerNameCacheTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "player_name_cache_total",
		Help: "BatchGetPlayerName cache lookups counted per id: hit (name cached) | " +
			"negative_hit (cache remembers the row is absent) | miss (falls through to MySQL).",
	}, []string{"result"})

	// ── 资产通道:已落盘账本读取(docs/design/guild-phase2/07-rollback-fail-closed.md §7.9.1)──
	//
	// 调用方是 guild 重投循环的离线读账本(LedgerReadMinAttempts 之后才读),量很小。
	// absent 持续偏高 = 被问到的玩家在 zone Redis 里没有 blob(从未入场或 Redis 丢数据);
	// error 非零 = 路由 / Redis / blob 矛盾,详情看同一时刻的 ERROR 日志。
	assetOpLedgerReadTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "asset_op_ledger_read_total",
		Help:      "GetPlayerAssetOpLedger outcomes: found (blob read, ledger returned) | absent (no PlayerAllData blob) | error (routing, Redis or corrupt blob).",
	}, []string{"result"})

	// ── 玩家收件箱:已落盘账本读取(docs/design/player-inbox-service.md §9.3)──
	//
	// 调用方是 go/player_inbox 的对账(领导者每小时一轮 + 运维 ReconcilePlayer),GM 域量很小。
	// result 集合与上面的资产账本相同;absent 由对账计为 unknown、不告警;error 非零看同一时刻的 ERROR 日志。
	playerInboxLedgerReadTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "player_inbox_ledger_read_total",
		Help:      "GetPlayerInboxLedger outcomes: found (blob read, ledger returned) | absent (no PlayerAllData blob) | error (routing, Redis or corrupt blob).",
	}, []string{"result"})

	// ── 回档的帮会资产闸(docs/design/guild-phase2/07-rollback-fail-closed.md §7.9.1)──
	//
	// 告警口径(07 §7.9.2):divergent_accepted → 通知(有人放行了,必须有人认领补偿单);
	// post_write_* → 紧急;unavailable|budget → 警告;retention → 通知;divergent_rejected 不告警(闸在正常工作)。
	// 这些都是"进程一辈子可能只发生一次"的事件,所以序列必须在启动时预置为 0(PrimeRollbackGuildCheck),
	// 否则 increase(...) > 0 恰好漏掉第一次。
	rollbackGuildCheckTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "rollback_guild_check_total",
		Help: "Rollback guild-asset gate outcomes by scope (player|zone|server) and result " +
			"(clean|divergent_rejected|divergent_accepted|unavailable|retention|truncated|budget|post_write_divergent|post_write_recheck_failed). " +
			"The check phase counts exactly one result per rollback RPC; a post-write problem adds one post_write_* on top.",
	}, []string{"scope", "result"})

	rollbackGuildDivergenceRows = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "rollback_guild_divergence_rows",
		Help:      "Guild asset divergence rows seen by the rollback gate (after per-player since filtering), by scope and whether the rollback was accepted anyway.",
	}, []string{"scope", "accepted"})

	// 桶按"一次检查翻完所有块所有页"的量级铺:零命中的单人回档是一两次 RPC(几十毫秒),
	// 全服回档是成百上千次串行 RPC;上限对齐默认检查预算 120s。
	rollbackGuildCheckSeconds = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "rollback_guild_check_seconds",
		Help:      "Wall time of one rollback guild check (all chunks and pages, excluding the settle wait).",
		Buckets:   []float64{0.01, 0.05, 0.1, 0.5, 1, 5, 15, 30, 60, 120},
	}, []string{"scope"})

	// ── 玩家归属映射的提交点(docs/design/player-route-mapping-durability-proposal.md §4,第 1a 批)──
	//
	// 只在 RouteStore.Mode=db_first 时有数(redis 模式的 RegisterPlayerZone 不计)。两个 label 都是封闭集合,
	// db_first 启动时由 PrimeRoute 全部预置为 0。**不要**把 player_id / rev 加成 label(AGENTS.md §9),
	// 逐条明细在 `[RegisterPlayerZone][db_first]` 的 ERROR 日志里。
	//
	// 每一次 db_first 的 W1 恰好计一次,合计 = db_first 下 W1 的请求数。Redis 预检本身出错(围栏 EXISTS / 三键 MGET
	// 失败、MGET 长度不对、缓存里的 zone 解析不了)单独计 precheck_error:RegisterPlayerZone 的 error_code 只在 gRPC
	// status 的 message 里,grpcstats 只分得出 Unavailable、分不出码 1 与码 30,不计的话这类拒绝在指标上就看不见。
	routeRegisterTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "route_register_total",
		Help: "db_first RegisterPlayerZone outcomes, exactly one per request: inserted|filled_home|filled_home_placement_kept|already_same " +
			"(committed in player_route, published separately, see route_publish_total) | conflict|fenced|refused_unknown_row " +
			"(decided by player_route) | precheck_conflict|precheck_fenced (decided by the mapping Redis precheck, store not called) | " +
			"precheck_error (the mapping Redis precheck itself failed: EXISTS/MGET error or an unparsable cached zone; store not called) | " +
			"store_error|invalid.",
	}, []string{"outcome"})

	// 发布(set-if-newer 脚本 + WAITAOF)每次 publishRoute 调用的最终结果。not_durable 持续非零 = WAITAOF 预算不够
	// (appendfsync everysec 下本地确认最长约 1s),每一次都在库里留下一行已提交未确认的映射,见提案 §11.2 的 P2 门禁。
	// WAITAOF 回复形状不对(不是两个非负整数)是确定性错误、不重试,计在 error 而不是 not_durable:
	// P2 门禁量「发布未确认率」时要把 error 一并看(提案 §11.2)。
	routePublishTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "route_publish_total",
		Help: "db_first route publish results per publishRoute call: applied|confirmed|superseded (success) | " +
			"conflict|legacy_conflict|fenced|not_durable|error (failure; the player_route row stays committed but unpublished).",
	}, []string{"result"})

	// ── 第 3 批读方门禁(docs/handoff/launch-readiness-20261013/spec_D1_batch1b2_2_3.md §5.2)──
	//
	// RouteStore.ReadRequiresReady=true 时 R2(home_fence)/ R6(home,含 ClientForPlayer)/ R3(batch,按块计)每次读计一次,
	// 结果是 cache(副本可信,用了缓存,含权威缺席)| sql(回源库读到了)| not_found_sql(回源库,权威缺席)| sql_error(回源失败,
	// RPC 回 Unavailable)。enumerate(R7)在 db_first 下一律读库,与开关无关,只有 sql / sql_error。两个 label 都是封闭集合,
	// 开关打开时 PrimeRouteRead 全部预置为 0;sql_error 持续非零 = 缓存不可信期间全局库也不可用,读方整体 fail-closed。
	// **不要**把 player_id 加成 label(AGENTS.md §9),逐条明细在 `[RouteRead]` 日志里。
	routeReadTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "route_read_total",
		Help: "Route read gate results (RouteStore.ReadRequiresReady): path home|home_fence|batch (per chunk)|enumerate, " +
			"result cache (trusted replica used, including authoritative absence) | sql (fell back to player_route and found the row) | " +
			"not_found_sql (fell back, authoritative absence) | sql_error (fell back and failed; the RPC answers Unavailable).",
	}, []string{"path", "result"})

	// 读方每执行一次就绪读脚本记一次 verdict(placement.RouteReadyVerdict 的低基数名字),外加 script_error:
	// 脚本没执行成功或回复形状不对(按不可信处理、回源库)。trusted 以外的取值持续出现 = 读方正在回源。
	// P5 的门禁看对账 leader 的自检 gauge,不看这个计数(开关打开前读方不执行脚本,规格 §13.1 第 8 条)。
	routeReadyVerdictTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "route_ready_verdict_total",
		Help: "Route ready read script verdicts seen by data_service readers: trusted|absent|malformed|newer_version|" +
			"identity_unavailable|not_master|identity_mismatch, or script_error (the script failed or replied in an unexpected shape).",
	}, []string{"verdict"})

	registerOnce sync.Once
)

// route_register_total{outcome} 的取值(封闭集合,PrimeRoute 逐个预置;新增取值必须同时加进 routeRegisterOutcomes)。
const (
	RouteRegisterInserted                = "inserted"
	RouteRegisterFilledHome              = "filled_home"
	RouteRegisterFilledHomePlacementKept = "filled_home_placement_kept"
	RouteRegisterAlreadySame             = "already_same"
	RouteRegisterConflict                = "conflict"
	RouteRegisterFenced                  = "fenced"
	RouteRegisterRefusedUnknownRow       = "refused_unknown_row"
	RouteRegisterPrecheckConflict        = "precheck_conflict"
	RouteRegisterPrecheckFenced          = "precheck_fenced"
	RouteRegisterPrecheckError           = "precheck_error"
	RouteRegisterStoreError              = "store_error"
	RouteRegisterInvalid                 = "invalid"
)

// route_publish_total{result} 的取值(封闭集合,同上)。
const (
	RoutePublishApplied        = "applied"
	RoutePublishConfirmed      = "confirmed"
	RoutePublishSuperseded     = "superseded"
	RoutePublishConflict       = "conflict"
	RoutePublishLegacyConflict = "legacy_conflict"
	RoutePublishFenced         = "fenced"
	RoutePublishNotDurable     = "not_durable"
	RoutePublishError          = "error"
)

var (
	routeRegisterOutcomes = []string{
		RouteRegisterInserted,
		RouteRegisterFilledHome,
		RouteRegisterFilledHomePlacementKept,
		RouteRegisterAlreadySame,
		RouteRegisterConflict,
		RouteRegisterFenced,
		RouteRegisterRefusedUnknownRow,
		RouteRegisterPrecheckConflict,
		RouteRegisterPrecheckFenced,
		RouteRegisterPrecheckError,
		RouteRegisterStoreError,
		RouteRegisterInvalid,
	}

	routePublishResults = []string{
		RoutePublishApplied,
		RoutePublishConfirmed,
		RoutePublishSuperseded,
		RoutePublishConflict,
		RoutePublishLegacyConflict,
		RoutePublishFenced,
		RoutePublishNotDurable,
		RoutePublishError,
	}
)

// 回档帮会闸指标的 label 取值(封闭集合,PrimeRollbackGuildCheck 逐个预置;新增取值必须同时加进下面两个切片)。
const (
	RollbackGuildScopePlayer = "player"
	RollbackGuildScopeZone   = "zone"
	RollbackGuildScopeServer = "server"

	RollbackGuildResultClean                  = "clean"
	RollbackGuildResultDivergentRejected      = "divergent_rejected"
	RollbackGuildResultDivergentAccepted      = "divergent_accepted"
	RollbackGuildResultUnavailable            = "unavailable"
	RollbackGuildResultRetention              = "retention"
	RollbackGuildResultTruncated              = "truncated"
	RollbackGuildResultBudget                 = "budget"
	RollbackGuildResultPostWriteDivergent     = "post_write_divergent"
	RollbackGuildResultPostWriteRecheckFailed = "post_write_recheck_failed"
)

var (
	rollbackGuildScopes = []string{RollbackGuildScopePlayer, RollbackGuildScopeZone, RollbackGuildScopeServer}

	rollbackGuildResults = []string{
		RollbackGuildResultClean,
		RollbackGuildResultDivergentRejected,
		RollbackGuildResultDivergentAccepted,
		RollbackGuildResultUnavailable,
		RollbackGuildResultRetention,
		RollbackGuildResultTruncated,
		RollbackGuildResultBudget,
		RollbackGuildResultPostWriteDivergent,
		RollbackGuildResultPostWriteRecheckFailed,
	}
)

func register() {
	registerOnce.Do(func() {
		prometheus.MustRegister(
			savePlayerDataTotal, saveLatencySeconds,
			playerLockTotal,
			versionMismatchTotal,
			crossSceneTransitionLatency, crossSceneTransitionTotal,
			rollbackTotal, rollbackPlayersAffectedTotal, rollbackOrphansCleanedTotal,
			kafkaConsumerUp, kafkaConsumerMessagesTotal,
			kafkaConsumerFlushSeconds, kafkaConsumerBatchRows, kafkaConsumerMessageBytes,
			kafkaConsumerLagMessages, kafkaConsumerLagMaxPartitionMessages, kafkaConsumerRetentionGapMessages,
			kafkaConsumerLagProbeTotal, kafkaConsumerIsolationTotal,
			idSegmentAllocateTotal,
			playerNameOpsTotal, playerNameOpSeconds, playerNameCacheTotal,
			assetOpLedgerReadTotal,
			playerInboxLedgerReadTotal,
			rollbackGuildCheckTotal, rollbackGuildDivergenceRows, rollbackGuildCheckSeconds,
			routeRegisterTotal, routePublishTotal,
			routeReadTotal, routeReadyVerdictTotal,
		)
	})
}

// PrimeRoute 把 route_register_total(11 条)与 route_publish_total(8 条)的封闭集合逐个预置为 0。
// 只在 RouteStore.Mode=db_first 时调(data_service.go 启动路径):理由同 PrimeRollbackGuildCheck ——
// fenced / refused_unknown_row / legacy_conflict 这类一辈子可能只发生几次的结局,序列不预先存在的话
// `increase(...) > 0` 恰好漏掉第一次。redis 模式不调,免得面板上多出一组恒为 0、看着像"在跑"的序列。
func PrimeRoute() {
	register()
	for _, outcome := range routeRegisterOutcomes {
		routeRegisterTotal.WithLabelValues(outcome).Add(0)
	}
	for _, result := range routePublishResults {
		routePublishTotal.WithLabelValues(result).Add(0)
	}
}

// ObserveRouteRegister 记一次 db_first RegisterPlayerZone 的结局。outcome 取 RouteRegister* 常量(封闭集合),本函数不校验。
func ObserveRouteRegister(outcome string) {
	register()
	routeRegisterTotal.WithLabelValues(outcome).Inc()
}

// ObserveRoutePublish 记一次发布(publishRoute 一次调用,含其内部重试)的最终结果。result 取 RoutePublish* 常量。
func ObserveRoutePublish(result string) {
	register()
	routePublishTotal.WithLabelValues(result).Inc()
}

// route_read_total{path,result} 与 route_ready_verdict_total{verdict} 的取值(封闭集合,PrimeRouteRead 逐个预置;
// 新增取值必须同时加进 routeReadSeries / routeReadyVerdicts)。
const (
	RouteReadPathHome      = "home"
	RouteReadPathHomeFence = "home_fence"
	RouteReadPathBatch     = "batch"
	RouteReadPathEnumerate = "enumerate"

	RouteReadResultCache       = "cache"
	RouteReadResultSQL         = "sql"
	RouteReadResultNotFoundSQL = "not_found_sql"
	RouteReadResultSQLError    = "sql_error"

	// RouteReadyVerdictScriptError 是 verdict 之外的一个取值:读脚本没执行成功或回复形状不对。
	RouteReadyVerdictScriptError = "script_error"
)

// routeReadSeries 是 route_read_total 里**会出现**的 path × result 组合(13 条):batch 没有逐 id 的缺席结论,
// enumerate 只读库。不出现的组合不预置,免得面板上多出恒为 0、看着像「在跑」的序列。
var routeReadSeries = [][2]string{
	{RouteReadPathHome, RouteReadResultCache},
	{RouteReadPathHome, RouteReadResultSQL},
	{RouteReadPathHome, RouteReadResultNotFoundSQL},
	{RouteReadPathHome, RouteReadResultSQLError},
	{RouteReadPathHomeFence, RouteReadResultCache},
	{RouteReadPathHomeFence, RouteReadResultSQL},
	{RouteReadPathHomeFence, RouteReadResultNotFoundSQL},
	{RouteReadPathHomeFence, RouteReadResultSQLError},
	{RouteReadPathBatch, RouteReadResultCache},
	{RouteReadPathBatch, RouteReadResultSQL},
	{RouteReadPathBatch, RouteReadResultSQLError},
	{RouteReadPathEnumerate, RouteReadResultSQL},
	{RouteReadPathEnumerate, RouteReadResultSQLError},
}

// routeReadyVerdicts 是 route_ready_verdict_total 的封闭集合:placement 的 7 个 verdict 名字(唯一出处,不在这里抄一份)
// 加 script_error,共 8 条。
func routeReadyVerdicts() []string {
	return append(placement.RouteReadyVerdictNames(), RouteReadyVerdictScriptError)
}

// PrimeRouteRead 把 route_read_total(13 条)与 route_ready_verdict_total(8 条)逐个预置为 0。
// 只在 RouteStore.ReadRequiresReady=true(必然 db_first)时调(svc.NewServiceContext):理由同 PrimeRoute ——
// sql_error 这类一辈子可能只发生几次的结局,序列不预先存在的话 `increase(...) > 0` 恰好漏掉第一次;
// 开关关着时不调,免得面板上多出一组恒为 0 的序列。
func PrimeRouteRead() {
	register()
	for _, s := range routeReadSeries {
		routeReadTotal.WithLabelValues(s[0], s[1]).Add(0)
	}
	for _, v := range routeReadyVerdicts() {
		routeReadyVerdictTotal.WithLabelValues(v).Add(0)
	}
}

// ObserveRouteRead 记一次读方门禁的读。path / result 取 RouteRead* 常量(封闭集合),本函数不校验。
func ObserveRouteRead(path, result string) {
	register()
	routeReadTotal.WithLabelValues(path, result).Inc()
}

// ObserveRouteReadyVerdict 记读方一次就绪读脚本的结论:placement.RouteReadyVerdict.String() 或 RouteReadyVerdictScriptError。
func ObserveRouteReadyVerdict(verdict string) {
	register()
	routeReadyVerdictTotal.WithLabelValues(verdict).Inc()
}

// PrimeRollbackGuildCheck 把 rollback_guild_check_total 的 scope × result 封闭集合(3 × 9 = 27 条)
// 逐个预置为 0。**必须在启动时调**(data_service.go 里紧跟 metrics.Start):
// CounterVec 的子序列在首次 WithLabelValues 之前不存在,`increase(...) > 0` 对"从无到 1"的序列
// 在窗口内只有一个样本,恰好漏掉第一次 —— 而放行回档与写后分歧正是一辈子可能只发生一次的事件
// (92-handoff:184-189 踩过的坑;同仓先例 SetKafkaConsumerUp 预注册 0)。
// 本包是直连 prometheus/client_golang 的懒注册,不受 go-zero core/metric 那个全局开关影响,
// 所以调用时机只要求"在第一次 scrape 之前",MetricsListenAddr 留空时调用也无害。
func PrimeRollbackGuildCheck() {
	register()
	for _, scope := range rollbackGuildScopes {
		for _, result := range rollbackGuildResults {
			rollbackGuildCheckTotal.WithLabelValues(scope, result).Add(0)
		}
	}
}

// ObserveRollbackGuildCheck 记一次回档帮会闸的结论。scope / result 取上面的常量(封闭集合),本函数不校验。
// 一次回档 RPC 的检查阶段只记一个 result;写后复查出问题时再额外记一个 post_write_*。
func ObserveRollbackGuildCheck(scope, result string) {
	register()
	rollbackGuildCheckTotal.WithLabelValues(scope, result).Inc()
}

// AddRollbackGuildDivergenceRows 累加检查阶段看到的分歧行数(过滤后)。n <= 0 直接返回。
// **不要**把 player_id / op_id 加成 label(AGENTS.md §9),逐行明细在 `[Rollback][GuildDivergence]` 日志里。
func AddRollbackGuildDivergenceRows(scope string, accepted bool, n int) {
	if n <= 0 {
		return
	}
	register()
	rollbackGuildDivergenceRows.WithLabelValues(scope, strconv.FormatBool(accepted)).Add(float64(n))
}

// ObserveRollbackGuildCheckSeconds 记一次检查(全部块、全部页,不含沉降等待)的耗时。
func ObserveRollbackGuildCheckSeconds(scope string, d time.Duration) {
	register()
	rollbackGuildCheckSeconds.WithLabelValues(scope).Observe(d.Seconds())
}

// 连接池类别(mmorpg_sql_pool_*{sql_client="data_service",pool=...},G2b):一个 store 一个独立连接池,类别即 store 名。
// 封闭集合;svc.ServiceContext.SQLPoolSamples 按这些常量报样本。
const (
	PoolSnapshot       = "snapshot"        // player_snapshot / rollback_audit_log(SnapshotMySQL.MaxOpenConn)
	PoolTransactionLog = "transaction_log" // transaction_log(SnapshotMySQL.MaxOpenConn)
	PoolIdSegment      = "id_segment"      // id_segment(SnapshotMySQL.MaxOpenConn)
	PoolPlayerName     = "player_name"     // player_name(PlayerName.MaxOpenConn)
	PoolPlayerRoute    = "player_route"    // 路由表(RouteStore.MaxOpenConn),只在 RouteStore.Mode=db_first 时装配
)

// NewSQLPoolCollector 只构造不注册(单测用自己的注册表)。sql_client 取本包的 subsystem("data_service")。
// includeRoute 为 false 时不声明 player_route:理由同 PrimeRoute —— redis 模式下不装配 route store,
// 不要一组恒为 0、看着像在跑的序列。
func NewSQLPoolCollector(source sqlpoolstats.Source, includeRoute bool) prometheus.Collector {
	pools := []string{PoolSnapshot, PoolTransactionLog, PoolIdSegment, PoolPlayerName}
	if includeRoute {
		pools = append(pools, PoolPlayerRoute)
	}
	return sqlpoolstats.NewCollector(subsystem, pools, source)
}

// RegisterSQLPools 把连接池快照接到默认注册表(Start 暴露的那个)。进程内调一次(data_service.go 启动路径;
// 不能放进 svc.NewServiceContext:单测会多次构造 ServiceContext,默认注册表重复注册会报错)。
// 注册失败只记日志:指标是观测面,不能挡住 Load/Save。
func RegisterSQLPools(source sqlpoolstats.Source, includeRoute bool) {
	register()
	if err := prometheus.Register(NewSQLPoolCollector(source, includeRoute)); err != nil {
		logx.Errorf("[metrics] register sql pool collector failed (pool metrics unavailable): %v", err)
	}
}

// SetKafkaConsumerUp flips the up gauge for one consumer ("transaction_log" | "player_snapshot").
func SetKafkaConsumerUp(consumer string, up bool) {
	register()
	v := 0.0
	if up {
		v = 1
	}
	kafkaConsumerUp.WithLabelValues(consumer).Set(v)
}

// ObserveKafkaConsumerMessages adds n messages with one outcome for one consumer.
func ObserveKafkaConsumerMessages(consumer, outcome string, n int) {
	if n <= 0 {
		return
	}
	register()
	kafkaConsumerMessagesTotal.WithLabelValues(consumer, outcome).Add(float64(n))
}

// kafka_consumer_* 审计管道指标(G2a)的封闭取值集合,PrimeKafkaConsumer 逐个预置;新增取值必须同时加进下面的切片。
const (
	KafkaResultOK    = "ok"
	KafkaResultError = "error"

	KafkaIsolationRecovered = "recovered"
	KafkaIsolationStopped   = "stopped"
)

var (
	// kafkaConsumerOutcomes 是 kafka_consumer_messages_total{outcome} 的全集(与它的 Help 一致)。
	kafkaConsumerOutcomes = []string{"inserted", "duplicate", "decode_error", "invalid", "oversize", "db_error"}
	kafkaLagProbeResults  = []string{KafkaResultOK, KafkaResultError}
	kafkaIsolationResults = []string{KafkaIsolationRecovered, KafkaIsolationStopped}
)

// PrimeKafkaConsumer 把一条消费者的 messages_total(6 个 outcome)、lag_probe_total(ok / error)、
// isolation_total(recovered / stopped)逐个预置为 0。理由同 PrimeRollbackGuildCheck:坏消息跳过、保留期丢失、
// 批量兜底停机这类一辈子可能只发生几次的结局,序列不预先存在的话 `increase(...) > 0` 恰好漏掉第一次。
// 启动路径(data_service.go startKafkaConsumers)对两条消费者各调一次;txlog 没有逐条兜底,它的 isolation 序列恒为 0,无害。
func PrimeKafkaConsumer(consumer string) {
	register()
	for _, outcome := range kafkaConsumerOutcomes {
		kafkaConsumerMessagesTotal.WithLabelValues(consumer, outcome).Add(0)
	}
	for _, result := range kafkaLagProbeResults {
		kafkaConsumerLagProbeTotal.WithLabelValues(consumer, result).Add(0)
	}
	for _, result := range kafkaIsolationResults {
		kafkaConsumerIsolationTotal.WithLabelValues(consumer, result).Add(0)
	}
}

// ObserveKafkaConsumerFlushAttempt 记一次 DB 尝试的耗时;result 取 KafkaResultOK / KafkaResultError。
// 调用方只计"拿到落库名额之后到 sink 返回"这一段,ctx 取消导致的失败不要调(关停 / 兄弟分区停机不是 DB 慢)。
func ObserveKafkaConsumerFlushAttempt(consumer, result string, d time.Duration) {
	register()
	kafkaConsumerFlushSeconds.WithLabelValues(consumer, result).Observe(d.Seconds())
}

// ObserveKafkaConsumerBatchRows 记一次 flush 写了几行。rows <= 0(纯坏消息的批)直接返回。
func ObserveKafkaConsumerBatchRows(consumer string, rows int) {
	if rows <= 0 {
		return
	}
	register()
	kafkaConsumerBatchRows.WithLabelValues(consumer).Observe(float64(rows))
}

// ObserveKafkaConsumerMessageBytes 记一条消息 value 的字节数。
func ObserveKafkaConsumerMessageBytes(consumer string, n int) {
	register()
	kafkaConsumerMessageBytes.WithLabelValues(consumer).Observe(float64(n))
}

// SetKafkaConsumerLag 写 lag 探针的结果:全部分区合计的积压条数、单分区最大积压条数、保留期已删掉的未消费条数。
func SetKafkaConsumerLag(consumer string, lag, maxPartitionLag, gap int64) {
	register()
	kafkaConsumerLagMessages.WithLabelValues(consumer).Set(float64(lag))
	kafkaConsumerLagMaxPartitionMessages.WithLabelValues(consumer).Set(float64(maxPartitionLag))
	kafkaConsumerRetentionGapMessages.WithLabelValues(consumer).Set(float64(gap))
}

// ObserveKafkaConsumerLagProbe 记一次 lag 探测的结局(KafkaResultOK / KafkaResultError)。
func ObserveKafkaConsumerLagProbe(consumer, result string) {
	register()
	kafkaConsumerLagProbeTotal.WithLabelValues(consumer, result).Inc()
}

// ObserveKafkaConsumerIsolation 记一次批量失败后逐条兜底的结局(KafkaIsolationRecovered / KafkaIsolationStopped)。
func ObserveKafkaConsumerIsolation(consumer, result string) {
	register()
	kafkaConsumerIsolationTotal.WithLabelValues(consumer, result).Inc()
}

// ObserveIdSegmentAllocate records one AllocateIdSegment outcome. biz_tag cardinality is
// bounded by the store's tag validation and the handful of real tags (player/guild/item).
func ObserveIdSegmentAllocate(bizTag, outcome string) {
	register()
	idSegmentAllocateTotal.WithLabelValues(bizTag, outcome).Inc()
}

// ObservePlayerNameOp 记一次玩家名字注册表操作:结果计数 + 耗时。
//
// op 是 "reserve" | "release" | "batch_get";result 取 Help 里那组有界取值。
// 两个 label 的取值集合由调用方(logic 的 playerNameOp* / playerNameResult* 常量)
// 封闭,本函数不做校验——它在每次操作的 defer 里跑,不是校验的地方。
//
// dur 由调用方从函数入口处的 time.Now() 算出,这样每一条出口(含提前 return 的
// 失败分支)都被计进直方图。**不要**把 player_id 加成 label(AGENTS.md §9:
// 高基数会把时间序列打爆),需要它的排障信息进日志。
func ObservePlayerNameOp(op, result string, dur time.Duration) {
	register()
	playerNameOpsTotal.WithLabelValues(op, result).Inc()
	playerNameOpSeconds.WithLabelValues(op).Observe(dur.Seconds())
}

// ObservePlayerNameCache 按 id 累计一次 BatchGet 的缓存命中情况。
// result 是 "hit" | "negative_hit" | "miss";n 是本批里落在该结果上的 id 个数
// (调用方先在循环里累加、退出循环后一次报,省掉逐 id 的 label 查找)。
// n <= 0 直接返回,让调用方三种结果都能无条件调用。
func ObservePlayerNameCache(result string, n int) {
	if n <= 0 {
		return
	}
	register()
	playerNameCacheTotal.WithLabelValues(result).Add(float64(n))
}

// ObserveAssetOpLedgerRead 记一次 GetPlayerAssetOpLedger 的结果。
// result 是 "found" | "absent" | "error",取值集合由调用方(logic 的 assetOpLedgerRead* 常量)封闭,
// 本函数不做校验。**不要**把 player_id 加成 label(AGENTS.md §9),排障信息进日志。
func ObserveAssetOpLedgerRead(result string) {
	register()
	assetOpLedgerReadTotal.WithLabelValues(result).Inc()
}

// ObservePlayerInboxLedgerRead 记一次 GetPlayerInboxLedger 的结果。
// result 是 "found" | "absent" | "error",取值集合由调用方(logic 的 inboxLedgerRead* 常量)封闭,
// 本函数不做校验。**不要**把 player_id 加成 label(AGENTS.md §9),排障信息进日志。
func ObservePlayerInboxLedgerRead(result string) {
	register()
	playerInboxLedgerReadTotal.WithLabelValues(result).Inc()
}

// ── Observe helpers ─────────────────────────────────────────────────

// ObserveSavePlayerData records one SavePlayerData / SetPlayerField outcome
// along with end-to-end latency. outcome must be one of
//
//	"ok" | "version_mismatch" | "lock_conflict" | "redis_error"
//
// Pass startTime = time.Now() captured at the very top of the RPC handler
// so the histogram captures every code path including failure exits.
func ObserveSavePlayerData(outcome string, startTime time.Time) {
	register()
	savePlayerDataTotal.WithLabelValues(outcome).Inc()
	saveLatencySeconds.WithLabelValues(outcome).Observe(time.Since(startTime).Seconds())
}

// ObservePlayerLock records one player-lock attempt. outcome is
// "acquired" | "conflict" | "error".
func ObservePlayerLock(outcome string) {
	register()
	playerLockTotal.WithLabelValues(outcome).Inc()
}

// ObserveVersionMismatch records one optimistic-lock collision. op is
// "save_player_data" | "set_player_field".
func ObserveVersionMismatch(op string) {
	register()
	versionMismatchTotal.WithLabelValues(op).Inc()
}

// ObserveCrossSceneTransition records latency for one phase of the
// SceneManager-orchestrated transition. phase is "release" | "save" |
// "load" | "total". Use a separate call per phase so dashboards can
// stack them.
func ObserveCrossSceneTransition(phase string, dur time.Duration) {
	register()
	crossSceneTransitionLatency.WithLabelValues(phase).Observe(dur.Seconds())
}

// ObserveCrossSceneTransitionOutcome records the final outcome of one
// transition attempt. outcome is "ok" | "release_timeout" | "save_failed"
// | "load_failed" | "aborted".
func ObserveCrossSceneTransitionOutcome(outcome string) {
	register()
	crossSceneTransitionTotal.WithLabelValues(outcome).Inc()
}

// ObserveRollback records one rollback attempt. scope is "player" |
// "zone" | "server"; outcome is "ok" | "partial" | "failed".
// affectedPlayers is the count of players whose data was actually
// restored (0 on failure / orphan-only sweeps). orphansCleaned is the
// count of orphan characters scrubbed (0 for player-scope rollback).
func ObserveRollback(scope, outcome string, affectedPlayers, orphansCleaned uint32) {
	register()
	rollbackTotal.WithLabelValues(scope, outcome).Inc()
	if affectedPlayers > 0 {
		rollbackPlayersAffectedTotal.WithLabelValues(scope).Add(float64(affectedPlayers))
	}
	if orphansCleaned > 0 {
		rollbackOrphansCleanedTotal.WithLabelValues(scope).Add(float64(orphansCleaned))
	}
}

// ── HTTP /metrics endpoint ──────────────────────────────────────────

// Start exposes /metrics over HTTP. Empty addr disables the endpoint.
// Mirrors scene_manager/internal/metrics.Start so operators can wire the
// same Prometheus scrape config against either service.
func Start(addr string) {
	if addr == "" {
		logx.Info("[metrics] MetricsListenAddr empty; Prometheus /metrics endpoint disabled")
		return
	}
	register()
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	srv := &http.Server{
		Addr:              addr,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	go func() {
		logx.Infof("[metrics] Prometheus /metrics listening on %s", addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logx.Errorf("[metrics] HTTP server exited: %v", err)
		}
	}()
}

// CommonLatencyHelper — convenience: returns a closure that observes a
// histogram with the elapsed time since the helper was created. Saves
// callers from having to capture startTime manually around early returns.
//
// Example:
//
//	defer metrics.WithSaveLatency("ok")() // observes 'ok' on normal exit
//	... if err != nil { defer metrics.WithSaveLatency("redis_error")() }
//
// (Use ObserveSavePlayerData directly if you want explicit control.)
func WithSaveLatency(outcome string) func() {
	register()
	start := time.Now()
	return func() {
		savePlayerDataTotal.WithLabelValues(outcome).Inc()
		saveLatencySeconds.WithLabelValues(outcome).Observe(time.Since(start).Seconds())
	}
}

// FormatZone is a tiny helper that callers can use when they need to
// expose a zone-labeled metric (none currently do — kept here so future
// observe helpers can stay consistent with scene_manager).
func FormatZone(zoneID uint32) string {
	return strconv.FormatUint(uint64(zoneID), 10)
}
