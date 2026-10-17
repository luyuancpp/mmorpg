package inbox

// VoidItem 强制模式(mode=FORCE_MISMATCH,设计 §7.9,评审 M1)的纯判定与入口校验单测。
// 锁内读写与审计只有 store_integration_test.go 的 TestIntegrationForceVoid(-tags integration + 真库)覆盖。
//
// 回归目标:scene 判 MISMATCH 之后,已投递的行只能挂 VOID 指令,而 scene 先做回放核对、后看指令,
// 该域每次登录都会再次停用。强制模式只在「已落盘账本证明本行从未执行」时终结它;
// 回放(同 seq 同 request_id)与 UNKNOWN(环里没有该 seq)都可能已执行,必须拒绝(IB-4 / IB-9)。

import (
	"context"
	"errors"
	"testing"

	"player_inbox/internal/constants"

	componentpb "proto/common/component"
	inboxpb "proto/player_inbox"

	"github.com/zeromicro/go-zero/core/logx/logtest"
)

const (
	fvSeq       = uint64(20)
	fvRequestID = uint64(2)
	// fvOtherRequestID 是消费方在 fvSeq 上实际裁决的那一条的 request_id(恢复回来的旧行与账本对不上)。
	fvOtherRequestID = uint64(999)
)

// fvRow 是一条已投递、仍 PENDING 的行:seq=20,request_id=2。
func fvRow() *inboxpb.PlayerInboxItemRow {
	return &inboxpb.PlayerInboxItemRow{
		Domain: uint32(gmDomain), Seq: fvSeq, RequestId: fvRequestID, MsgType: uint32(gmBlock),
		State: uint32(statePending), DeliveryCount: 2,
	}
}

// fvDecision 是环里一条真实形状的结局(APPLIED、类型同本行)。
func fvDecision(seq, requestID uint64) *componentpb.PlayerInboxDecision {
	return &componentpb.PlayerInboxDecision{Seq: seq, Outcome: uint32(outApplied), RequestId: requestID, MsgType: uint32(gmBlock)}
}

func fvCursor(domain inboxpb.PlayerInboxDomain, decidedThrough uint64, recent ...*componentpb.PlayerInboxDecision) *componentpb.PlayerInboxDomainCursor {
	return &componentpb.PlayerInboxDomainCursor{Domain: uint32(domain), DecidedThroughSeq: decidedThrough, Recent: recent}
}

func fvLedger(cursors ...*componentpb.PlayerInboxDomainCursor) *componentpb.PlayerInboxLedgerComp {
	return &componentpb.PlayerInboxLedgerComp{Cursors: cursors}
}

// fvProvingLedger:水位 30,环 10/20/30,其中 seq=20 记的是另一个 request_id —— 证明成立的典型形状。
func fvProvingLedger() *componentpb.PlayerInboxLedgerComp {
	return fvLedger(fvCursor(gmDomain, 30, fvDecision(10, 1), fvDecision(fvSeq, fvOtherRequestID), fvDecision(30, 3)))
}

