package assetop

import (
	"context"
	"fmt"
	"sort"

	"github.com/zeromicro/go-zero/core/logx"

	assetpb "proto/common/asset"
)

// 日对账(只读):docs/design/landing-recovery-sync-digest.md §6.2。
//
// 每 AuditInterval(默认 24 小时)把全部 PENDING 行按「为什么还没终结」分成四类计数,刷新两组按类 gauge,
// 并打一行汇总日志(只有计数与年龄,不列 player_id)。**不改任何行**。
//
// 实时告警仍靠计数器:assetop_unknown_total、assetop_outcome_flip_total、
// assetop_store_errors_total{op="decode"}。对账给的是「积压长什么样」的日视图,以及按类的最老年龄 ——
// 卡死告警只该看 online_retry:offline 与 abandon_candidate 本来就会很老,混在一起的「最老未决行年龄」
// (assetop_pending_oldest_age_seconds)在有离线玩家时恒为真,告警形同虚设。

// PendingClass 是一行 PENDING 在日对账里的类别,同时是 class label 的唯一出处。
type PendingClass uint8

const (
	// PendingOnlineRetry 正常重试中:最近一次结局不是本地合成的 NOT_HERE,也不是告警类。
	PendingOnlineRetry PendingClass = iota + 1
	// PendingOffline 最近一次结局是本地合成的 NOT_HERE:玩家不在线(行多半在停放档里等他回来),
	// 或有位置记录却暂时找不到持有节点(列里分不出这两种,后者不停放,见 Result.Absent)。
	PendingOffline
	// PendingAlert UNKNOWN、坏流号、投过之后成了毒行:要人看。
	PendingAlert
	// PendingAbandonCandidate 满足自动放弃的时间条件(created_ms 早于 now − AbandonAfter)。
	PendingAbandonCandidate

	pendingClassCount
)

var pendingClassNames = [...]string{"invalid", "online_retry", "offline", "alert", "abandon_candidate"}

var _ = [1]struct{}{}[len(pendingClassNames)-int(pendingClassCount)]

// allPendingClasses 是刷新 gauge 时逐类置值的顺序;长度必须正好覆盖全部合法类别。
var allPendingClasses = [...]PendingClass{PendingOnlineRetry, PendingOffline, PendingAlert, PendingAbandonCandidate}

var _ = [1]struct{}{}[len(allPendingClasses)+1-int(pendingClassCount)]

// String 返回 label 值;越界回 "invalid"。
func (c PendingClass) String() string {
	if c >= pendingClassCount {
		return pendingClassNames[0]
	}
	return pendingClassNames[c]
}

func (c PendingClass) valid() bool { return c > 0 && c < pendingClassCount }

// PendingFacts 是给一行 PENDING 分类所需的全部列,Store 用非加锁读取出。
type PendingFacts struct {
	Stream      assetpb.AssetOpStream
	CreatedMs   uint64
	Attempts    uint32
	LastOutcome assetpb.AssetOpOutcome
	LastReason  uint32
}

// ClassifyPending 判一行 PENDING 属于哪一类。四类互斥,判断顺序就是优先级:
//
//  1. alert —— Attempts > 0 且 last_outcome = UNKNOWN。scene 回 UNKNOWN(配置不一致、纪元过期、跳号、
//     验签失败)、坏流号重排、投过之后成了毒行,写进行里的都是 UNKNOWN。排在最前:告警类不能因为
//     又老又离线就被归进别的类而不响。新插入、还没投过的行 last_outcome 也是 0(UNKNOWN),
//     所以必须带 Attempts > 0。
//     传输失败(没拿到 scene 任何答复)**不**在这一类:ProcessOne 落库时把它写成 RETRY + 原因码 0
//     (reconcile.go rescheduleTransportFailure),归 online_retry —— 否则对账那一刻 scene 短暂不可达,
//     这一类就会误报一整天。滚动升级前由旧版写下的传输失败行仍是 UNKNOWN,下一次投递后自然改写。
//     结局翻转与首投即毒行(Attempts 仍为 0)不在这一类,它们各有实时计数器。
//  2. abandon_candidate —— created_ms 早于 abandonBeforeMs(= now − AbandonAfter,由循环算好;
//     0 表示不分出这一类)。这类由自动放弃处理,计数只供观察。
//  3. offline —— 最近一次结局是**本地合成**的 NOT_HERE。列里只存了结局与原因,判据是
//     last_outcome = NOT_HERE 且 last_reason != ReasonPlayerNotHere:scene 自己回 NOT_HERE 时一律带
//     kAssetPlayerNotHere(asset_op_system.cpp),本地合成的不带原因(或带粘性的 27007,见 carryPartialReason)。
//  4. online_retry —— 其余:RETRY(含传输失败)、scene 回的 NOT_HERE、未落盘的 APPLIED / REJECTED、
//     还没投过的新行。
//
// 纯函数。guild 与 trade 的 Store 都调它,口径只此一处;不要在 SQL 里另写一份 CASE。
func ClassifyPending(f PendingFacts, abandonBeforeMs uint64) PendingClass {
	switch {
	case f.Attempts > 0 && f.LastOutcome == assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_UNKNOWN:
		return PendingAlert
	case abandonBeforeMs != 0 && f.CreatedMs < abandonBeforeMs:
		return PendingAbandonCandidate
	case f.LastOutcome == assetpb.AssetOpOutcome_ASSET_OP_OUTCOME_NOT_HERE && f.LastReason != ReasonPlayerNotHere:
		return PendingOffline
	default:
		return PendingOnlineRetry
	}
}

