package inbox

// store_* 与 reconcile.go 抽出来的纯逻辑单测(decide.go 下半部分)。读写与事务本身只有
// store_integration_test.go(-tags integration + 真库)覆盖;这里钉住「读出之后怎么判、写回什么」。

import (
	"testing"

	"player_inbox/internal/constants"
	"player_inbox/internal/metrics"

	inboxpb "proto/player_inbox"
)

const (
	fenceFirst   = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_FIRST
	fenceMatch   = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_MATCH
	fenceAdvance = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_ADVANCE
)

// ── 入箱 ─────────────────────────────────────────────────────────────────

// TestDecideEnqueueLockedDedupesBeforeFull 钉住锁内「先判重、后判满箱」(设计 §6.2 第 7.2–7.3 步、§8 #2):
// 差一条满箱时原请求与重试赛跑,输的一方看见赢家已提交的行 —— 此时流也满了,结局必须是 DUPLICATE 而不是 FULL。
func TestDecideEnqueueLockedDedupesBeforeFull(t *testing.T) {
	digest := ContentSHA256(gmBlock, testNowMs, []byte{8, 1})
	other := ContentSHA256(gmBlock, testNowMs+1, []byte{8, 1})
	existing := &inboxpb.PlayerInboxItemRow{Seq: 77, State: uint32(statePending), ContentSha256: digest}

	cases := []struct {
		name     string
		existing *inboxpb.PlayerInboxItemRow
		digest   []byte
		open     uint32
		want     EnqueueResult
		proceed  bool
	}{
		{"已存在同摘要 + 满箱 → DUPLICATE", existing, digest, 64, EnqueueResult{Seq: 77, State: statePending}, false},
		{"已存在不同摘要 + 满箱 → CONFLICT", existing, other, 64, EnqueueResult{Code: constants.ErrIdempotencyConflict}, false},
		{"已存在同摘要 + 未满 → DUPLICATE", existing, digest, 1, EnqueueResult{Seq: 77, State: statePending}, false},
		{"不存在 + 满箱 → FULL", nil, digest, 64, EnqueueResult{Code: constants.ErrFull}, false},
		{"不存在 + 超过上限也算满", nil, digest, 65, EnqueueResult{Code: constants.ErrFull}, false},
		{"不存在 + 差一条满 → 放行", nil, digest, 63, EnqueueResult{}, true},
	}
	for _, tc := range cases {
		got, proceed := decideEnqueueLocked(tc.existing, tc.digest, tc.open, 64)
		if got != tc.want || proceed != tc.proceed {
			t.Errorf("%s: got (%+v, %v), want (%+v, %v)", tc.name, got, proceed, tc.want, tc.proceed)
		}
	}
}

// ── Fetch ────────────────────────────────────────────────────────────────

// TestFetchNeedsLock 钉住快路径条件(设计 §7.8 Fetch 第 0 步):没有未终结条目且没有 CursorAhead 才不加锁。
func TestFetchNeedsLock(t *testing.T) {
	cases := []struct {
		name           string
		open           uint32
		nextSeq        uint64
		decidedThrough uint64
		want           bool
	}{
		{"空流、水位在后 → 快路径", 0, 100, 99, false},
		{"空流、新玩家水位 0 → 快路径", 0, 1, 0, false},
		{"有未终结条目 → 加锁", 1, 100, 0, true},
		{"空流但水位 == next_seq(CursorAhead)→ 加锁抬号", 0, 100, 100, true},
		{"空流但水位领先 → 加锁抬号", 0, 100, 5000, true},
	}
	for _, tc := range cases {
		snap := &inboxpb.PlayerInboxStream{OpenCount: tc.open, NextSeq: tc.nextSeq}
		if got := fetchNeedsLock(snap, tc.decidedThrough); got != tc.want {
			t.Errorf("%s: fetchNeedsLock = %v, want %v", tc.name, got, tc.want)
		}
	}
}