// TestProveNeverExecuted 钉住强制终结的账本证明:只有「水位已过 + 环里同 seq 是另一个 request_id」放行,
// 其余每一种都拒绝,并给出可区分的原因。
func TestProveNeverExecuted(t *testing.T) {
	undelivered := fvRow()
	undelivered.DeliveryCount = 0

	mismatchedType := fvDecision(fvSeq, fvRequestID)
	mismatchedType.MsgType = uint32(gmUnblock)
	unknownOutcome := fvDecision(fvSeq, fvOtherRequestID)
	unknownOutcome.Outcome = uint32(outUnknown)

	cases := []struct {
		name   string
		row    *inboxpb.PlayerInboxItemRow
		ledger *componentpb.PlayerInboxLedgerComp
		found  bool
		want   inboxpb.PlayerInboxForceVoidVerdict
	}{
		{"环里同 seq 是另一个 request_id → 证明成立", fvRow(), fvProvingLedger(), true, forceVoidProven},
		{"水位恰好等于 seq、它是环尾 → 证明成立", fvRow(),
			fvLedger(fvCursor(gmDomain, fvSeq, fvDecision(10, 1), fvDecision(fvSeq, fvOtherRequestID))), true, forceVoidProven},
		{"别的域的游标不参与证明(那里出现本行的 seq / request_id 也无关)", fvRow(),
			fvLedger(fvCursor(2, 30, fvDecision(fvSeq, fvRequestID)), fvCursor(gmDomain, 30, fvDecision(fvSeq, fvOtherRequestID))),
			true, forceVoidProven},

		{"从未投递 → 改用普通模式", undelivered, fvProvingLedger(), true, forceVoidNotDelivered},
		{"读不到存档", fvRow(), nil, false, forceVoidLedgerAbsent},
		{"found=true 却没给账本也按缺失", fvRow(), nil, true, forceVoidLedgerAbsent},
		{"found=false 时账本内容不作数", fvRow(), fvProvingLedger(), false, forceVoidLedgerAbsent},

		{"账本没有该域", fvRow(), fvLedger(fvCursor(2, 30, fvDecision(fvSeq, fvOtherRequestID))), true, forceVoidNotDecided},
		{"水位没到这个 seq", fvRow(), fvLedger(fvCursor(gmDomain, fvSeq-1, fvDecision(10, 1))), true, forceVoidNotDecided},

		{"回放:同 seq 同 request_id(可能已执行)", fvRow(),
			fvLedger(fvCursor(gmDomain, 30, fvDecision(10, 1), fvDecision(fvSeq, fvRequestID))), true, forceVoidRequestInLedger},
		{"同 seq 同 request_id 但类型不同:scene 也判 MISMATCH,但不能证明没执行", fvRow(),
			fvLedger(fvCursor(gmDomain, 30, mismatchedType)), true, forceVoidRequestInLedger},
		{"本行 request_id 出现在环里别的 seq 上", fvRow(),
			fvLedger(fvCursor(gmDomain, 30, fvDecision(10, fvRequestID), fvDecision(fvSeq, fvOtherRequestID))), true, forceVoidRequestInLedger},

		{"水位已过、环里没有该 seq(UNKNOWN 一类)", fvRow(),
			fvLedger(fvCursor(gmDomain, 30, fvDecision(25, 5), fvDecision(30, 3))), true, forceVoidSeqNotInRing},
		{"水位已过、环为空", fvRow(), fvLedger(fvCursor(gmDomain, 30)), true, forceVoidSeqNotInRing},

		{"同一个域两条游标", fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(fvSeq, fvOtherRequestID)), fvCursor(gmDomain, 30)),
			true, forceVoidLedgerInvalid},
		{"环非严格升序", fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(fvSeq, fvOtherRequestID), fvDecision(10, 1))),
			true, forceVoidLedgerInvalid},
		{"环里同 seq 两条", fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(fvSeq, fvOtherRequestID), fvDecision(fvSeq, 998))),
			true, forceVoidLedgerInvalid},
		{"环里 seq 为 0", fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(0, 5), fvDecision(fvSeq, fvOtherRequestID))),
			true, forceVoidLedgerInvalid},
		{"环越过水位", fvRow(), fvLedger(fvCursor(gmDomain, fvSeq, fvDecision(fvSeq, fvOtherRequestID), fvDecision(30, 3))),
			true, forceVoidLedgerInvalid},
		{"环里结局不可记(UNKNOWN 不入环)", fvRow(), fvLedger(fvCursor(gmDomain, 30, unknownOutcome)), true, forceVoidLedgerInvalid},
		{"环里 request_id 为 0:不得当成「另一个 request_id」", fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(fvSeq, 0))),
			true, forceVoidLedgerInvalid},
		{"环坏了,即使水位没到也报坏", fvRow(), fvLedger(fvCursor(gmDomain, 15, fvDecision(10, 1), fvDecision(5, 4))),
			true, forceVoidLedgerInvalid},
		{"别的域重复:整份判坏(scene 侧同样整份判坏)", fvRow(),
			fvLedger(fvCursor(2, 30), fvCursor(2, 40), fvCursor(gmDomain, 30, fvDecision(fvSeq, fvOtherRequestID))), true, forceVoidLedgerInvalid},
		{"别的域的环坏了:整份判坏", fvRow(),
			fvLedger(fvCursor(2, 5, fvDecision(9, 7)), fvCursor(gmDomain, 30, fvDecision(fvSeq, fvOtherRequestID))), true, forceVoidLedgerInvalid},
		{"游标 domain 为 0", fvRow(),
			fvLedger(fvCursor(0, 30), fvCursor(gmDomain, 30, fvDecision(fvSeq, fvOtherRequestID))), true, forceVoidLedgerInvalid},
	}
	seen := map[inboxpb.PlayerInboxForceVoidVerdict]bool{}
	for _, tc := range cases {
		got := proveNeverExecuted(tc.row, tc.ledger, tc.found)
		seen[got.verdict] = true
		if got.verdict != tc.want {
			t.Errorf("%s: verdict = %v, want %v", tc.name, got.verdict, tc.want)
		}
	}
	// 每一个结论值都至少有一个用例走到(UNSPECIFIED 只属于普通模式与锁内的不存在 / 终态):
	// proto 新增结论却没有判定分支时,这里会红。
	for v := range inboxpb.PlayerInboxForceVoidVerdict_name {
		verdict := inboxpb.PlayerInboxForceVoidVerdict(v)
		if verdict != forceVoidUnspecified && !seen[verdict] {
			t.Errorf("结论 %v 没有任何用例走到", verdict)
		}
	}
}

