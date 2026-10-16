package inbox

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"

	inboxpb "proto/player_inbox"
)

// 本文件是存储层的纯判定函数:不碰库、不碰时钟,单测直接对着设计表断言(设计 §4.3、§6.2、§7.8、§7.9)。

// ContentSHA256 = sha256(msg_type ‖ issued_at_ms ‖ payload)(IB-2)。
// 整数按 big-endian 定长编码(msg_type 4 字节、issued_at_ms 8 字节),拼接无歧义;
// issued_at_ms 在摘要里,所以生产方刷新它重试会得到 CONFLICT(设计 §6.2 生产方契约)。
func ContentSHA256(t inboxpb.PlayerInboxMessageType, issuedAtMs uint64, payload []byte) []byte {
	h := sha256.New()
	var head [12]byte
	binary.BigEndian.PutUint32(head[0:4], uint32(t))
	binary.BigEndian.PutUint64(head[4:12], issuedAtMs)
	h.Write(head[:])
	h.Write(payload)
	return h.Sum(nil)
}

// IsTerminalState 报告条目状态是否为终态(只有 PENDING 不是)。
func IsTerminalState(s inboxpb.PlayerInboxItemState) bool {
	switch s {
	case inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_APPLIED,
		inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_REJECTED,
		inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_VOIDED,
		inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_SETTLED_UNKNOWN:
		return true
	default:
		return false
	}
}

// existingVerdict 是幂等键已存在时的结论:同摘要 = DUPLICATE,不同 = CONFLICT。
type existingVerdict uint8

const (
	existingDuplicate existingVerdict = iota + 1
	existingConflict
)

// decideExisting 比对已存在行的内容摘要与本次请求的摘要(IB-2)。
func decideExisting(stored, requested []byte) existingVerdict {
	if len(stored) == sha256.Size && bytes.Equal(stored, requested) {
		return existingDuplicate
	}
	return existingConflict
}

// outcomeToState 把消费方报告的结局映射成终态。非法结局返回 UNSPECIFIED。
func outcomeToState(o inboxpb.PlayerInboxOutcome) inboxpb.PlayerInboxItemState {
	switch o {
	case inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_APPLIED:
		return inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_APPLIED
	case inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_REJECTED:
		return inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_REJECTED
	case inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_SKIPPED:
		return inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_VOIDED
	case inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_UNKNOWN:
		return inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_SETTLED_UNKNOWN
	default:
		return inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_UNSPECIFIED
	}
}

// ackDecision 是单条 Ack 报告的判定结果。
type ackDecision struct {
	result inboxpb.PlayerInboxAckResult
	// finalize 为真时把行从 PENDING 改成 newState(写 finalized_by=CONSUMER、decided_epoch、outcome_reason)。
	finalize bool
	newState inboxpb.PlayerInboxItemState
	// state 是处理后的行状态(回给消费方);行不存在时为 UNSPECIFIED。
	state inboxpb.PlayerInboxItemState
}

// decideAck 按设计 §7.8 的 Ack 表判定一条报告。row 为 nil 表示行不存在。
// 报告本身的合法性(seq 非 0、结局已知、升序)由调用方先校验。
//
//	| 行的现状                   | 报告                                   | 结果                       |
//	| 不存在                     | 任意                                   | UNKNOWN_SEQ(告警,不改)    |
//	| request_id 与报告不一致    | 任意                                   | MISMATCH(告警,不改;IB-9) |
//	| PENDING 且 delivery_count=0 | 任意                                  | NOT_DELIVERED(告警,不改)  |
//	| PENDING,没有指令           | SKIPPED                                | SKIP_NOT_REQUESTED(告警)   |
//	| PENDING                    | APPLIED / REJECTED / 有指令时的 SKIPPED / UNKNOWN | 改成对应终态,FINALIZED |
//	| 终态,与报告一致            | —                                      | ALREADY_SAME               |
//	| 终态,与报告不一致          | —                                      | CONFLICT(告警,永不翻转;IB-4) |
//
// 挂了作废指令、消费方却报 APPLIED:照收 APPLIED(管理员的取消晚了一步,以消费方已落盘的事实为准)。
// 「一致」= 映射后的终态相同;REJECTED 另要求原因码相同(消费方回放的是账本里的原结局,原因码必然相同)。
func decideAck(row *inboxpb.PlayerInboxItemRow, report *inboxpb.PlayerInboxDecisionReport) ackDecision {
	if row == nil {
		return ackDecision{result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_UNKNOWN_SEQ}
	}
	state := inboxpb.PlayerInboxItemState(row.GetState())
	if row.GetRequestId() != report.GetRequestId() {
		return ackDecision{result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_MISMATCH, state: state}
	}
	want := outcomeToState(report.GetOutcome())
	if state == inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING {
		if row.GetDeliveryCount() == 0 {
			return ackDecision{result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_NOT_DELIVERED, state: state}
		}
		if report.GetOutcome() == inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_SKIPPED &&
			inboxpb.PlayerInboxDirective(row.GetDirective()) != inboxpb.PlayerInboxDirective_PLAYER_INBOX_DIRECTIVE_VOID {
			return ackDecision{result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_SKIP_NOT_REQUESTED, state: state}
		}
		return ackDecision{
			result:   inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_FINALIZED,
			finalize: true,
			newState: want,
			state:    want,
		}
	}
	if state == want {
		if want != inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_REJECTED || row.GetOutcomeReason() == report.GetReason() {
			return ackDecision{result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_ALREADY_SAME, state: state}
		}
	}
	return ackDecision{result: inboxpb.PlayerInboxAckResult_PLAYER_INBOX_ACK_CONFLICT, state: state}
}