// TestFetchStreamUpdate 钉住 Fetch 对流行的写回(设计 §7.8 Fetch 第 2、5 步):FIRST / ADVANCE 写围栏,
// CursorAhead 写抬过的 next_seq;MATCH 且没抬号不写(锁路径不做多余的写)。
func TestFetchStreamUpdate(t *testing.T) {
	cases := []struct {
		name      string
		v         inboxpb.PlayerInboxFenceVerdict
		raised    bool
		write     bool
		wantFence uint64
	}{
		{"MATCH 不抬号 → 不写", fenceMatch, false, false, 5},
		{"MATCH + CursorAhead → 只写 next_seq,围栏原样", fenceMatch, true, true, 5},
		{"FIRST → 围栏写成请求 epoch", fenceFirst, false, true, 7},
		{"ADVANCE + CursorAhead → 两样都写", fenceAdvance, true, true, 7},
	}
	for _, tc := range cases {
		write, fence, next := fetchStreamUpdate(tc.v, 5, 7, 900, tc.raised)
		if write != tc.write || (write && (fence != tc.wantFence || next != 900)) {
			t.Errorf("%s: got (write=%v fence=%d next=%d), want (write=%v fence=%d next=900)",
				tc.name, write, fence, next, tc.write, tc.wantFence)
		}
	}
}

// TestSplitPage 钉住 LIMIT n+1 读法的切页:多读的那一行只用来判断 has_more。
func TestSplitPage(t *testing.T) {
	rows := []int{1, 2, 3}
	if page, more := splitPage(rows, 2); len(page) != 2 || !more || page[1] != 2 {
		t.Errorf("读到 limit+1 行: page=%v more=%v", page, more)
	}
	if page, more := splitPage(rows, 3); len(page) != 3 || more {
		t.Errorf("恰好 limit 行: page=%v more=%v", page, more)
	}
	if page, more := splitPage(rows, 5); len(page) != 3 || more {
		t.Errorf("不足 limit 行: page=%v more=%v", page, more)
	}
	if page, more := splitPage[int](nil, 16); len(page) != 0 || more {
		t.Errorf("空: page=%v more=%v", page, more)
	}
	if page, more := splitPage(rows, -1); len(page) != 0 || !more {
		t.Errorf("负 limit 按 0: page=%v more=%v", page, more)
	}
}

// TestFetchScanBound 钉住 fetch_scanned_rows 的口径(设计 §4.2):读满 LIMIT 时数到最后读出那一行;
// 没读满说明整条流都走过了。数的是走过的行,不是返回的条数。
func TestFetchScanBound(t *testing.T) {
	rows := func(seqs ...uint64) []*inboxpb.PlayerInboxItemRow {
		out := make([]*inboxpb.PlayerInboxItemRow, 0, len(seqs))
		for _, s := range seqs {
			out = append(out, &inboxpb.PlayerInboxItemRow{Seq: s})
		}
		return out
	}
	if bound, whole := fetchScanBound(rows(10, 20, 30), 3); bound != 30 || whole {
		t.Errorf("读满 LIMIT: bound=%d whole=%v, want 30/false", bound, whole)
	}
	if _, whole := fetchScanBound(rows(10, 20), 3); !whole {
		t.Error("没读满:整条流都走过了,应 whole=true")
	}
	if _, whole := fetchScanBound(nil, 17); !whole {
		t.Error("一行 PENDING 都没读到(open_count>0 却没有 PENDING):整条流都走过了,应 whole=true")
	}
}

// ── Ack / 作废共用的 open_count ────────────────────────────────────────────

func TestSubtractOpenCount(t *testing.T) {
	if left, under := subtractOpenCount(5, 2); left != 3 || under {
		t.Errorf("5-2 = (%d, %v)", left, under)
	}
	if left, under := subtractOpenCount(2, 2); left != 0 || under {
		t.Errorf("2-2 = (%d, %v)", left, under)
	}
	if left, under := subtractOpenCount(1, 3); left != 0 || !under {
		t.Errorf("1-3 应钳到 0 并报 underflow,got (%d, %v)", left, under)
	}
}

// TestAckStreamUpdate 钉住 Ack 末尾对流行的写回(设计 §7.8 Ack):没终结也没推进围栏就不写;
// 终结了就减 open_count;FIRST / ADVANCE 写围栏;open_count 不够减钳到 0 并报告。
func TestAckStreamUpdate(t *testing.T) {
	cases := []struct {
		name      string
		v         inboxpb.PlayerInboxFenceVerdict
		open      uint32
		finalized uint32
		write     bool
		newOpen   uint32
		newFence  uint64
		underflow bool
	}{
		{"MATCH 且全是 ALREADY_SAME / 异常 → 不写", fenceMatch, 3, 0, false, 3, 5, false},
		{"MATCH 终结 2 条", fenceMatch, 3, 2, true, 1, 5, false},
		{"ADVANCE 但没终结 → 只推进围栏", fenceAdvance, 3, 0, true, 3, 9, false},
		{"FIRST 终结 1 条", fenceFirst, 1, 1, true, 0, 9, false},
		{"open_count 不够减 → 钳 0 并报告", fenceMatch, 1, 2, true, 0, 5, true},
	}
	for _, tc := range cases {
		write, newOpen, newFence, under := ackStreamUpdate(tc.v, tc.open, tc.finalized, 5, 9)
		if write != tc.write || under != tc.underflow || (write && (newOpen != tc.newOpen || newFence != tc.newFence)) {
			t.Errorf("%s: got (write=%v open=%d fence=%d underflow=%v), want (%v %d %d %v)",
				tc.name, write, newOpen, newFence, under, tc.write, tc.newOpen, tc.newFence, tc.underflow)
		}
	}
}

