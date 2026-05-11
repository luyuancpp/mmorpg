package clientplayerloginlogic

import (
	"sync/atomic"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/metric"
)

// legacyLoginCallerTracker throttles the deprecation notice for the old
// cpp gate → login.Login RPC path AND exposes the same signal as a
// Prometheus counter for ARCH §12 T+1 exit-criteria dashboards.
//
// Two parallel sinks, deliberately:
//
//   atomic.Uint64 (legacyLoginCount / newLoginCount):
//     Used by the throttled log line below. The log includes the
//     in-process running totals so a single grep on a single replica
//     answers "is anyone still hitting me on the old path right now"
//     without a Prometheus query. This is what on-call cares about
//     during the rollout window.
//
//   metric.CounterVec (loginPathTotal):
//     Exported via go-zero's built-in /metrics endpoint
//     (RpcServerConf.Prometheus). Operations scrapes this into Grafana
//     to graph the legacy/new ratio over hours/days, which is what
//     T+1 exit criteria depend on. Labels:
//       path = "legacy" | "new"
//       auth_type = "password" | "satoken" | "wechat" | "qq" | ...
//     authType is included so the dashboard can show e.g. "wechat
//     traffic still on legacy" separately from "password traffic".
//
// Both increment in lockstep on every call to warnLegacyLoginCaller.
// The atomic counters are NEVER reset (process-lifetime totals); the
// Prometheus counters are also monotonic per-process — Grafana
// rate()/increase() handle the scrape window.
var (
	legacyLoginCount atomic.Uint64
	newLoginCount    atomic.Uint64
	lastLegacyWarnNs atomic.Int64

	loginPathTotal = metric.NewCounterVec(&metric.CounterVecOpts{
		Namespace: "login",
		Subsystem: "auth",
		Name:      "path_total",
		Help: "ClientPlayerLogin.Login calls bucketed by transport path " +
			"(legacy=cpp gate forwarded, new=Java Gateway HTTP) and auth_type. " +
			"Grafana dashboard: increase(login_auth_path_total{path=\"legacy\"}[5m]) " +
			"trending to zero is the ARCH §12 T+1 exit criterion.",
		Labels: []string{"path", "auth_type"},
	})
)

const legacyWarnThrottle = 60 * time.Second

// warnLegacyLoginCaller records the caller source on both the in-process
// counters and the Prometheus counter, plus emits a throttled warn-level
// notice for the legacy path so on-call sees migration progress without
// log floods.
//
// authType is normalized to "password" when empty so the Prometheus label
// cardinality stays bounded and matches how loginlogic.go labels the rest
// of the request.
func warnLegacyLoginCaller(isLegacy bool, authType string) {
	pathLabel := "new"
	if isLegacy {
		pathLabel = "legacy"
	}
	authLabel := authType
	if authLabel == "" {
		authLabel = "password"
	}
	loginPathTotal.Inc(pathLabel, authLabel)

	if !isLegacy {
		newLoginCount.Add(1)
		return
	}
	total := legacyLoginCount.Add(1)

	now := time.Now().UnixNano()
	last := lastLegacyWarnNs.Load()
	if now-last < int64(legacyWarnThrottle) {
		return
	}
	// CAS so that concurrent callers don't all fire the warn.
	if !lastLegacyWarnNs.CompareAndSwap(last, now) {
		return
	}
	logx.Errorf(
		"[DEPRECATION] ClientPlayerLogin.Login called via legacy cpp gate path "+
			"(auth_type=%s total_legacy=%d total_new=%d). "+
			"New clients should POST /api/login on Java Gateway; the gate path "+
			"is kept for backwards compatibility and will be gated behind a "+
			"config flag in a later release.",
		authType, total, newLoginCount.Load(),
	)
}
