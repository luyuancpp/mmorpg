// Package metrics 是 player_inbox 的 Prometheus 指标(docs/design/player-inbox-service.md §9.4)。
//
// 前缀统一 player_inbox_;label 只用有限枚举(type / domain / fence / path / result / reason),
// **绝不用 player_id**(AGENTS §9:高基数会爆),player_id 只进日志。
//
// 计数器每个副本各报一份;扫描型 gauge 只由领导者产出(避免多副本重复上报被放大),
// 告警与看板一律用 max() 聚合。领导权丢失时 ResetScanGauges 把本副本的 gauge 归零,
// 免得卸任副本的旧值在 max() 里一直顶着。
//
// 指标对象在包初始化时建好,单测里没调 Register 也能正常 Inc;只有 Register 把它们挂进默认注册表。
package metrics

import (
	"database/sql"
	"errors"
	"net"
	"net/http"
	"sync"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/collectors"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/zeromicro/go-zero/core/logx"
)

const namespace = "player_inbox"

var (
	enqueueTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "enqueue_total",
		Help: "Enqueue attempts by message type and result (created|duplicate|conflict|full|disabled|unknown_type|invalid|payload_too_large|payload_invalid|text_too_long|out_of_window|player_not_found|error).",
	}, []string{"type", "result"})

	fetchTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "fetch_total",
		Help: "FetchItems calls by domain, fence verdict and path (fast = unlocked snapshot read, locked = stream row FOR UPDATE).",
	}, []string{"domain", "fence", "path"})

	ackTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "ack_total",
		Help: "AckItems per-decision results by domain (finalized|finalized_settled_unknown|already_same|conflict|unknown_seq|not_delivered|skip_not_requested|mismatch) or the fence verdict when the whole request was rejected.",
	}, []string{"domain", "result"})

	voidTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "void_total",
		Help: "Admin VoidItem by domain, reason and path (direct = never delivered → VOIDED, directive = VOID directive attached, already = directive already attached, force = delivered row finalized VOIDED after the persisted-ledger proof).",
	}, []string{"domain", "reason", "path"})

	notifyTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "notify_total",
		Help: "Online notify attempts by result (sent|offline|awaiting_placement|no_node|empty_uuid|error).",
	}, []string{"result"})

	cursorAheadTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Namespace: namespace, Name: "cursor_ahead_total",
		Help: "Fetch saw consumer ledger watermark >= stream next_seq (inbox DB restored); next_seq was raised. Critical.",
	})

	fenceRegressedTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Namespace: namespace, Name: "fence_regressed_total",
		Help: "STALE verdicts classified as owner_epoch counter regression in core Redis. Critical: run epoch-raise, never reset the fence.",
	})

	epochAheadTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Namespace: namespace, Name: "epoch_ahead_total",
		Help: "Requests whose owner_epoch exceeded the core Redis value (defect or forgery). Critical.",
	})

	// reconcileInconsistentRows 取代了原来的 reconcile_mismatch_total 计数器(评审 L11):同一处不一致只要还在
	// 24h 窗口里,每轮都会再被发现一次,累加进计数器会让 critical 告警挂满约 25 小时。它是「当前」读数,只由领导者产出。
	reconcileInconsistentRows = prometheus.NewGauge(prometheus.GaugeOpts{
		Namespace: namespace, Name: "reconcile_inconsistent_rows",
		Help: "Consumer-finalized rows in the reconcile window whose outcome the persisted ledger does not hold " +
			"(beyond the watermark, missing from the recent ring, or a different item/outcome at that seq), " +
			"as of the latest reconcile round (leader-only; aggregate with max()). A level, not an event count. Critical.",
	})

	reconcileTruncatedTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Namespace: namespace, Name: "reconcile_truncated_total",
		Help: "Reconcile rounds that did not cover the whole window (a shard hit the per-shard row limit or the round hit " +
			"the player limit); the ERROR log says where it stopped and how many shards were left.",
	})

	reconcileRoundsTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "reconcile_rounds_total",
		Help: "Reconcile rounds by result (ok|error|timeout), leader-only. Rounds cancelled by losing leadership or shutdown are not counted.",
	}, []string{"result"})

	reconcileReadTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "reconcile_ledger_read_total",
		Help: "Reconcile ledger reads by result (found|absent|error|invalid); absent = unknown, not alerted; " +
			"invalid = structurally broken ledger, cannot be used as evidence.",
	}, []string{"result"})

	sweepDeletedTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Namespace: namespace, Name: "sweep_deleted_total",
		Help: "Terminal rows deleted after the retention window.",
	})

	// fetchScannedRows 是设计 §4.2 量级假设的触发指标:锁路径的 PENDING 扫描走「主键范围 + state 过滤」,
	// 数的是数据库实际走过的行(含被过滤掉的终态行),不是返回的条数(返回最多 17 条,数它永远触发不了加索引)。
	fetchScannedRows = prometheus.NewHistogram(prometheus.HistogramOpts{
		Namespace: namespace, Name: "fetch_scanned_rows",
		Help:    "Item rows walked by the locked Fetch PENDING scan (primary-key range up to the last row read, terminal rows included); sustained values in the hundreds mean the (shard,player_id,domain,state,seq) index is due.",
		Buckets: []float64{0, 1, 2, 4, 8, 17, 32, 64, 128, 256, 512, 1024},
	})

	deliveredUnacked = prometheus.NewGauge(prometheus.GaugeOpts{
		Namespace: namespace, Name: "delivered_unacked",
		Help: "PENDING items delivered at least once (leader-only scan; aggregate with max()).",
	})
	deliveredUnackedOldestSeconds = prometheus.NewGauge(prometheus.GaugeOpts{
		Namespace: namespace, Name: "delivered_unacked_oldest_seconds",
		Help: "Age since last delivery of the oldest delivered-but-unacked item (leader-only; the real 'stuck' signal).",
	})
	openItems = prometheus.NewGauge(prometheus.GaugeOpts{
		Namespace: namespace, Name: "open_items",
		Help: "PENDING items (leader-only scan; info dashboard only, offline players never drain).",
	})
	oldestOpenAgeSeconds = prometheus.NewGauge(prometheus.GaugeOpts{
		Namespace: namespace, Name: "oldest_open_age_seconds",
		Help: "Age of the oldest PENDING item (leader-only scan; info dashboard only).",
	})
	maxDeliveryCount = prometheus.NewGauge(prometheus.GaugeOpts{
		Namespace: namespace, Name: "max_delivery_count",
		Help: "Highest delivery_count among PENDING items (leader-only; >= 5 suggests a poison message).",
	})
	notifyBacklog = prometheus.NewGauge(prometheus.GaugeOpts{
		Namespace: namespace, Name: "notify_backlog",
		Help: "Streams whose online notify has been due for more than 5 minutes (leader-only scan).",
	})

	registerOnce sync.Once
)