// TestProveNeverExecutedEvidence:证明的两个读数进日志留证 —— 水位与环里同 seq 那一条的 request_id。
func TestProveNeverExecutedEvidence(t *testing.T) {
	got := proveNeverExecuted(fvRow(), fvProvingLedger(), true)
	want := forceVoidProof{verdict: forceVoidProven, decidedThrough: 30, ringRequestID: fvOtherRequestID}
	if got != want {
		t.Fatalf("proof = %+v, want %+v", got, want)
	}
	notDecided := proveNeverExecuted(fvRow(), fvLedger(fvCursor(gmDomain, 19)), true)
	if notDecided != (forceVoidProof{verdict: forceVoidNotDecided, decidedThrough: 19}) {
		t.Fatalf("水位没到时只留水位读数: %+v", notDecided)
	}
	replay := proveNeverExecuted(fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(fvSeq, fvRequestID))), true)
	if replay != (forceVoidProof{verdict: forceVoidRequestInLedger, decidedThrough: 30, ringRequestID: fvRequestID}) {
		t.Fatalf("回放时环里同 seq 的 request_id 就是本行: %+v", replay)
	}
}

// TestDecideForceVoidLocked 钉住锁内分流:不存在 / 终态沿用普通模式的结论且不看账本;PENDING 按证明分流。
func TestDecideForceVoidLocked(t *testing.T) {
	terminal := fvRow()
	terminal.State = uint32(stateVoided)
	terminal.FinalizedBy = uint32(inboxpb.PlayerInboxFinalizer_PLAYER_INBOX_FINALIZER_ADMIN)

	cases := []struct {
		name    string
		row     *inboxpb.PlayerInboxItemRow
		ledger  *componentpb.PlayerInboxLedgerComp
		action  voidAction
		verdict inboxpb.PlayerInboxForceVoidVerdict
	}{
		{"行不存在", nil, fvProvingLedger(), voidNotFound, forceVoidUnspecified},
		{"已是终态(含已强制终结过):不看账本", terminal, fvProvingLedger(), voidTerminal, forceVoidUnspecified},
		{"PENDING 且证明成立 → 终结", fvRow(), fvProvingLedger(), voidForced, forceVoidProven},
		{"PENDING 但回放 → 拒绝", fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(fvSeq, fvRequestID))), voidForceRefused, forceVoidRequestInLedger},
		{"PENDING 但 UNKNOWN → 拒绝", fvRow(), fvLedger(fvCursor(gmDomain, 30, fvDecision(30, 3))), voidForceRefused, forceVoidSeqNotInRing},
	}
	for _, tc := range cases {
		action, proof := decideForceVoidLocked(tc.row, tc.ledger, true)
		if action != tc.action || proof.verdict != tc.verdict {
			t.Errorf("%s: got (%v, %v), want (%v, %v)", tc.name, action, proof.verdict, tc.action, tc.verdict)
		}
	}
}