// ackOutcomeReason 是终结时写进 outcome_reason 的值:REJECTED 写消费方给的 tip id;
// SKIPPED 写消费方回报的作废原因(与行上 void_reason 同源);其余为 0。
func ackOutcomeReason(report *inboxpb.PlayerInboxDecisionReport) uint32 {
	switch report.GetOutcome() {
	case inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_REJECTED, inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_SKIPPED:
		return report.GetReason()
	default:
		return 0
	}
}

// voidAction 是运维作废一条条目的判定结果(设计 §7.9、IB-5)。
type voidAction uint8

const (
	// voidNotFound:条目不存在。
	voidNotFound voidAction = iota + 1
	// voidTerminal:已是终态 → kPlayerInboxItemStateConflict。
	voidTerminal
	// voidDirect:从未投递 → 在流行锁下直接改 VOIDED(finalized_by=ADMIN),载荷从未离开过收件箱。
	voidDirect
	// voidDirective:已投递 → 只挂 directive=VOID,由消费方裁决。
	voidDirective
	// voidAlreadyRequested:已投递且已挂过 VOID 指令 → 幂等成功,不改行。
	voidAlreadyRequested
)

// decideVoid 判定作废动作。
func decideVoid(row *inboxpb.PlayerInboxItemRow) voidAction {
	if row == nil {
		return voidNotFound
	}
	if inboxpb.PlayerInboxItemState(row.GetState()) != inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING {
		return voidTerminal
	}
	if row.GetDeliveryCount() == 0 {
		return voidDirect
	}
	if inboxpb.PlayerInboxDirective(row.GetDirective()) == inboxpb.PlayerInboxDirective_PLAYER_INBOX_DIRECTIVE_VOID {
		return voidAlreadyRequested
	}
	return voidDirective
}

// validateAckReports 校验一次 Ack 的报告列表:1..AckMax 条、seq 非 0 且严格升序、结局已知。
func validateAckReports(reports []*inboxpb.PlayerInboxDecisionReport, maxReports int) bool {
	if len(reports) == 0 || len(reports) > maxReports {
		return false
	}
	var prev uint64
	for _, r := range reports {
		if r == nil || r.GetSeq() == 0 || r.GetSeq() <= prev {
			return false
		}
		if outcomeToState(r.GetOutcome()) == inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_UNSPECIFIED {
			return false
		}
		prev = r.GetSeq()
	}
	return true
}

// clampFetchItems 把请求的 max_items 归一到 [1, max]:0 取默认值。
func clampFetchItems(requested uint32, def, maxItems int) int {
	if requested == 0 {
		return def
	}
	return min(int(requested), maxItems)
}

// reconcileDiff 判定一条收件箱行与已落盘账本水位之间的差异(设计 §9.3):
//   - 收件箱已是消费方终结的终态,但账本水位没到这个 seq → FINAL_NOT_IN_LEDGER(回档或 Redis 丢数据);
//   - 收件箱仍 PENDING,但账本水位已过这个 seq → PENDING_BUT_DECIDED(确认丢了且玩家一直离线)。
//
// 管理员直接作废的行(finalized_by=ADMIN,从未投递)不在消费方账本里,不比对。
func reconcileDiff(row *inboxpb.PlayerInboxItemRow, decidedThrough uint64) inboxpb.PlayerInboxReconcileDiffKind {
	state := inboxpb.PlayerInboxItemState(row.GetState())
	switch {
	case state == inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING:
		if row.GetSeq() <= decidedThrough {
			return inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_PENDING_BUT_DECIDED
		}
	case IsTerminalState(state) &&
		inboxpb.PlayerInboxFinalizer(row.GetFinalizedBy()) == inboxpb.PlayerInboxFinalizer_PLAYER_INBOX_FINALIZER_CONSUMER:
		if row.GetSeq() > decidedThrough {
			return inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_FINAL_NOT_IN_LEDGER
		}
	}
	return inboxpb.PlayerInboxReconcileDiffKind_PLAYER_INBOX_RECONCILE_DIFF_UNSPECIFIED
}
