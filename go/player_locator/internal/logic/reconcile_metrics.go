package logic

import "github.com/zeromicro/go-zero/core/metric"

// 会话对账的指标(docs/design/player-locator-gate-session-index.md §9;告警 deploy/k8s/player-locator-alerts.yaml)。
//
// label 全是有限枚举,**不用 player_id,也不用 gate uuid**(AGENTS §9:高基数;gate uuid 每次进程启动都变)。
// 要定位具体 gate / 玩家看同一时刻的日志。
//
// 计数器每个副本各报一份,规则用 sum();gauge **只由 leader 产出**:进程启动预置 0、任期开始与结束都清零
// (resetReconcileLeaderGauges),所以每个副本都导出这些序列,非零值只可能来自现任(或卡死僵尸 leader 只会变旧的值)。
// 告警大多乘上 reconcile_is_leader==1 过滤后再 max();Stalled 例外,直接对全体副本取 max(见告警文件头)。
//
// 指标经 go-zero 自带的 /metrics agent 暴露。go-zero 在 Prometheus agent 启动之前的写入一律丢弃
// (core/metric 的 update 先判 prometheus.Enabled()),所以预置 0(primeReconcileMetrics)必须在
// zrpc.MustNewServer 之后调用,见 player_locator.go 启动对账的位置。
const (
	reconcileMetricsNamespace = "player_locator"

	metricReconcileRoundTotal      = "reconcile_round_total"
	metricReconcileLastSuccessUnix = "reconcile_last_success_unix"
	metricReconcileSessionsTotal   = "reconcile_sessions_total"
	metricReconcileDeadGates       = "reconcile_dead_gates"
	metricReconcileDeadGateMembers = "reconcile_dead_gate_members"
	metricReconcileLegacyWriters   = "reconcile_legacy_writers"
	metricReconcileIsLeader        = "reconcile_is_leader"
	metricGateIndexReady           = "gate_index_ready"
	metricGateIndexMembers         = "gate_index_members"
)

// reconcile_round_total / reconcile_last_success_unix 的 loop 取值。
const (
	reconcileLoopIndex  = "index"  // 索引循环:每拍读死 gate 的索引
	reconcileLoopRepair = "repair" // 补建:就绪标记缺失时的全键扫描
	reconcileLoopAudit  = "audit"  // 审计:就绪后按 AuditIntervalSeconds 的全键扫描
)

// reconcileLoops 是 loop 的全部取值:reconcile_last_success_unix 的预置与清零都按这张表走,
// 加一个 loop 只改这里(TestReconcileMetricLabelsArePrimed 核对它覆盖 reconcile_round_total 用到的全部 loop)。
var reconcileLoops = []string{reconcileLoopIndex, reconcileLoopRepair, reconcileLoopAudit}

// reconcile_round_total 的 result 取值。
const (
	reconcileResultOK         = "ok"
	reconcileResultEtcdError  = "etcd_error"
	reconcileResultRedisError = "redis_error"
	reconcileResultGenChanged = "gen_changed" // 补建期间出现过旧写者,结束时不设就绪标记
	reconcileResultIncomplete = "incomplete"  // 补建扫完了,但有 ONLINE 会话因键类型错误没能补进索引,不设就绪标记
	reconcileResultPanic      = "panic"
)

// reconcile_sessions_total 的 action 取值。
const (
	reconcileActionToDisconnecting = "to_disconnecting"
	reconcileActionCASConflict     = "cas_conflict"
	reconcileActionPrunedStale     = "pruned_stale"
	reconcileActionPrunedMissing   = "pruned_missing"
	reconcileActionPrunedMalformed = "pruned_malformed"
	reconcileActionPrunedBadMember = "pruned_bad_member"
	reconcileActionRepaired        = "repaired"       // 补建(就绪标记缺失)时补进的成员:升级窗口内是预期的
	reconcileActionAuditRepaired   = "audit_repaired" // 审计(就绪标记存在)时补进的成员:不变量被破坏,要排查
	// member_error / gate_error:某个成员(会话键)或某个 gate(索引键)的键类型不对(WRONGTYPE),本拍 / 本遍跳过它,
	// 不中止整拍。毒成员会一直留在索引里、那个 gate 不会被释放,由 DeadGatesPending 与这两个计数暴露。
	reconcileActionMemberError = "member_error"
	reconcileActionGateError   = "gate_error"
)

// reconcileRoundCombos 是 reconcile_round_total 预置为 0 的全部 (loop, result)。
var reconcileRoundCombos = [][2]string{
	{reconcileLoopIndex, reconcileResultOK},
	{reconcileLoopIndex, reconcileResultEtcdError},
	{reconcileLoopIndex, reconcileResultRedisError},
	{reconcileLoopIndex, reconcileResultPanic},
	{reconcileLoopRepair, reconcileResultOK},
	{reconcileLoopRepair, reconcileResultEtcdError},
	{reconcileLoopRepair, reconcileResultRedisError},
	{reconcileLoopRepair, reconcileResultGenChanged},
	{reconcileLoopRepair, reconcileResultIncomplete},
	{reconcileLoopRepair, reconcileResultPanic},
	{reconcileLoopAudit, reconcileResultOK},
	{reconcileLoopAudit, reconcileResultRedisError},
	{reconcileLoopAudit, reconcileResultPanic},
}

