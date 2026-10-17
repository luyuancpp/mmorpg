// Package metrics 暴露 mail 服务的 Prometheus 指标(设计 docs/design/mail-system.md §2.5、§4.3、§4.5、§4.6、§4.7)。
// /metrics 端口是 :9240(etc/mail.yaml 的 MetricsListenAddr)。
//
// 两条硬约束(AGENTS.md §9):
//  1. label 全是**有限枚举**,取值集合就是本文件的常量。player_id / mail_id / request_id 一律只进日志。
//  2. 指标**懒注册**:Vec 在包级变量就建好,MustRegister 只在 Register 的 sync.Once 里做(Start 会调它)。没调的
//     `-migrate` 进程照样可以安全 Inc,不会重复注册 panic;单测要读数时先调 Register,再从 prometheus.DefaultGatherer 读。
//
// 所有 label 组合都在 register 里**预建 0 值序列**:Prometheus 里"从未发生过"的序列根本不存在,
// `increase(...[1h]) > 0` 一类告警在序列缺失时既不报警也不报错,与"一切正常"长得一样(设计 §4.7)。
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

// subsystem 决定指标名前缀:mail_admin_send_total / mail_push_total / …改它等于改全部指标名。
const subsystem = "mail"

// MailAdmin 发件的 method label。
const (
	MethodSendPersonal = "send_personal"
	MethodSendSystem   = "send_system"
)

// MailAdmin 发件的 sender_kind label(与 mail_admin.proto 的 MailSenderKind 对应;未知值一律记 unknown,不拿数字当 label)。
const (
	SenderOpsTool = "ops_tool"
	SenderRobot   = "robot"
	SenderUnknown = "unknown"
)

// MailAdmin 发件的 outcome label(设计 §2.5):
//   - ok:首次发出;duplicated:同键同摘要的重放,回首次结果;rejected:业务拒绝(满箱、冲突、附件不合法……);
//   - fault:存储 / 号段故障(kServiceUnavailable);denied:令牌缺失或错误(gRPC PermissionDenied)。
const (
	OutcomeOK         = "ok"
	OutcomeDuplicated = "duplicated"
	OutcomeRejected   = "rejected"
	OutcomeFault      = "fault"
	OutcomeDenied     = "denied"
)

// 推送的 outcome label(设计 §4.5)。offline 与 session_error、error 必须分开:读会话失败若也记成 offline,
// 一次协议事故会表现为"推送全都不推"而没有任何异常指标。
const (
	PushOK           = "ok"            // 已写入 Kafka
	PushOffline      = "offline"       // 收件人无会话或非 ONLINE:最常见的正常分支
	PushSessionError = "session_error" // 读 / 解码 player:session 失败
	PushError        = "error"         // 推送通道未配置、Kafka 写失败、组包失败
)

// sweep 的 table label:sweep 物理删的三张表。mail_box 守卫行不清理(行数 ≤ 被发过定向邮件的玩家数,设计 §3.4)。
const (
	TablePersonal    = "mail_personal"
	TableSystem      = "mail_system"
	TableSystemState = "mail_system_state"
)

// sweep 的 mode label。与 config.SweepMode* 同值(本包保持叶子包,不 import config;三份字面量由测试钉一致)。
const (
	SweepModeReportOnly = "report_only"
	SweepModeDelete     = "delete"
)

var (
	adminSendTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "admin_send_total",
		Help:      "MailAdmin send calls by method (send_personal|send_system), sender_kind (ops_tool|robot|unknown) and outcome (ok|duplicated|rejected|fault|denied).",
	}, []string{"method", "sender_kind", "outcome"})

	pushTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "push_total",
		Help:      "NotifyMailEvent pushes for new personal mail by outcome (ok|offline|session_error|error).",
	}, []string{"outcome"})

	claimShortCircuitTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "claim_short_circuit_total",
		Help:      "ClaimMailAttachments calls answered with MailClaimNotOpen in M1 (claiming is not open yet).",
	})

	listTruncatedTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "list_truncated_total",
		Help:      "ListMails calls whose active system mail set hit Mail.MaxActiveSystemMails (players see fewer system mails).",
	})

	sweepCandidateRows = prometheus.NewGaugeVec(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "sweep_candidate_rows",
		Help:      "Rows the last sweep pass counted as eligible for physical deletion (bounded count), by table and sweep mode.",
	}, []string{"table", "mode"})

	sweepDeletedTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "sweep_deleted_total",
		Help:      "Rows physically deleted by sweep, by table.",
	}, []string{"table"})

	// 设计 §4.6 写的名字是 mail_sweep_invalid_rows;它是 counter,按 Prometheus 命名约定带 _total 后缀(§12.2 登记)。
	sweepInvalidRows = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "sweep_invalid_rows_total",
		Help:      "Rows with expire_ms = 0 seen by sweep (counted, never deleted; send-side validation was bypassed), by table.",
	}, []string{"table"})

	sweepLastRunUnixtime = prometheus.NewGauge(prometheus.GaugeOpts{
		Subsystem: subsystem,
		Name:      "sweep_last_run_unixtime",
		Help:      "Unix time at the end of the last sweep pass, written every pass regardless of rows deleted; stops advancing when the loop is stuck.",
	})

	registerOnce sync.Once
)