// TestAckMetricLabelAndAnomaly 钉住 ack_total{result} 的标签与「协议异常」集合:告警规则按标签正则取
// (deploy/k8s/player-inbox-alerts.yaml),SETTLED_UNKNOWN 必须单列,协议异常恰好是设计 §7.7 列的五种。
func TestAckMetricLabelAndAnomaly(t *testing.T) {
	settled := &inboxpb.PlayerInboxAckItem{Result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_FINALIZED, State: stateUnknown}
	if got := ackMetricLabel(settled); got != "finalized_settled_unknown" {
		t.Errorf("SETTLED_UNKNOWN 的标签 = %q", got)
	}
	applied := &inboxpb.PlayerInboxAckItem{Result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_FINALIZED, State: stateApplied}
	if got := ackMetricLabel(applied); got != "finalized" {
		t.Errorf("普通 FINALIZED 的标签 = %q", got)
	}
	// 非 FINALIZED 的结果即使带着 SETTLED_UNKNOWN 状态(终态比对)也按结果名记。
	same := &inboxpb.PlayerInboxAckItem{Result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_ALREADY_SAME, State: stateUnknown}
	if got := ackMetricLabel(same); got != "already_same" {
		t.Errorf("ALREADY_SAME 的标签 = %q", got)
	}

	anomalies := map[inboxpb.PlayerInboxAckResult]bool{}
	for v := range inboxpb.PlayerInboxAckResult_name {
		r := inboxpb.PlayerInboxAckResult(v)
		if r != inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_UNSPECIFIED && isAckAnomaly(r) {
			anomalies[r] = true
		}
	}
	want := []inboxpb.PlayerInboxAckResult{
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_CONFLICT,
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_UNKNOWN_SEQ,
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_NOT_DELIVERED,
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_SKIP_NOT_REQUESTED,
		inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_MISMATCH,
	}
	if len(anomalies) != len(want) {
		t.Fatalf("协议异常集合 = %v, want %v", anomalies, want)
	}
	for _, r := range want {
		if !anomalies[r] {
			t.Errorf("%v 应是协议异常", r)
		}
	}
}

// ── 运维接口 ─────────────────────────────────────────────────────────────

// TestVoidOutcomeOf 钉住作废的回包与指标路径(设计 §7.9):拒绝不记 void_total;已投递(含重复作废)回 void_requested。
func TestVoidOutcomeOf(t *testing.T) {
	cases := []struct {
		name   string
		action voidAction
		state  inboxpb.PlayerInboxItemState
		want   voidOutcome
	}{
		{"不存在", voidNotFound, inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_UNSPECIFIED, voidOutcome{tip: constants.ErrItemNotFound}},
		{"已是终态", voidTerminal, stateApplied, voidOutcome{tip: constants.ErrItemStateConflict, state: stateApplied}},
		{"从未投递 → 直接 VOIDED", voidDirect, stateVoided, voidOutcome{state: stateVoided, path: "direct"}},
		{"已投递 → 挂指令", voidDirective, statePending, voidOutcome{state: statePending, voidRequested: true, path: "directive"}},
		{"重复作废 → 幂等", voidAlreadyRequested, statePending, voidOutcome{state: statePending, voidRequested: true, path: "already"}},
	}
	for _, tc := range cases {
		if got := voidOutcomeOf(tc.action, tc.state); got != tc.want {
			t.Errorf("%s: got %+v, want %+v", tc.name, got, tc.want)
		}
	}
}

