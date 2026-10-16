package inbox

import (
	"errors"
	"fmt"
)

// seqClockShift:seq 的时间下界 = 当前毫秒左移 12 位,即每毫秒可容纳 4096 个号,远大于 max_pending(设计 §6.2)。
const seqClockShift = 12

// maxSeqClockMs:左移之后仍不越过 2^63 的最大毫秒值。真实时钟约 2^41,超过它只可能是时钟坏了。
const maxSeqClockMs = uint64(1) << (63 - seqClockShift)

// ErrSeqClockInvalid 表示当前时钟不可用于 seq 分配(为 0 或大得离谱)。调用方按故障处理,不入箱。
var ErrSeqClockInvalid = errors.New("inbox: system clock unusable for seq allocation")

// NextSeq 在流行锁下分配下一个 seq:max(next_seq, now_ms << 12)(IB-6)。
//
//   - 同一条流内的单调性由 next_seq 保证(调用方随后写回 next_seq = seq + 1),不受时钟回拨影响;
//   - 时间下界让全局库按备份恢复之后,新号也不会落回恢复前已分配(并可能已被消费方处理)的号段上
//     —— 前提是恢复之后的墙钟晚于备份之后已分配号的时刻;时钟严重偏慢的极端情况由 IB-9 的回放核对
//     与 Fetch 的 CursorAhead 抬号兜底(设计 §6.2、§7.8)。
//
// nextSeq 为 0(不该出现:流行建出来就是 1)时按 1 处理,保证 seq 恒非 0。
func NextSeq(nextSeq, nowMs uint64) (uint64, error) {
	if nowMs == 0 || nowMs >= maxSeqClockMs {
		return 0, fmt.Errorf("%w: now_ms=%d", ErrSeqClockInvalid, nowMs)
	}
	return max(nextSeq, nowMs<<seqClockShift, 1), nil
}

// RaiseNextSeq 处理 CursorAhead(设计 §7.8 Fetch 第 2 步):消费方账本水位 ≥ next_seq 说明收件箱库被恢复过,
// 把 next_seq 抬到水位之上,只升不降。返回抬升后的值与是否发生了抬升。
func RaiseNextSeq(nextSeq, consumerDecidedThrough uint64) (uint64, bool) {
	if consumerDecidedThrough < nextSeq {
		return nextSeq, false
	}
	return consumerDecidedThrough + 1, true
}

// IsCursorAhead 报告消费方水位是否已经领先流的 next_seq(即 RaiseNextSeq 会抬号)。
func IsCursorAhead(nextSeq, consumerDecidedThrough uint64) bool {
	return consumerDecidedThrough >= nextSeq
}
