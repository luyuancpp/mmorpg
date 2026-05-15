// metrics.go — Prometheus instrumentation for the AssignGate login queue.
//
// All metrics are exposed through go-zero's built-in /metrics endpoint
// (RpcServerConf.Prometheus in login.yaml). Naming follows the
// `login_queue_*` convention for easy Grafana filtering.
//
// Six metrics, mapping 1:1 onto the runbook's "待补 Prometheus 指标" table:
//
//   login_queue_size              Gauge       per zone     ZCARD snapshot
//   login_queue_admitted_inflight Gauge       per zone     SCARD snapshot
//   login_queue_wait_seconds      Histogram   per zone     time from Enqueue to Lookup-admit
//   login_queue_admit_total       Counter     per zone     dispatcher PopAdmit results
//   login_queue_expired_total     Counter     per zone+reason  EXPIRED responses (token invalid / meta gone / etc.)
//   login_queue_dispatcher_is_leader  Gauge   per pod      1 = this pod is the leader, 0 = standby
//
// Why only these six (and not, say, "rank histogram" or "per-gate admit
// distribution"):
//   - rank already shows up in the periodic robot stats line; on the server
//     side ZRANK is O(log N) per call, recording it for every Lookup is
//     wasteful when the queue length itself (login_queue_size) tells the
//     same story.
//   - per-gate admit distribution is interesting for capacity planning but
//     not for SLO; defer to a later PR if ops asks for it.
//
// Histogram bucket choice for wait_seconds:
//   - sub-second buckets: catches "queue effectively empty, dispatcher
//     just hadn't gotten to it" cases that should NOT show up in the tail.
//   - 1s..120s buckets: this is the "normal queue" range; a healthy server
//     under capacity should sit in 5-30s.
//   - 300s/600s outliers: anything past 5min is a red flag (dispatcher
//     stalled or AdmitTTL races).
//
// All metrics are nil-safe to call: counters/gauges from go-zero
// metric.NewXxxVec are non-nil singletons, so handlers don't need
// guard checks.
package loginqueue

import (
	"context"
	"strconv"
	"time"

	"github.com/zeromicro/go-zero/core/metric"
)

const metricNamespace = "login_queue"

var (
	// queueSizeGauge mirrors ZCARD queue:zone:{zone}. Updated periodically
	// by the dispatcher (every drain tick) and on Enqueue/Lookup paths so
	// scrape granularity matches dispatcher granularity.
	queueSizeGauge = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "size",
		Help: "Current length of the AssignGate login queue per zone (ZCARD queue:zone:{zone}). " +
			"Healthy: drops to 0 within a few minutes after open-server burst. " +
			"Red flag: monotonically rising — dispatcher likely stalled.",
		Labels: []string{"zone"},
	})

	// admittedInflightGauge mirrors SCARD admitted:zone:{zone} — players
	// holding an admit token but not yet connected to gate. Critical for
	// capacity accounting (FreeSlots subtracts this).
	admittedInflightGauge = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "admitted_inflight",
		Help: "Players who have an admit token but haven't connected to gate yet " +
			"(SCARD admitted:zone:{zone}). Sustained > 100 per zone suggests clients " +
			"are dropping admit tokens; check AdmitTTL and client poll cadence.",
		Labels: []string{"zone"},
	})

	// waitSecondsHistogram measures end-to-end queue wait per admitted
	// entry. Recorded in Lookup() when status flips QUEUEING → ADMITTED.
	waitSecondsHistogram = metric.NewHistogramVec(&metric.HistogramVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "wait_seconds",
		Help: "End-to-end wait time for queue entries that were admitted, " +
			"measured from Enqueue (queue:meta:{queueId} createdAt) to the " +
			"Lookup call that consumed admit:{queueId}. p99 > 300s is a red " +
			"flag — dispatcher can't keep up with EnterGame.",
		Labels:  []string{"zone"},
		Buckets: []float64{0.5, 1, 2, 5, 10, 20, 30, 60, 120, 300, 600},
	})

	// admitTotal counts dispatcher PopAdmit successes. The increase rate
	// equals the dispatcher's effective drain throughput.
	admitTotal = metric.NewCounterVec(&metric.CounterVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "admit_total",
		Help: "Cumulative count of queue entries admitted by the dispatcher, " +
			"per zone. rate(login_queue_admit_total[1m]) == dispatcher drain QPS.",
		Labels: []string{"zone"},
	})

	// expiredTotal counts EXPIRED responses. The reason label distinguishes
	// the failure modes that share the same status code on the wire:
	//   "bad_signature" — HMAC mismatch / malformed token
	//   "ttl"           — body.ExpireTS in the past
	//   "meta_gone"     — Lookup found token but meta TTL'd before admit
	//   "queue_disabled" — Queue.Enabled=false but client polled QueryQueueStatus
	expiredTotal = metric.NewCounterVec(&metric.CounterVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "expired_total",
		Help: "Cumulative count of EXPIRED responses to AssignGate/QueryQueueStatus, " +
			"per zone (best-effort, may be 'unknown' when token couldn't be parsed) " +
			"and reason. Sustained non-zero 'ttl' = client polling slower than admit_ttl.",
		Labels: []string{"zone", "reason"},
	})

	// dispatcherIsLeaderGauge is 1 on the pod currently holding the
	// dispatcher leader lock, 0 elsewhere. Sum across pods should be
	// exactly 1 in steady state; transient 0 (no leader) or 2 (split-
	// brain after Redis flap) is expected only briefly. The pod label
	// disambiguates which replica is leading at any moment.
	dispatcherIsLeaderGauge = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: metricNamespace,
		Subsystem: "",
		Name:      "dispatcher_is_leader",
		Help: "Whether this login pod currently holds the dispatcher leader lock " +
			"(1=leader, 0=standby). sum(login_queue_dispatcher_is_leader) should " +
			"be 1 in steady state.",
		Labels: []string{"pod"},
	})
)