// TestSummarizeStreams 钉住 GetSummary 的组装:PENDING 聚合按域对上,max_fence_epoch 取各流最大值(epoch-raise 的来源,C4)。
func TestSummarizeStreams(t *testing.T) {
	streams := []*inboxpb.PlayerInboxStream{
		{Domain: 1, OpenCount: 2, NextSeq: 100, FenceEpoch: 9, NotifyDueMs: 5},
		{Domain: 2, OpenCount: 0, NextSeq: 7, FenceEpoch: 12},
	}
	aggs := map[uint32]pendingAggregate{1: {deliveredUnacked: 1, oldestCreatedMs: 1234}}
	resp := summarizeStreams(42, streams, aggs)
	if resp.GetPlayerId() != 42 || resp.GetMaxFenceEpoch() != 12 || len(resp.GetStreams()) != 2 {
		t.Fatalf("resp = %+v", resp)
	}
	s := resp.GetStreams()[0]
	if s.GetDomain() != 1 || s.GetOpenCount() != 2 || s.GetNextSeq() != 100 || s.GetFenceEpoch() != 9 ||
		s.GetDeliveredUnackedCount() != 1 || s.GetOldestPendingCreatedMs() != 1234 || s.GetNotifyDueMs() != 5 {
		t.Errorf("域 1 概况: %+v", s)
	}
	if s := resp.GetStreams()[1]; s.GetDeliveredUnackedCount() != 0 || s.GetOldestPendingCreatedMs() != 0 {
		t.Errorf("没有 PENDING 行的域聚合应为 0: %+v", s)
	}
	if empty := summarizeStreams(7, nil, nil); empty.GetPlayerId() != 7 || len(empty.GetStreams()) != 0 || empty.GetMaxFenceEpoch() != 0 {
		t.Errorf("没有流: %+v", empty)
	}
}

// ── 后台循环 ─────────────────────────────────────────────────────────────

// TestFoldScanGauges 钉住扫描型 gauge 的跨分片折叠(设计 §9.4):计数相加、最大投递次数取最大、
// 「最老」取各分片非 0 的最小时刻 —— 0 表示该分片没有这类行,绝不能被当成最老的时刻。
// 陈旧的已投递未确认行(评审 M4)只在 delivered_unacked_stale 里相加:分片读数里它们已被 SQL 排除在
// deliveredUnacked / maxDeliveryCount / minDeliveredMs 之外,折叠不得把它们加回告警口径。
func TestFoldScanGauges(t *testing.T) {
	const now = testNowMs
	shards := []shardGaugeScan{
		{}, // 空分片:全 0
		{openItems: 3, deliveredUnacked: 1, maxDeliveryCount: 2, minCreatedMs: now - 10_000, minDeliveredMs: now - 4_000, notifyBacklog: 1},
		{openItems: 2, deliveredUnacked: 2, maxDeliveryCount: 6, minCreatedMs: now - 60_000, minDeliveredMs: 0, notifyBacklog: 0},
		{openItems: 1, deliveredUnacked: 1, maxDeliveryCount: 1, minCreatedMs: now - 5_000, minDeliveredMs: now - 7_000, notifyBacklog: 2},
		// 只有陈旧行的分片:不影响告警口径的三项。
		{openItems: 4, deliveredUnackedStale: 4, minCreatedMs: now - 30_000},
		{openItems: 2, deliveredUnacked: 1, deliveredUnackedStale: 1, maxDeliveryCount: 3, minCreatedMs: now - 20_000, minDeliveredMs: now - 1_000},
	}
	got := foldScanGauges(now, shards)
	want := metrics.ScanGauges{
		OpenItems: 12, DeliveredUnacked: 5, DeliveredUnackedStale: 5, MaxDeliveryCount: 6, NotifyBacklog: 3,
		OldestOpenAgeSeconds: 60, DeliveredUnackedOldestSeconds: 7,
	}
	if got != want {
		t.Fatalf("fold = %+v, want %+v", got, want)
	}
	if empty := foldScanGauges(now, make([]shardGaugeScan, ShardCount)); empty != (metrics.ScanGauges{}) {
		t.Errorf("全空时所有 gauge 为 0,got %+v", empty)
	}
	// 时钟回拨(最老时刻晚于 now)不报负数年龄。
	if skew := foldScanGauges(now, []shardGaugeScan{{openItems: 1, minCreatedMs: now + 1000}}); skew.OldestOpenAgeSeconds != 0 {
		t.Errorf("时钟回拨时年龄应为 0,got %v", skew.OldestOpenAgeSeconds)
	}
}

