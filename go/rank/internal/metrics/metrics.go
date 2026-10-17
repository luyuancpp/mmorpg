// Package metrics 暴露 rank 服务的 Prometheus 指标(设计 docs/design/leaderboard-system.md §3.6 / §3.7)。
// /metrics 端口是 :9250(etc/rank.yaml 的 MetricsListenAddr)。
//
// 两条硬约束(AGENTS.md §9):
//  1. label 全是**有限枚举**,取值集合就是本文件的常量。player_id 一律只进日志。
//  2. 指标**懒注册**:Vec 在包级变量就建好,MustRegister 只在 Register 的 sync.Once 里做(Start 会调它)。
//
// 所有 label 组合都在 Register 里**预建 0 值序列**:Prometheus 里"从未发生过"的序列根本不存在,
// `increase(...) > 0` 一类告警在序列缺失时既不报警也不报错,与"一切正常"长得一样。
package metrics

import (
	"context"
	"net/http"
	"sync"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/zeromicro/go-zero/core/logx"

	"shared/safego"
)

// subsystem 决定指标名前缀:rank_write_total / rank_read_total / …改它等于改全部指标名(告警按这些名字写)。
const subsystem = "rank"

// 写榜的 outcome label(§3.6;invalid 是落码时补的一档,见设计 §12.3 偏差第 7 条):
//   - written:同代际 games 更大或首次上榜;stale:版本不新(重放 / 乱序 / 旧代际残留;Redis 慢时 go-zero 的命令重试
//     会把一次已生效的写重发一遍,重发那次读到新版本、返回 stale —— 此时 written / gen_reset 偏低、stale 偏高,只作趋势);
//   - gen_reset:match 侧评分键丢过、新代际覆盖;out_of_range:分数 / 时间超出编码范围(理论上不可能,出现即 bug);
//   - invalid:消息反序列化失败,或 player_id / games / rating_gen 为 0(match 的对外承诺被违反;都是永久跳过,
//     告警 RankWriteFailing 覆盖它 —— 跳过时 rank_last_consume_unixtime 照常前进,RankConsumerStalled 看不见这类问题);
//   - error:Redis 写失败(瞬时,退避重试)或某个玩家的版本值被手写坏(永久跳过)。
const (
	WriteWritten    = "written"
	WriteStale      = "stale"
	WriteGenReset   = "gen_reset"
	WriteOutOfRange = "out_of_range"
	WriteInvalid    = "invalid"
	WriteError      = "error"
)

// 读路径的 method / outcome label。outcome 与 serverbase.TipVerdict 的分类对齐:ok / rejected(业务拒绝)/ fault(故障)。
const (
	MethodGetRankPage = "get_rank_page"

	ReadOK       = "ok"
	ReadRejected = "rejected"
	ReadFault    = "fault"
)

// 名字补全的 outcome(§4.3:fail-open 必须配指标)。
const (
	NameLookupOK    = "ok"
	NameLookupError = "error"
)

// BoardMatchRating 是 rank_board_size 的 board label(v1 只有这一张榜)。
const BoardMatchRating = "match_rating"

var (
	writeTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "write_total",
		Help:      "Leaderboard snapshot writes by outcome (written|stale|gen_reset|out_of_range|invalid|error).",
	}, []string{"outcome"})

	consumeErrorsTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "consume_errors_total",
		Help:      "Rating snapshot messages that failed to decode or to be written (each failed attempt counts once).",
	})

	lastConsumeUnixtime = prometheus.NewGauge(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "last_consume_unixtime",
		Help:      "Unix time of the last successfully processed rating snapshot message; stops advancing when the consumer is stuck.",
	})

	consumeLagSeconds = prometheus.NewGauge(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "consume_lag_seconds",
		Help:      "Seconds between the battle finishing (finished_at_ms) and the snapshot being written; only updated when a message arrives.",
	})

	readTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "read_total",
		Help:      "Leaderboard reads by method and outcome (ok|rejected|fault).",
	}, []string{"method", "outcome"})

	readDuration = prometheus.NewHistogramVec(prometheus.HistogramOpts{
		Subsystem: subsystem,
		Name:      "read_duration_seconds",
		Help:      "Leaderboard read latency by method (Redis read plus name lookup).",
		Buckets:   []float64{0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2, 4},
	}, []string{"method"})

	nameLookupTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "name_lookup_total",
		Help:      "BatchGetPlayerName calls for a leaderboard page by outcome (ok|error); names are left empty on error (fail-open).",
	}, []string{"outcome"})

	boardSize = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "board_size",
		Help:      "ZCARD of the leaderboard, sampled by each replica (aggregate with max()).",
	}, []string{"board"})

	registerOnce sync.Once
)

