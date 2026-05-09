package clientplayerloginlogic

import (
	"sync/atomic"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

// legacyLoginCallerTracker throttles the deprecation notice for the old
// cpp gate → login.Login RPC path. Every observed call increments the
// counter; a single summary log line fires at most once per minute, so a
// busy fleet doesn't flood the log but ops can still see migration progress
// by scraping the counter (or promoting it to a Prometheus gauge later).
//
// legacyLoginCount — monotonic total of calls arriving via the legacy path
// newLoginCount    — monotonic total of calls arriving via Java Gateway
//                    (i.e. with no SessionDetails attached)
//
// Rationale for keeping this in-process (not a distributed metric yet): the
// first signal we want is "is there a single tenant still on the old path",
// which any one replica can answer on its own. When we're ready to retire
// the legacy path we'll replace this with a Prometheus counter wired up
// via go-zero's metrics middleware.
var (
	legacyLoginCount atomic.Uint64
	newLoginCount    atomic.Uint64
	lastLegacyWarnNs atomic.Int64
)

const legacyWarnThrottle = 60 * time.Second

// warnLegacyLoginCaller records the caller source and, for the legacy
// code path, emits a throttled warn-level notice pointing operators at the
// replacement HTTP endpoint.
//
// authType is passed purely for the log; it lets a single-line grep tell
// password/satoken/wechat callers apart without re-parsing the RPC args.
func warnLegacyLoginCaller(isLegacy bool, authType string) {
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