func TestMinNonZero(t *testing.T) {
	cases := [][3]uint64{{0, 0, 0}, {0, 5, 5}, {5, 0, 5}, {3, 5, 3}, {5, 3, 3}}
	for _, c := range cases {
		if got := minNonZero(c[0], c[1]); got != c[2] {
			t.Errorf("minNonZero(%d, %d) = %d, want %d", c[0], c[1], got, c[2])
		}
	}
}

// TestNotifyRetryDueMs 钉住补发没落定时的退避(评审 L6):下一次 = now + 「已悬时长」,钳到 [30s, 30min]。
// 每次没落定都把已悬时长再等一遍,重试时刻按 2 倍递增;起算点为 0 或晚于 now(时钟回拨)时取下限。
func TestNotifyRetryDueMs(t *testing.T) {
	const now = testNowMs
	sec := func(n uint64) uint64 { return n * 1000 }
	cases := []struct {
		name  string
		since uint64
		want  uint64
	}{
		{"刚悬 5s:取下限 30s", now - sec(5), now + sec(30)},
		{"恰好 30s", now - sec(30), now + sec(30)},
		{"悬了 100s:再等 100s", now - sec(100), now + sec(100)},
		{"恰好 30min", now - sec(1800), now + sec(1800)},
		{"悬了 3 小时:封顶 30min", now - sec(3*3600), now + sec(1800)},
		{"起算点为 0:取下限", 0, now + sec(30)},
		{"起算点等于 now:取下限", now, now + sec(30)},
		{"时钟回拨(起算点晚于 now):取下限", now + sec(60), now + sec(30)},
	}
	for _, tc := range cases {
		if got := notifyRetryDueMs(now, tc.since); got != tc.want {
			t.Errorf("%s: notifyRetryDueMs = now%+d ms, want now%+d ms", tc.name, int64(got-now), int64(tc.want-now))
		}
	}
	// 连续失败的节拍:入箱 T0 起悬,补发循环第一次在 T0+5s 看到它,之后每次都在上次定下的时刻重试。
	since, at := now, now+sec(5)
	var gaps []uint64
	for i := 0; i < 8; i++ {
		next := notifyRetryDueMs(at, since)
		gaps = append(gaps, (next-at)/1000)
		at = next
	}
	want := []uint64{30, 35, 70, 140, 280, 560, 1120, 1800}
	for i := range want {
		if gaps[i] != want[i] {
			t.Fatalf("连续失败的退避(秒) = %v, want %v(按 2 倍递增、封顶 30 分钟)", gaps, want)
		}
	}
}

// ── 对账 ─────────────────────────────────────────────────────────────────
// 账本一律是真实形状(水位 = 环尾,见 reconcile_test.go 的 rcCursor);判定谓词的全表见 TestFinalizedLedgerGap。