// ObserveAdminSend 记一次 MailAdmin 发件的终态。三个参数都取本包常量,别传数字或拼出来的串。
func ObserveAdminSend(method, senderKind, outcome string) {
	adminSendTotal.WithLabelValues(method, senderKind, outcome).Inc()
}

// ObservePush 记一次推送的终态,outcome 取 Push* 之一。
func ObservePush(outcome string) {
	pushTotal.WithLabelValues(outcome).Inc()
}

// ObserveClaimShortCircuit 记一次领取短路(设计 §4.3:M1 里这是领取的唯一可观察痕迹)。
func ObserveClaimShortCircuit() {
	claimShortCircuitTotal.Inc()
}

// ObserveListTruncated 记一次 ListMails 读系统有效集时撞到上限(有告警 MailSystemListTruncated)。
func ObserveListTruncated() {
	listTruncatedTotal.Inc()
}

// SetSweepCandidateRows 刷新某张表在某个 sweep 模式下本轮的候选行数。每轮都要写,**包括 0**。
func SetSweepCandidateRows(table, mode string, rows float64) {
	sweepCandidateRows.WithLabelValues(table, mode).Set(rows)
}

// AddSweepDeleted 累加某张表本轮实删的行数(0 也可以调,无副作用)。
func AddSweepDeleted(table string, rows float64) {
	sweepDeletedTotal.WithLabelValues(table).Add(rows)
}

// AddSweepInvalidRows 累加 expire_ms = 0 的保险分支命中数(有告警 MailSweepInvalidRows)。
func AddSweepInvalidRows(table string, rows float64) {
	sweepInvalidRows.WithLabelValues(table).Add(rows)
}

// MarkSweepRun 在每轮 sweep 结束时写当前时刻(有告警 MailSweepStalled:它长期不前进 = 循环卡死或停止调度)。
func MarkSweepRun(now time.Time) {
	sweepLastRunUnixtime.Set(float64(now.Unix()))
}

// Register 注册全部指标并预建 0 值序列。幂等:重复调用不会 panic。
func Register() {
	registerOnce.Do(func() {
		prometheus.MustRegister(
			adminSendTotal,
			pushTotal,
			claimShortCircuitTotal,
			listTruncatedTotal,
			sweepCandidateRows,
			sweepDeletedTotal,
			sweepInvalidRows,
			sweepLastRunUnixtime,
		)
		for _, method := range []string{MethodSendPersonal, MethodSendSystem} {
			for _, sender := range []string{SenderOpsTool, SenderRobot, SenderUnknown} {
				for _, outcome := range []string{OutcomeOK, OutcomeDuplicated, OutcomeRejected, OutcomeFault, OutcomeDenied} {
					adminSendTotal.WithLabelValues(method, sender, outcome)
				}
			}
		}
		for _, outcome := range []string{PushOK, PushOffline, PushSessionError, PushError} {
			pushTotal.WithLabelValues(outcome)
		}
		for _, table := range []string{TablePersonal, TableSystem, TableSystemState} {
			for _, mode := range []string{SweepModeReportOnly, SweepModeDelete} {
				sweepCandidateRows.WithLabelValues(table, mode)
			}
			sweepDeletedTotal.WithLabelValues(table)
			sweepInvalidRows.WithLabelValues(table)
		}
	})
}

// Start 启动 Prometheus /metrics 端点并返回关闭函数;addr 为空则不启动,返回空操作。
//
// 端点是"起不来也要让进程活着"的辅助设施:ListenAndServe 失败只打日志,不 panic(本机多副本撞端口很常见)。
// 注册只做一次:Start 被重复调用不会 panic。关闭函数有界(1s),供 lifecycle 的 CloseResources 调用。
func Start(addr string) (stop func()) {
	if addr == "" {
		logx.Info("[mail] MetricsListenAddr empty; Prometheus /metrics endpoint disabled")
		return func() {}
	}
	Register()
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	srv := &http.Server{
		Addr:    addr,
		Handler: mux,
		// ReadHeaderTimeout 防慢速请求占着连接不放。
		ReadHeaderTimeout: 5 * time.Second,
	}
	safego.Go("mail.metrics_http", func() {
		logx.Infof("[mail] Prometheus /metrics listening on %s", addr)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logx.Errorf("[mail] metrics HTTP server exited: %v", err)
		}
	})
	return func() {
		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()
		if err := srv.Shutdown(ctx); err != nil {
			logx.Errorf("[mail] metrics HTTP server shutdown: %v", err)
		}
	}
}