// ObserveEnqueue 记一次入箱结局。
func ObserveEnqueue(typeLabel, result string) { enqueueTotal.WithLabelValues(typeLabel, result).Inc() }

// ObserveFetch 记一次 Fetch。
func ObserveFetch(domain, fence, path string) { fetchTotal.WithLabelValues(domain, fence, path).Inc() }

// ObserveFetchScanned 记锁路径一次读出的行数。
func ObserveFetchScanned(rows int) { fetchScannedRows.Observe(float64(rows)) }

// ObserveAck 记一条 Ack 结果(或整次请求的围栏拒绝)。
func ObserveAck(domain, result string) { ackTotal.WithLabelValues(domain, result).Inc() }

// ObserveVoid 记一次运维作废。
func ObserveVoid(domain, reason, path string) { voidTotal.WithLabelValues(domain, reason, path).Inc() }

// ObserveNotify 记一次在线提醒尝试。
func ObserveNotify(result string) { notifyTotal.WithLabelValues(result).Inc() }

// IncCursorAhead / IncFenceRegressed / IncEpochAhead 是 critical 级计数。
func IncCursorAhead()    { cursorAheadTotal.Inc() }
func IncFenceRegressed() { fenceRegressedTotal.Inc() }
func IncEpochAhead()     { epochAheadTotal.Inc() }

// ObserveReconcileRead 记一次对账读账本的结果。
func ObserveReconcileRead(result string) { reconcileReadTotal.WithLabelValues(result).Inc() }

// reconcileRoundResults 是 reconcile_rounds_total{result} 的全部取值(Register 预建零值序列)。
var reconcileRoundResults = []string{"ok", "error", "timeout"}

// ObserveReconcileRound 记一轮对账的结果(ok|error|timeout)。
func ObserveReconcileRound(result string) { reconcileRoundsTotal.WithLabelValues(result).Inc() }

// IncReconcileTruncated 记一轮没有覆盖完整个窗口的对账。
func IncReconcileTruncated() { reconcileTruncatedTotal.Inc() }

// SetReconcileInconsistent 写入领导者最近一轮对账发现的不一致行数(critical 告警按 max() 取它)。
func SetReconcileInconsistent(n int) { reconcileInconsistentRows.Set(float64(max(n, 0))) }