// ObserveWrite 记一次写榜的终态,outcome 取 Write* 之一。
func ObserveWrite(outcome string) { writeTotal.WithLabelValues(outcome).Inc() }

// ObserveConsumeError 记一次消费失败(反序列化 / 写榜失败的一次尝试)。
func ObserveConsumeError() { consumeErrorsTotal.Inc() }

// MarkConsumed 在一条消息被成功处理(写入或判定 stale)后写当前时刻与消费延迟。finishedAtMs = 0 时不更新延迟。
func MarkConsumed(now time.Time, finishedAtMs uint64) {
	lastConsumeUnixtime.Set(float64(now.Unix()))
	if finishedAtMs == 0 {
		return
	}
	lag := now.Sub(time.UnixMilli(int64(finishedAtMs))).Seconds()
	if lag < 0 {
		lag = 0 // 两台机器的时钟偏差:负延迟没有意义
	}
	consumeLagSeconds.Set(lag)
}

// ObserveRead 记一次读,outcome 取 Read* 之一。
func ObserveRead(method, outcome string, elapsed time.Duration) {
	readTotal.WithLabelValues(method, outcome).Inc()
	readDuration.WithLabelValues(method).Observe(elapsed.Seconds())
}

// ObserveNameLookup 记一次批量取名的结果。
func ObserveNameLookup(outcome string) { nameLookupTotal.WithLabelValues(outcome).Inc() }

// SetBoardSize 刷新榜长采样。
func SetBoardSize(board string, size int) { boardSize.WithLabelValues(board).Set(float64(size)) }

// Register 注册全部指标并预建 0 值序列。幂等。
func Register() {
	registerOnce.Do(func() {
		prometheus.MustRegister(writeTotal, consumeErrorsTotal, lastConsumeUnixtime, consumeLagSeconds,
			readTotal, readDuration, nameLookupTotal, boardSize)
		for _, outcome := range []string{WriteWritten, WriteStale, WriteGenReset, WriteOutOfRange, WriteInvalid, WriteError} {
			writeTotal.WithLabelValues(outcome)
		}
		for _, outcome := range []string{ReadOK, ReadRejected, ReadFault} {
			readTotal.WithLabelValues(MethodGetRankPage, outcome)
		}
		readDuration.WithLabelValues(MethodGetRankPage)
		for _, outcome := range []string{NameLookupOK, NameLookupError} {
			nameLookupTotal.WithLabelValues(outcome)
		}
		// rank_board_size **刻意不预建 0 值**:告警 RankBoardShrunk 比的是 max(rank_board_size) 与它 1h 内的最大值,
		// 刚起的副本若先报 0,单副本部署会在第一次采样前误报"榜缩水"。序列在第一次采样成功时才出现。
	})
}

// Start 启动 Prometheus /metrics 端点并返回关闭函数;addr 为空则不启动,返回空操作(照 mail)。
func Start(addr string) (stop func()) {
	if addr == "" {
		logx.Info("[rank] MetricsListenAddr empty; Prometheus /metrics endpoint disabled")
		return func() {}
	}
	Register()
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	srv := &http.Server{Addr: addr, Handler: mux, ReadHeaderTimeout: 5 * time.Second}
	safego.Go("rank.metrics_http", func() {
		logx.Infof("[rank] Prometheus /metrics listening on %s", addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logx.Errorf("[rank] metrics HTTP server exited: %v", err)
		}
	})
	return func() {
		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()
		if err := srv.Shutdown(ctx); err != nil {
			logx.Errorf("[rank] metrics HTTP server shutdown: %v", err)
		}
	}
}