// TestVoidOutcomeOfForce:强制终结记 void_total{path="force"};证明不成立回 StateConflict、行仍 PENDING、不记指标。
func TestVoidOutcomeOfForce(t *testing.T) {
	if got := voidOutcomeOf(voidForced, stateVoided); got != (voidOutcome{state: stateVoided, path: "force"}) {
		t.Errorf("强制终结: %+v", got)
	}
	if got := voidOutcomeOf(voidForceRefused, statePending); got != (voidOutcome{tip: constants.ErrItemStateConflict, state: statePending}) {
		t.Errorf("证明不成立: %+v", got)
	}
}

// fvLedgers 是记录调用次数的账本读取器替身。
type fvLedgers struct {
	calls  int
	ledger *componentpb.PlayerInboxLedgerComp
	found  bool
	err    error
}

func (l *fvLedgers) ReadInboxLedger(ctx context.Context, _ uint64) (*componentpb.PlayerInboxLedgerComp, bool, error) {
	l.calls++
	if _, ok := ctx.Deadline(); !ok {
		return nil, false, errors.New("ledger read without deadline")
	}
	return l.ledger, l.found, l.err
}

// TestVoidForceRejectsBeforeTouchingStore 钉住进事务之前的拒绝(Store 不带库:走到事务就会 panic,
// 所以这里每条路径都证明了「没碰库、没改任何状态」):
//   - mode 越界 → InvalidParameter,不读账本;
//   - 强制模式 note 必填 → InvalidParameter,不读账本;
//   - 没装账本读取器 → 故障(适配层折成 StoreUnavailable);
//   - 读账本失败 → 故障,可重试。
func TestVoidForceRejectsBeforeTouchingStore(t *testing.T) {
	logtest.Discard(t)
	s := &Store{registry: mustRegistry(t)}
	ctx := context.Background()
	base := VoidParams{
		PlayerID: 42, Domain: gmDomain, Seq: fvSeq, Reason: inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_QUARANTINED,
		Operator: "ops_it", Note: "工单 #1", PeerAddr: "127.0.0.1:1",
		Mode: inboxpb.PlayerInboxVoidMode_PLAYER_INBOX_VOID_MODE_FORCE_MISMATCH,
	}

	badMode := base
	badMode.Mode = inboxpb.PlayerInboxVoidMode(99)
	l := &fvLedgers{ledger: fvProvingLedger(), found: true}
	if resp, err := s.Void(ctx, badMode, l); err != nil || resp.GetErrorMessage().GetId() != constants.ErrInvalidParameter || l.calls != 0 {
		t.Fatalf("mode 越界: resp=%+v err=%v ledger_calls=%d", resp, err, l.calls)
	}

	noNote := base
	noNote.Note = ""
	if resp, err := s.Void(ctx, noNote, l); err != nil || resp.GetErrorMessage().GetId() != constants.ErrInvalidParameter || l.calls != 0 {
		t.Fatalf("强制模式 note 必填: resp=%+v err=%v ledger_calls=%d", resp, err, l.calls)
	}

	if resp, err := s.Void(ctx, base, nil); err == nil {
		t.Fatalf("没装账本读取器应返回故障,got resp=%+v", resp)
	}

	failing := &fvLedgers{err: errors.New("data_service down")}
	if resp, err := s.Void(ctx, base, failing); err == nil || failing.calls != 1 {
		t.Fatalf("读账本失败应返回故障(不进事务): resp=%+v err=%v calls=%d", resp, err, failing.calls)
	}
}