// ClassCount 是一个 (流, 类别) 组合的计数。
type ClassCount struct {
	Stream assetpb.AssetOpStream
	Class  PendingClass
	Rows   uint64
	// OldestAgeMs 是这一组里最老一行的年龄(now − min(created_ms)),毫秒;Rows 为 0 时为 0。
	OldestAgeMs uint64
}

// PendingClassCounter 是 Store 的**可选**能力:日对账。没实现它,AuditOnce 只打一次 INFO。
type PendingClassCounter interface {
	// CountPendingByClass 非加锁读全部 PENDING 行,按 (stream, class) 计数。分类必须用 ClassifyPending
	// (逐行扫描时可用 PendingTally 累加);abandonBeforeMs 由循环按 AbandonAfter 算好传入
	// (0 = 不分出 abandon_candidate),Store 不另存放弃门槛。只需回出现过的组合,没回的由循环置 0。
	// 行数多时按主键分页读,不要一条大查询长时间占着连接。
	CountPendingByClass(ctx context.Context, nowMs, abandonBeforeMs uint64) ([]ClassCount, error)
}

// pendingKey 是 (流, 类别) 组合。
type pendingKey struct {
	stream assetpb.AssetOpStream
	class  PendingClass
}

// PendingTally 把逐行分类累加成 []ClassCount,给按行扫描的 Store 实现用。不并发安全。
type PendingTally struct {
	nowMs           uint64
	abandonBeforeMs uint64
	byKey           map[pendingKey]*ClassCount
}

// NewPendingTally 建累加器;两个时刻原样取自 CountPendingByClass 的入参。
func NewPendingTally(nowMs, abandonBeforeMs uint64) *PendingTally {
	return &PendingTally{nowMs: nowMs, abandonBeforeMs: abandonBeforeMs, byKey: map[pendingKey]*ClassCount{}}
}

// Add 分类并累加一行。created_ms 晚于 now(时钟回拨、跨机时差)的行年龄按 0 计。
func (t *PendingTally) Add(f PendingFacts) {
	class := ClassifyPending(f, t.abandonBeforeMs)
	key := pendingKey{stream: f.Stream, class: class}
	c, ok := t.byKey[key]
	if !ok {
		c = &ClassCount{Stream: f.Stream, Class: class}
		t.byKey[key] = c
	}
	c.Rows++
	if t.nowMs > f.CreatedMs && t.nowMs-f.CreatedMs > c.OldestAgeMs {
		c.OldestAgeMs = t.nowMs - f.CreatedMs
	}
}

// Counts 返回累加结果,按 (流, 类别) 排序,输出稳定便于测试与日志对照。
func (t *PendingTally) Counts() []ClassCount {
	out := make([]ClassCount, 0, len(t.byKey))
	for _, c := range t.byKey {
		out = append(out, *c)
	}
	sort.Slice(out, func(i, j int) bool {
		if out[i].Stream != out[j].Stream {
			return out[i].Stream < out[j].Stream
		}
		return out[i].Class < out[j].Class
	})
	return out
}

