package clientplayerloginlogic

import (
	"sync/atomic"
	"time"

	"login/internal/logic/pkg/auth"

	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/metric"
)

// legacyLoginCallerTracker throttles the deprecation notice for the old
// cpp gate → login.Login RPC path AND exposes the same signal as a
// Prometheus counter for ARCH §12 T+1 exit-criteria dashboards.
//
// Two parallel sinks, deliberately:
//
//	atomic.Uint64 (legacyLoginCount / newLoginCount):
//	  Used by the throttled log line below. The log includes the
//	  in-process running totals so a single grep on a single replica
//	  answers "is anyone still hitting me on the old path right now"
//	  without a Prometheus query. This is what on-call cares about
//	  during the rollout window.
//
//	metric.CounterVec (loginPathTotal):
//	  Exported via go-zero's built-in /metrics endpoint
//	  (RpcServerConf.Prometheus). Operations scrapes this into Grafana
//	  to graph the legacy/new ratio over hours/days, which is what
//	  T+1 exit criteria depend on. Labels:
//	    path = "new" | "gate_bind" | "legacy"(取值与含义见 loginPath)
//	    auth_type = "password" | "access_token" | auth 注册表里的名字
//	                ("satoken" | "wechat" | "qq" | "netease")| "unknown"
//	                (收敛规则见 authTypeMetricLabel)
//	  authType is included so the dashboard can show e.g. "wechat
//	  traffic still on legacy" separately from "password traffic".
//
//	  计数点在 Login() 入口、认证之前:每次调用都计,含认证失败、抢锁失败和被
//	  LegacyGateLoginEnabled 拒绝的旧路径请求(开关关闭后运维靠 legacy 看还剩多少尾巴)。
//
//	  gate_bind(gate 上的 access_token 会话绑定)是新流程每次登录都要走的一步,
//	  必须单列:算进 legacy,legacy 永远降不到零,T+1 退出判据无法达成;
//	  算进 new,一次新流程登录计两次 new,legacy 占比被低估。所以 legacy 占比的
//	  分母只取 path!="gate_bind"(见 docs/ops/grafana-login-path-deprecation.json)。
//
// Both increment in lockstep on every new / legacy call to warnLegacyLoginCaller
// (gate_bind 只有 Prometheus 计数)。
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
		Help: "ClientPlayerLogin.Login calls bucketed by entry path " +
			"(new=Java Gateway HTTP, gate_bind=gate-forwarded access_token session bind, " +
			"legacy=gate-forwarded credential login) and auth_type. " +
			"Grafana dashboard: increase(login_auth_path_total{path=\"legacy\"}[5m]) " +
			"trending to zero is the ARCH §12 T+1 exit criterion.",
		Labels: []string{"path", "auth_type"},
	})
)

const legacyWarnThrottle = 60 * time.Second

// authTypeLabelUnknown 收容一切未注册的 auth_type。
const authTypeLabelUnknown = "unknown"

// authTypeMetricLabel 把客户端送来的 auth_type 收敛成有界的取值,供指标 label 与日志字段使用。
//
// 弃用遥测和 kill switch 都在认证之前,此时 auth_type 还是未经认证的客户端输入。原样当 Prometheus
// label,任何客户端换个字符串就能造一条新时间序列,基数没有上限(AGENTS §9 / §11.3)。规则:
//   - 空串是 "password" 的旧拼法,记 password(与 resolveAccount 一致);
//   - "password" / "access_token" 保留原值:二者是协议约定的固定名字,口令认证器生产默认不注册,
//     不能因为没注册就把口令流量记成 unknown;
//   - auth 注册表里已注册的名字(satoken / wechat / qq / netease 等)保留原值,面板按渠道拆分要用;
//   - 其余一律 unknown,大小写不符也算(注册表大小写敏感,与 classifyLoginPath 一致)。
//
// 注册表只在进程启动时(svc/auth_init.go)写入,所以取值集合在进程生命周期内有界。
func authTypeMetricLabel(authType string) string {
	switch authType {
	case "", "password":
		return "password"
	case "access_token":
		return "access_token"
	}
	if auth.Get(authType) != nil {
		return authType
	}
	return authTypeLabelUnknown
}

// warnLegacyLoginCaller records the caller source on both the in-process
// counters and the Prometheus counter, plus emits a throttled warn-level
// notice for the legacy path so on-call sees migration progress without
// log floods. gate_bind 只进 Prometheus:不计进两个进程内计数,也不打弃用日志。
//
// Login() 在认证之前调用本函数(被 kill switch 拒绝的旧路径请求也要计数),
// authType 一律先经 authTypeMetricLabel 收敛,label 与日志里都不出现客户端原始输入。
func warnLegacyLoginCaller(path loginPath, authType string) {
	authLabel := authTypeMetricLabel(authType)
	loginPathTotal.Inc(path.metricLabel(), authLabel)

	switch path {
	case loginPathHTTP:
		newLoginCount.Add(1)
		return
	case loginPathGateBind:
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
		"[DEPRECATION] ClientPlayerLogin.Login called via legacy cpp gate credential path "+
			"(auth_type=%s total_legacy=%d total_new=%d). "+
			"New clients should POST /api/login on Java Gateway; the gate path "+
			"is kept for backwards compatibility until ops sets "+
			"LegacyGateLoginEnabled=false.",
		authLabel, total, newLoginCount.Load(),
	)
}