// TestReconcileReport 钉住 ReconcilePlayer 的纯逻辑(设计 §9.3):逐行按该域游标(含最近结局环)判定、
// 管理员终结的行不比对、两边都没有的域不列游标。
// 账本的来历:消费方裁决过 10、12,随后 Redis 丢尾回到 10;12 在收件箱已是终态、不会再投,之后又裁决了 13、15。
// 所以环是 [10, 13, 15]、水位 15 —— 只比水位看不出 12 丢了(评审 M6)。13 已裁决、确认丢了,收件箱仍 PENDING。
func TestReconcileReport(t *testing.T) {
	const otherDomain = inboxpb.PlayerInboxDomain(2)
	rows := []*inboxpb.PlayerInboxItemRow{
		rcRow(10, 1, stateApplied, byConsumer), // 在环里:一致
		rcRow(12, 6, stateApplied, byConsumer), // 水位以内、环里查无此 seq:被之后的条目掩盖的丢失
		rcRow(13, 4, statePending, 0),          // PENDING 但水位已过:确认丢了
		rcRow(15, 7, stateApplied, byConsumer), // 在环里:一致
		rcRow(20, 2, stateApplied, byConsumer), // > 水位:账本没到
		rcRow(25, 3, stateVoided, byAdmin),     // 管理员作废:不比对
		rcRow(30, 5, statePending, 0),          // PENDING 且未处理:一致
	}
	ledger := rcLedger(rcCursor(gmDomain,
		rcDecision(10, 1, outApplied, 0), rcDecision(13, 4, outApplied, 0), rcDecision(15, 7, outApplied, 0)))
	streams := map[inboxpb.PlayerInboxDomain]*inboxpb.PlayerInboxStream{gmDomain: {Domain: uint32(gmDomain), NextSeq: 31}}
	cursors, diffs := reconcileReport([]inboxpb.PlayerInboxDomain{gmDomain, otherDomain}, streams, ledger, rows)

	if len(cursors) != 1 || cursors[0].GetDomain() != gmDomain || cursors[0].GetLedgerDecidedThrough() != 15 || cursors[0].GetInboxNextSeq() != 31 {
		t.Fatalf("cursors = %+v(没有流也没有水位的域 2 不列)", cursors)
	}
	final := inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_FINAL_NOT_IN_LEDGER
	decided := inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_PENDING_BUT_DECIDED
	want := []struct {
		seq, requestID uint64
		kind           inboxpb.PlayerInboxReconcileDiffKind
		state          inboxpb.PlayerInboxItemState
	}{
		{12, 6, final, stateApplied},
		{13, 4, decided, statePending},
		{20, 2, final, stateApplied},
	}
	if len(diffs) != len(want) {
		t.Fatalf("diffs = %+v, want %d", diffs, len(want))
	}
	for i, w := range want {
		if d := diffs[i]; d.GetDomain() != gmDomain || d.GetSeq() != w.seq || d.GetRequestId() != w.requestID ||
			d.GetKind() != w.kind || d.GetState() != w.state {
			t.Errorf("diffs[%d] = %+v, want seq=%d request_id=%d kind=%v state=%v", i, d, w.seq, w.requestID, w.kind, w.state)
		}
	}

	// 收件箱没有流、账本却有水位(例如流行所在库被恢复过):照样列出游标,next_seq 为 0。
	cursors, _ = reconcileReport([]inboxpb.PlayerInboxDomain{gmDomain}, nil, rcLedger(rcCursor(gmDomain, rcDecision(9, 1, outApplied, 0))), nil)
	if len(cursors) != 1 || cursors[0].GetInboxNextSeq() != 0 || cursors[0].GetLedgerDecidedThrough() != 9 {
		t.Errorf("只有账本水位的域: %+v", cursors)
	}
}

// TestLedgerMismatches 钉住定时对账的判定:与 ReconcilePlayer 同一个谓词(finalizedLedgerGap),逐行带出缺口种类,
// 顺序同输入;账本里没有该域按水位 0。
func TestLedgerMismatches(t *testing.T) {
	ledger := rcLedger(rcCursor(gmDomain, rcDecision(10, 10, outApplied, 0), rcDecision(15, 15, outApplied, 0)))
	otherDomain := rcRef(3, 3, stateApplied, byConsumer)
	otherDomain.Domain = inboxpb.PlayerInboxDomain(2)
	refs := []FinalizedRef{
		rcRef(10, 10, stateApplied, byConsumer), // 在环里:一致
		rcRef(12, 12, stateApplied, byConsumer), // 水位以内、环里查无此 seq
		rcRef(15, 15, stateApplied, byConsumer), // 在环里:一致
		rcRef(16, 16, stateApplied, byConsumer), // 水位没到
		otherDomain,                             // 账本无该域
	}
	got := ledgerMismatches(refs, ledger)
	want := []ledgerMismatch{
		{ref: refs[1], gap: gapMissingInRing},
		{ref: refs[3], gap: gapBeyondWatermark},
		{ref: refs[4], gap: gapBeyondWatermark},
	}
	if len(got) != len(want) {
		t.Fatalf("mismatches = %+v, want %+v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Errorf("mismatches[%d] = %+v, want %+v", i, got[i], want[i])
		}
	}
	if got := ledgerMismatches([]FinalizedRef{refs[0], refs[2]}, ledger); len(got) != 0 {
		t.Errorf("都在环里时不应有不一致: %+v", got)
	}
	if got := ledgerMismatches(nil, ledger); len(got) != 0 {
		t.Errorf("没有终结行: %+v", got)
	}
}

// TestRegistryDomainsSorted:运维对账逐域输出,顺序必须稳定(不随 map 遍历变)。
func TestRegistryDomainsSorted(t *testing.T) {
	r := mustRegistry(t)
	for i := 0; i < 20; i++ {
		ds := r.Domains()
		for j := 1; j < len(ds); j++ {
			if ds[j-1] >= ds[j] {
				t.Fatalf("Domains() 未按域号升序: %v", ds)
			}
		}
	}
	if got := r.Domains(); len(got) != 1 || got[0] != gmDomain {
		t.Errorf("v1 只登记了 GM 处置域: %v", got)
	}
}