// ResetReconcileInconsistent 在卸任时把本副本的不一致行数归零,理由同 ResetScanGauges。
func ResetReconcileInconsistent() { reconcileInconsistentRows.Set(0) }

// AddSweepDeleted 记清理删掉的行数。
func AddSweepDeleted(n int) {
	if n > 0 {
		sweepDeletedTotal.Add(float64(n))
	}
}

// ScanGauges 是一次扫描型 gauge 的结果。
type ScanGauges struct {
	DeliveredUnacked              uint64
	DeliveredUnackedOldestSeconds float64
	OpenItems                     uint64
	OldestOpenAgeSeconds          float64
	MaxDeliveryCount              uint64
	NotifyBacklog                 uint64
}

// SetScanGauges 写入领导者的一次扫描结果。
func SetScanGauges(g ScanGauges) {
	deliveredUnacked.Set(float64(g.DeliveredUnacked))
	deliveredUnackedOldestSeconds.Set(g.DeliveredUnackedOldestSeconds)
	openItems.Set(float64(g.OpenItems))
	oldestOpenAgeSeconds.Set(g.OldestOpenAgeSeconds)
	maxDeliveryCount.Set(float64(g.MaxDeliveryCount))
	notifyBacklog.Set(float64(g.NotifyBacklog))
}

// ResetScanGauges 在卸任时把本副本的扫描型 gauge 归零。
func ResetScanGauges() { SetScanGauges(ScanGauges{}) }

// Register 把全部指标与连接池指标挂进默认注册表(只执行一次)。db 可为 nil(不出池指标)。
func Register(db *sql.DB) {
	registerOnce.Do(func() {
		prometheus.MustRegister(
			enqueueTotal, fetchTotal, ackTotal, voidTotal, notifyTotal,
			cursorAheadTotal, fenceRegressedTotal, epochAheadTotal,
			reconcileInconsistentRows, reconcileTruncatedTotal, reconcileRoundsTotal, reconcileReadTotal,
			sweepDeletedTotal, fetchScannedRows,
			deliveredUnacked, deliveredUnackedOldestSeconds, openItems, oldestOpenAgeSeconds, maxDeliveryCount, notifyBacklog,
		)
		if db != nil {
			// 连接池单独出池指标(设计 §9.4):go_sql_* 带 db_name="player_inbox"。
			prometheus.MustRegister(collectors.NewDBStatsCollector(db, namespace))
		}
		// 预建 critical 计数器的零值序列:否则「从未发生」与「指标不存在」在告警规则里长得一样。
		for _, r := range []string{"sent", "offline", "awaiting_placement", "no_node", "empty_uuid", "error"} {
			notifyTotal.WithLabelValues(r)
		}
		for _, r := range []string{"found", "absent", "error", "invalid"} {
			reconcileReadTotal.WithLabelValues(r)
		}
		for _, r := range reconcileRoundResults {
			reconcileRoundsTotal.WithLabelValues(r)
		}
	})
}

// StartHTTP 启动 Prometheus /metrics 端点;addr 为空则关闭(与 trade 同模式)。返回的 stop 关闭监听,可重复调用。
// 监听失败(端口被占、地址非法)只记错误、进程照常服务(没有 /metrics 不该拖垮收件箱),返回空操作的 stop。
func StartHTTP(addr string) (stop func()) {
	stop, _ = startHTTP(addr)
	return stop
}

// startHTTP 是 StartHTTP 的实现,另返回实际监听地址(addr 写 ":0" 时由内核分配端口;单测靠它拨号)。
// 先同步监听再在后台 Serve:监听失败当场可见,不必等后台 goroutine 里的 ListenAndServe 报错。
func startHTTP(addr string) (stop func(), bound string) {
	if addr == "" {
		logx.Info("[player_inbox] MetricsListenAddr empty; Prometheus /metrics endpoint disabled")
		return func() {}, ""
	}
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		logx.Errorf("[player_inbox] metrics HTTP listen on %s failed; continuing without /metrics: %v", addr, err)
		return func() {}, ""
	}
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	srv := &http.Server{
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	bound = ln.Addr().String()
	logx.Infof("[player_inbox] Prometheus /metrics listening on %s", bound)
	go func() {
		if err := srv.Serve(ln); err != nil && !errors.Is(err, http.ErrServerClosed) {
			logx.Errorf("[player_inbox] metrics HTTP server exited: %v", err)
		}
	}()
	return func() { _ = srv.Close() }, bound
}
