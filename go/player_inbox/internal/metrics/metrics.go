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
		Help: "Admin VoidItem by domain, reason and path (direct = never delivered → VOIDED, directive = VOID directive attached, already = directive already attached).",
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

	reconcileMismatchTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Namespace: namespace, Name: "reconcile_mismatch_total",
		Help: "Reconcile found a consumer-finalized row beyond the persisted ledger watermark. Critical.",
	})

	reconcileReadTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: namespace, Name: "reconcile_ledger_read_total",
		Help: "Reconcile ledger reads by result (found|absent|error); absent = unknown, not alerted.",
	}, []string{"result"})

	sweepDeletedTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Namespace: namespace, Name: "sweep_deleted_total",
		Help: "Terminal rows deleted after the retention window.",
	})

	fetchScannedRows = prometheus.NewHistogram(prometheus.HistogramOpts{
		Namespace: namespace, Name: "fetch_scanned_rows",
		Help:    "Rows read by the locked Fetch path (primary-key range scan filtered by state=PENDING).",
		Buckets: []float64{0, 1, 2, 4, 8, 17, 32, 64, 128},
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

// IncCursorAhead / IncFenceRegressed / IncEpochAhead / IncReconcileMismatch 是 critical 级计数。
func IncCursorAhead()       { cursorAheadTotal.Inc() }
func IncFenceRegressed()    { fenceRegressedTotal.Inc() }
func IncEpochAhead()        { epochAheadTotal.Inc() }
func IncReconcileMismatch() { reconcileMismatchTotal.Inc() }

// ObserveReconcileRead 记一次对账读账本的结果。
func ObserveReconcileRead(result string) { reconcileReadTotal.WithLabelValues(result).Inc() }

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
			cursorAheadTotal, fenceRegressedTotal, epochAheadTotal, reconcileMismatchTotal, reconcileReadTotal,
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
		for _, r := range []string{"found", "absent", "error"} {
			reconcileReadTotal.WithLabelValues(r)
		}
	})
}

// StartHTTP 启动 Prometheus /metrics 端点;addr 为空则关闭(与 trade 同模式)。返回的 stop 关闭监听。
func StartHTTP(addr string) (stop func()) {
	if addr == "" {
		logx.Info("[player_inbox] MetricsListenAddr empty; Prometheus /metrics endpoint disabled")
		return func() {}
	}
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	srv := &http.Server{
		Addr:              addr,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	go func() {
		logx.Infof("[player_inbox] Prometheus /metrics listening on %s", addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logx.Errorf("[player_inbox] metrics HTTP server exited: %v", err)
		}
	}()
	return func() { _ = srv.Close() }
}