// AuditOnce 跑一轮日对账:读按类计数 → 刷新按类 gauge → 打一行汇总日志。不改任何行。
//
// 本服务独占的每条流(LoopConfig.Streams)× 四类都会被置值,没回的组合置 0 —— 否则某一类清空之后
// gauge 会停在旧值上一直告警。Store 回了不在 Streams 里的流(配置漏写)也照样报出来,不吞;
// 这类组合记在 auditExtra 里,下一轮没再出现时同样置 0。逐个置 0 而不是先 Reset 再全量置值:
// Reset 与重新置值之间被抓取到的空序列会让 `for: 30m` 的告警重新计时。
// Store 没实现 PendingClassCounter 时只打一次 INFO 并返回 nil;计数失败时 gauge 保持上一轮的值。
// 并发调用按 auditMu 串行。
func (l *Loop) AuditOnce(ctx context.Context, nowMs uint64) error {
	counter, ok := l.store.(PendingClassCounter)
	if !ok {
		l.auditOffOnce.Do(func() {
			logx.Info("[AssetOp] Pending audit not running: store does not implement PendingClassCounter")
		})
		return nil
	}
	l.auditMu.Lock()
	defer l.auditMu.Unlock()

	counts, err := counter.CountPendingByClass(ctx, nowMs, l.abandonCutoffMs(nowMs))
	if err != nil {
		l.metrics.incStoreError("audit")
		logx.Errorf("[AssetOp] Failed to count pending rows by class: %v", err)
		return fmt.Errorf("assetop: 日对账计数失败: %w", err)
	}

	merged := make(map[pendingKey]ClassCount, len(counts))
	var totals [pendingClassCount]ClassCount
	for _, c := range counts {
		if !c.Class.valid() {
			logx.Errorf("[AssetOp] Pending audit got an invalid class from store; ignored stream=%d class=%d rows=%d",
				c.Stream, c.Class, c.Rows)
			continue
		}
		key := pendingKey{stream: c.Stream, class: c.Class}
		m := merged[key]
		m.Stream, m.Class = c.Stream, c.Class
		m.Rows += c.Rows
		m.OldestAgeMs = max(m.OldestAgeMs, c.OldestAgeMs)
		merged[key] = m

		totals[c.Class].Rows += c.Rows
		totals[c.Class].OldestAgeMs = max(totals[c.Class].OldestAgeMs, c.OldestAgeMs)
	}

	for _, stream := range l.cfg.Streams {
		for _, class := range allPendingClasses {
			key := pendingKey{stream: stream, class: class}
			c := merged[key]
			l.metrics.setPendingClass(stream, class, c.Rows, msToSeconds(c.OldestAgeMs))
			delete(merged, key)
		}
	}
	// 剩下的都是 Streams 之外的组合:本轮报出来,并把上一轮报过、本轮消失的置 0。
	extra := make(map[pendingKey]struct{}, len(merged))
	for key, c := range merged {
		l.metrics.setPendingClass(key.stream, key.class, c.Rows, msToSeconds(c.OldestAgeMs))
		extra[key] = struct{}{}
	}
	for key := range l.auditExtra {
		if _, still := extra[key]; !still {
			l.metrics.setPendingClass(key.stream, key.class, 0, 0)
		}
	}
	l.auditExtra = extra

	logx.Infof("[AssetOp] Pending audit: online_retry=%d(oldest %.0fs) offline=%d(oldest %.0fs) alert=%d(oldest %.0fs) abandon_candidate=%d(oldest %.0fs)",
		totals[PendingOnlineRetry].Rows, msToSeconds(totals[PendingOnlineRetry].OldestAgeMs),
		totals[PendingOffline].Rows, msToSeconds(totals[PendingOffline].OldestAgeMs),
		totals[PendingAlert].Rows, msToSeconds(totals[PendingAlert].OldestAgeMs),
		totals[PendingAbandonCandidate].Rows, msToSeconds(totals[PendingAbandonCandidate].OldestAgeMs))
	return nil
}

// resetAuditGauges 在对账执行者卸任(RunMaintenance 退出)时清空按类 gauge(理由见
// Metrics.resetPendingClass),并忘掉上一轮报过的配置外组合:序列已经清掉,再当选时不必再给它们置 0。
func (l *Loop) resetAuditGauges() {
	l.auditMu.Lock()
	defer l.auditMu.Unlock()
	l.auditExtra = nil
	l.metrics.resetPendingClass()
}

func msToSeconds(ms uint64) float64 { return float64(ms) / 1000 }