// reconcileSessionActions 是 reconcile_sessions_total 预置为 0 的全部 action。
var reconcileSessionActions = []string{
	reconcileActionToDisconnecting,
	reconcileActionCASConflict,
	reconcileActionPrunedStale,
	reconcileActionPrunedMissing,
	reconcileActionPrunedMalformed,
	reconcileActionPrunedBadMember,
	reconcileActionRepaired,
	reconcileActionAuditRepaired,
	reconcileActionMemberError,
	reconcileActionGateError,
}

var (
	reconcileRoundTotal = metric.NewCounterVec(&metric.CounterVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricReconcileRoundTotal,
		Help: "Session reconciler rounds by loop (index | repair | audit) and result " +
			"(ok | etcd_error | redis_error | gen_changed | incomplete | panic). Only the leader runs rounds.",
		Labels: []string{"loop", "result"},
	})
	reconcileLastSuccessUnix = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricReconcileLastSuccessUnix,
		Help: "Unix seconds of the last completed round per loop in the current leader term. Leader-only: primed to 0 " +
			"at process start and reset to 0 when a term starts and when it ends, so every replica exports the series " +
			"and a term reads 0 until its first completed round.",
		Labels: []string{"loop"},
	})
	reconcileSessionsTotal = metric.NewCounterVec(&metric.CounterVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricReconcileSessionsTotal,
		Help: "Per-member reconciler actions: to_disconnecting | cas_conflict | pruned_stale | pruned_missing | " +
			"pruned_malformed | pruned_bad_member | repaired (backfill while not ready) | audit_repaired " +
			"(member missing while the index was ready: invariant broken) | member_error (member skipped: its session " +
			"key has the wrong type) | gate_error (gate skipped for this round or pass: its index key has the wrong type).",
		Labels: []string{"action"},
	})
	reconcileDeadGates = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricReconcileDeadGates,
		Help:      "Gates in the index registry absent from etcd for two consecutive rounds and not yet released. Leader-only.",
	})
	reconcileDeadGateMembers = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricReconcileDeadGateMembers,
		Help: "Sum of index members (SCARD) of confirmed-dead gates after this round's budget: the work left. " +
			"Leader-only.",
	})
	reconcileLegacyWriters = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricReconcileLegacyWriters,
		Help: "Registered player_locator instances without the gate-session-index capability key, " +
			"as of the last successful etcd read. Leader-only.",
	})
	reconcileIsLeader = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricReconcileIsLeader,
		Help:      "1 on the replica holding the session reconciler leadership, 0 elsewhere.",
	})
	gateIndexReady = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricGateIndexReady,
		Help: "1 if the gate session index ready marker exists (renewed this round or just set by a backfill), " +
			"else 0. Leader-only.",
	})
	gateIndexMembers = metric.NewGaugeVec(&metric.GaugeVecOpts{
		Namespace: reconcileMetricsNamespace,
		Name:      metricGateIndexMembers,
		Help: "Sum of index members (SCARD) over live gates. An UPPER BOUND of ONLINE sessions, not an online " +
			"count: stale members linger until the audit prunes them. Leader-only.",
	})
)

// reconcileMetricFullNames 是本文件注册的全部指标全名(告警文件守卫用,见 reconcile_metrics_test.go)。
func reconcileMetricFullNames() []string {
	names := []string{
		metricReconcileRoundTotal, metricReconcileLastSuccessUnix, metricReconcileSessionsTotal,
		metricReconcileDeadGates, metricReconcileDeadGateMembers,
		metricReconcileLegacyWriters, metricReconcileIsLeader, metricGateIndexReady, metricGateIndexMembers,
	}
	for i, n := range names {
		names[i] = reconcileMetricsNamespace + "_" + n
	}
	return names
}

// primeReconcileMetrics 把计数器的全部 label 组合、is_leader 与全部 leader-only gauge 预置为 0:
//   - increase() 对「从无到 1」只有一个样本,计数器不预置会恰好漏掉第一次;
//   - is_leader 不预置,跟随者没有序列,NoLeader 的 sum 就分不清「没有 leader」与「没抓到」;
//   - last_success 不预置,从未成功过的 Pod 当选后在第一拍成功之前没有这条序列,Stalled 的判定就取决于别的 Pod
//     有没有残留序列,而不是现任自己的读数(2026-10-18 评审:旧规则按 is_leader 关联,现任为空集、卸任过的 Pod 挂着 0,
//     absent() 也不触发,「每一任都失败」时 critical 告警恰好无声)。
//
// 必须在 go-zero Prometheus agent 启动之后调用(见文件头)。
func primeReconcileMetrics() {
	for _, c := range reconcileRoundCombos {
		reconcileRoundTotal.Add(0, c[0], c[1])
	}
	for _, a := range reconcileSessionActions {
		reconcileSessionsTotal.Add(0, a)
	}
	reconcileIsLeader.Set(0)
	resetReconcileLeaderGauges()
}

// resetReconcileLeaderGauges 把只由 leader 产出的 gauge 清零:进程启动(预置)、任期开始、任期结束各调一次。
func resetReconcileLeaderGauges() {
	reconcileDeadGates.Set(0)
	reconcileDeadGateMembers.Set(0)
	reconcileLegacyWriters.Set(0)
	gateIndexReady.Set(0)
	gateIndexMembers.Set(0)
	for _, loop := range reconcileLoops {
		reconcileLastSuccessUnix.Set(0, loop)
	}
}

// onReconcilerLeaderChange 是选主器的 OnStateChange:当选置 1;卸任置 0 并清零 leader-only gauge。
func onReconcilerLeaderChange(isLeader bool) {
	if isLeader {
		reconcileIsLeader.Set(1)
		return
	}
	reconcileIsLeader.Set(0)
	resetReconcileLeaderGauges()
}
