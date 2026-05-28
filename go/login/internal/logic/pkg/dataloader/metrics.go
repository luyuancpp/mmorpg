// metrics.go — Prometheus instrumentation for the dataloader package.
//
// (3) in the 2026-05-28 stress series: the (f) postmortem found that
// `entergame_preload_seconds` averaged 3s on success and 35s on failure.
// That's a 100-1000x slowdown from baseline expectation (sub-second).
// `preload` is a black box from outside — it could be Kafka send, the
// dispatcher's Pub/Sub wait, sub-cache Redis Get round-trips, or the
// fan-in finalize step. This file splits preload into 5 sub-stages so
// we can see exactly which one regresses under load.
//
// Stage map (matches EnsurePlayerAllDataInRedisAsync's control flow):
//
//   1. `cache_check`        — single Redis EXISTS for the parent key
//                              (the fast-path early-return check).
//   2. `sub_cache_check`    — loop of Redis GET for each sub-table key.
//                              On full cache hit the function returns
//                              before ever touching Kafka.
//   3. `dispatcher_register`— Register N dispatcher callbacks. CPU only,
//                              should be microseconds, but instrumented
//                              for completeness so a regression here is
//                              visible.
//   4. `kafka_send`         — sendDBReadRequests: SyncProducer.SendMessages
//                              for the batch of sub-tasks. mutex-serialized
//                              inside Kafka producer, prime suspect under
//                              load.
//   5. `callback_wait`      — from "last Kafka ack returned" to "finalize
//                              fires" (i.e. all dispatcher callbacks have
//                              run + saveToRedis returned). This is the
//                              critical bucket: if `preload_seconds` is 3s
//                              and this is 2.95s, then DB-worker turnaround
//                              + Pub/Sub plumbing is the bottleneck, not
//                              Kafka send.
//
// All buckets use the same shape as login_queue / entergame for consistency.
// Result labels: success | failed | timeout, mirroring the parent histogram
// in clientplayerlogin/metrics.go.
package dataloader

import (
	"time"

	"github.com/zeromicro/go-zero/core/metric"
)

const metricNamespace = "dataloader_preload"

var stageBuckets = []float64{
	0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 30,
}

var (
	// stage1: parent-key EXISTS check. Expected sub-ms.
	cacheCheckSeconds = metric.NewHistogramVec(&metric.HistogramVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "cache_check_seconds",
		Help:      "Time spent in Redis EXISTS check for the parent PlayerAllData key.",
		Labels:    []string{"hit"},
		Buckets:   stageBuckets,
	})

	// stage2: sub-table cache GET loop. Recorded only when the parent
	// key is not yet cached. Multi-key, so latency scales with sub-table
	// count.
	subCacheCheckSeconds = metric.NewHistogramVec(&metric.HistogramVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "sub_cache_check_seconds",
		Help: "Time spent in the Redis GET loop over sub-table keys. " +
			"Recorded once per preload, after the parent EXISTS miss. " +
			"Label `subs` is the number of subs that needed a DB fetch " +
			"(0 = all sub keys hot, full cache hit on a multi-key path).",
		Labels:  []string{"subs"},
		Buckets: stageBuckets,
	})

	// stage3: dispatcher Register loop. CPU-bound, microseconds expected.
	dispatcherRegisterSeconds = metric.NewHistogramVec(&metric.HistogramVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "dispatcher_register_seconds",
		Help:      "Time spent registering dispatcher callbacks (CPU-only loop).",
		Labels:    []string{},
		Buckets:   stageBuckets,
	})

	// stage4: Kafka SyncProducer.SendMessages call. Mutex-serialized in
	// the producer, so under load this is where contention shows up if
	// Kafka is the bottleneck.
	kafkaSendSeconds = metric.NewHistogramVec(&metric.HistogramVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "kafka_send_seconds",
		Help: "Time spent in sendDBReadRequests (SyncProducer.SendMessages). " +
			"Result: success | failed.",
		Labels:  []string{"result"},
		Buckets: stageBuckets,
	})

	// stage5: wait from "kafka sent" to "finalize fired". This is the
	// big one — if it dominates total preload, the DB worker turnaround
	// + Pub/Sub Plumbing is the bottleneck. If it's small but
	// kafka_send is large, Kafka producer mutex contention is the
	// bottleneck. The split tells you which dial to turn.
	callbackWaitSeconds = metric.NewHistogramVec(&metric.HistogramVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "callback_wait_seconds",
		Help: "Time from final Kafka ack to dispatcher fan-in completion. " +
			"Includes DB worker SELECT + dispatcher Pub/Sub roundtrip. " +
			"Result: success | failed | timeout.",
		Labels:  []string{"result"},
		Buckets: stageBuckets,
	})

	// Counters for terminal states of the wait stage — cheaper to alert
	// on than histogram counts; mirrors entergame_total.
	callbackResultTotal = metric.NewCounterVec(&metric.CounterVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "callback_result_total",
		Help: "Terminal-state count of the dispatcher fan-in wait. " +
			"Result: success | failed | timeout.",
		Labels: []string{"result"},
	})
)

const (
	resultSuccess = "success"
	resultFailed  = "failed"
	resultTimeout = "timeout"
)

// observeStage returns a deferred-timer closure for the matching
// histogram. Same pattern as clientplayerlogin/metrics.go's helper,
// duplicated here to avoid a cross-package metric helper (the two
// histogram namespaces are independent and should stay that way).
func observeStage(h metric.HistogramVec, labels ...string) func() {
	start := time.Now()
	return func() {
		h.ObserveFloat(time.Since(start).Seconds(), labels...)
	}
}