// Reason strings for expiredTotal — exported as constants so the call
// sites self-document why they're recording an EXPIRED.
const (
	ExpireReasonBadSignature  = "bad_signature"
	ExpireReasonTTL           = "ttl"
	ExpireReasonMetaGone      = "meta_gone"
	ExpireReasonQueueDisabled = "queue_disabled"
)

// labelZone formats a uint32 zone id for use as a Prometheus label.
// Centralized so a future label-strategy change (e.g. zone groups) only
// touches one spot.
func labelZone(zoneID uint32) string {
	return strconv.FormatUint(uint64(zoneID), 10)
}

// recordExpired is a small wrapper so callers don't repeat the labelZone
// formatting and so unknown-zone (parse-failed token) gets a consistent
// "unknown" label instead of "0", which would alias zone 0.
func recordExpired(zoneID uint32, reason string) {
	z := "unknown"
	if zoneID != 0 {
		z = labelZone(zoneID)
	}
	expiredTotal.Inc(z, reason)
}

// RecordExpiredQueueDisabled is an exported helper for handlers that need
// to surface "queue disabled" without going through ParseAndVerifyQueueToken.
// Lives in the package's public API because the handler in
// loginpregate/querystatuslogic.go records it directly.
func RecordExpiredQueueDisabled() {
	recordExpired(0, ExpireReasonQueueDisabled)
}

// recordAdmit is called by the dispatcher after each successful PopAdmit
// batch, with the count of entries that were actually admitted (which may
// be less than the requested batch if signFn failed for some entries).
func recordAdmit(zoneID uint32, count int) {
	if count <= 0 {
		return
	}
	admitTotal.Add(float64(count), labelZone(zoneID))
}

// recordWait records one queue wait observation in the histogram.
// Negative durations (clock skew across pods) are clamped to 0 — better
// than polluting the histogram with bogus data.
//
// We use ObserveFloat (not Observe, which truncates to int64 seconds)
// because the histogram buckets start at 0.5s — int64 truncation would
// collapse all sub-1s observations into the 0 bucket and hide the
// "queue is healthy, drained instantly" case from the dashboard.
func recordWait(zoneID uint32, d time.Duration) {
	secs := d.Seconds()
	if secs < 0 {
		secs = 0
	}
	waitSecondsHistogram.ObserveFloat(secs, labelZone(zoneID))
}

// publishGaugeSnapshot updates queue_size and admitted_inflight gauges
// for the given zone by reading Redis directly. Called by the dispatcher
// once per drain tick — that cadence (1s) is the right granularity for
// these metrics; doing it inline on every Enqueue/Lookup would drown
// Redis in extra ZCARD/SCARD calls for marginal accuracy gain.
//
// The dispatcher invokes this even for zones where it admits nothing,
// so the gauges stay fresh during periods of capacity exhaustion.
func publishGaugeSnapshot(ctx context.Context, q *Queue, zoneID uint32) {
	if size, err := q.QueueLen(ctx, zoneID); err == nil {
		queueSizeGauge.Set(float64(size), labelZone(zoneID))
	}
	if inflight, err := q.AdmittedCount(ctx, zoneID); err == nil {
		admittedInflightGauge.Set(float64(inflight), labelZone(zoneID))
	}
}
