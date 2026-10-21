package commerce

// tick_isolation.go —— Engine.Tick 的单行隔离(2026-10-20,第一轮审计 §2.5 go-asset-services#0 / #1)。
//
// 以前 Tick 只要一条合同出错就整轮返回;失败行的 updated_ms 不变,下一轮仍排在 Due 队头,全服交易被一行卡死
// (最容易触发的是离线买家累计 16 条未决 credit 后的 ErrTooManyPending)。持有中的竞价(bid_held)也不改 updated_ms,
// 在拍拍品数达到 Due 上限后同样霸占队头。本文件把一行的失败收口在这一行:
//   - 暂时失败:推迟这一行的下次到期(CommerceRepo.Defer,只在版本未变时生效,不旧盖新);
//   - 确定性坏数据:隔离为 Manual 并移出 Due(quarantine),等人工处理;
//   - 等待中的持有竞价:推迟到拍品截止(settleHeldBid)。
// 设计说明见 docs/design/jubaozhai-market.md §15.3。

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"math/rand/v2"
	"time"

	tradepb "proto/trade"
	"shared/assetop"
	"trade/internal/data"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/zeromicro/go-zero/core/logx"
)

const (
	// tickDueLimit / tickAckLimit:每轮最多推进的合同数与最多尝试的收据 ACK 数(沿用改动前的 100)。
	tickDueLimit = 100
	tickAckLimit = 100
	// tickRowBudget 是单行推进的子预算(Tick 总预算 10s,见 svc.AssetChannel.Start)。一行跑满它算这一行的失败、
	// 按暂时失败推迟;没有它的话,一条每次都卡满总预算的行会让后面的行永远轮不到。
	// 单行的步骤:EnsureSeqRows(≤ StoreOpTimeout 2s)、8 次 op_id 取号、一次合同事务(两次尝试),5s 留足余量。
	tickRowBudget = 5 * time.Second
	// tickRetryBackoff:暂时失败(存储超时、死锁重试用尽、号段暂不可用等)的推迟中值,±20% 抖动。
	tickRetryBackoff = 10 * time.Second
	// tickPendingBackoff:ErrTooManyPending 的推迟中值,±20% 抖动。它要等那位玩家的未决资产指令终结
	// (多半要他重新上线),比一般的暂时失败慢得多,没必要每 10 秒白跑一遍。
	tickPendingBackoff = 60 * time.Second
	// bidHeldRecheck / bidHeldMaxWait:持有竞价等拍品结算时的推迟下限与上限。拍品截止前它不可能结算,
	// 所以推迟到拍品截止,但至少 bidHeldRecheck、至多 bidHeldMaxWait(拍品卡在 Manual 时仍有界地复查)。
	bidHeldRecheck = 30 * time.Second
	bidHeldMaxWait = time.Hour
)

// errCorruptContract 标记"合同自身的持久数据坏了"的确定性失败:重试只会每轮撞同一个错。
var errCorruptContract = errors.New("交易合同数据损坏")

// corrupt 把 err 标成确定性坏数据。errors.Is(…, errCorruptContract) 与 errors.Is(…, 原错误) 同时成立。
func corrupt(err error) error { return fmt.Errorf("%w: %w", errCorruptContract, err) }

// 失败分类,即 tickRowFailuresTotal 的 class label(低基数常量,不带任何玩家或合同 id)。
const (
	failureRetry          = "retry"
	failureTooManyPending = "too_many_pending"
	failureCorrupt        = "corrupt"
)

func failureClass(err error) string {
	switch {
	case errors.Is(err, errCorruptContract):
		return failureCorrupt
	case errors.Is(err, assetop.ErrTooManyPending):
		return failureTooManyPending
	default:
		return failureRetry
	}
}

// 指标注册在 prometheus.DefaultRegisterer 上,与 data 包的 trade_assetop_* 计数器同一个 registry,
// 由 svc.StartMetrics 的 promhttp.Handler() 一起暴露。包级变量只初始化一次,单测里重复构造 Engine 也不会重复注册。
var (
	tickRowFailuresTotal = promauto.With(prometheus.DefaultRegisterer).NewCounterVec(prometheus.CounterOpts{
		Subsystem: "trade",
		Name:      "commerce_tick_row_failures_total",
		Help:      "交易合同推进单行失败次数(按 class:retry / too_many_pending / corrupt);失败行被推迟或隔离,不再挡住同轮其余合同。",
	}, []string{"class"})
	quarantinedTotal = promauto.With(prometheus.DefaultRegisterer).NewCounter(prometheus.CounterOpts{
		Subsystem: "trade",
		Name:      "commerce_quarantined_total",
		Help:      "因确定性坏数据被隔离为 Manual 的交易合同数;>0 需要人工处理(托管资产仍在合同里)。",
	})
)

func init() {
	// 先把三个 class 都建出来:从未发生与指标不存在在告警规则里不该长得一样。
	for _, class := range []string{failureRetry, failureTooManyPending, failureCorrupt} {
		tickRowFailuresTotal.WithLabelValues(class)
	}
}

// step 推进 Due 里的一行。返回的错误由 Tick 交给 deferCommerce 分流;合同自身的持久数据坏了用 corrupt 包装。
func (e *Engine) step(ctx context.Context, r *tradepb.TradeCommerceRecord) error {
	if r.Kind == 6 {
		if e.Rmb == nil {
			return errors.New("人民币寄售恢复器未装配")
		}
		return e.Rmb.step(ctx, r)
	}
	w, err := data.Workflow(r)
	if err != nil {
		return corrupt(err)
	}
	switch w.Stage {
	case "bid_ready":
		return e.acceptBid(ctx, r.CommerceId)
	case "bid_held":
		return e.settleHeldBid(ctx, r)
	}
	if err := e.advance(ctx, r); err != nil {
		return fmt.Errorf("推进交易合同: %w", err)
	}
	if r.Kind == 4 && r.Side != 0 {
		// advance 已提交并把 updated_ms 写成现在,这一行本就排到了队尾;撮合失败只记一次失败,
		// 随后的推迟会因版本已变而落空,下一轮照常重撮。
		if err := e.matchMarket(ctx, r.CommerceId); err != nil {
			return fmt.Errorf("撮合市场订单: %w", err)
		}
	}
	return nil
}

// settleHeldBid:拍品结算后把持有中的竞价结案;拍品未结算时把这一行推迟到拍品截止。
//
// 以前未结算时原样跳过、updated_ms 不变,每个有出价的在拍拍品都有一行 bid_held 永远排在 Due 队头;
// 在拍拍品数达到 tickDueLimit 后 Due 只回这些行,拍品自己也推进不了,整个交易停摆。
func (e *Engine) settleHeldBid(ctx context.Context, r *tradepb.TradeCommerceRecord) error {
	parent, err := e.Repo.Get(ctx, r.ParentId)
	if errors.Is(err, sql.ErrNoRows) {
		return corrupt(fmt.Errorf("持有竞价的拍品 %d 不存在", r.ParentId))
	}
	if err != nil {
		return err
	}
	if parent.Status != Settled {
		now := e.now()
		until := min(max(parent.ExpiresMs, now+uint64(bidHeldRecheck.Milliseconds())), now+uint64(bidHeldMaxWait.Milliseconds()))
		// 落空(版本已变)说明这一行刚被出价改成 bid_refund 之类,新状态立即到期,不需要推迟。
		_, err := e.Repo.Defer(ctx, r.CommerceId, r.Version, until)
		return err
	}
	return e.Repo.Tx(ctx, func(tx *sql.Tx) error {
		current, err := e.Repo.Lock(ctx, tx, r.CommerceId)
		if err != nil {
			return err
		}
		cw, err := data.Workflow(current)
		if err != nil {
			return corrupt(err)
		}
		if cw.Stage != "bid_held" {
			return nil
		}
		current.Status = Settled
		cw.Stage = "done"
		if err := e.Repo.Ops.MarkCommerceOpsConsumedTx(ctx, tx, current.CommerceId); err != nil {
			return err
		}
		return e.Repo.Save(ctx, tx, current, cw, e.now())
	})
}

// deferCommerce 处理一行推进失败:确定性坏数据先尝试隔离;其余(以及隔离落空的)按 tickFailureBackoff 推迟下次到期。
// 用 Tick 的父 ctx(这一行的子预算可能已经用完)。推迟本身失败只记日志:这一行下一轮还会被取到,不丢任何东西。
//
// 推迟与隔离都以 Due 读到的 r.Version 为条件。这一行若在 Due 之后被别的副本或玩家操作推进过,两者都会落空,
// 由新状态决定它何时到期 —— 绝不拿旧观察覆盖新行。
func (e *Engine) deferCommerce(ctx context.Context, r *tradepb.TradeCommerceRecord, cause error) {
	class := failureClass(cause)
	tickRowFailuresTotal.WithLabelValues(class).Inc()
	if class == failureCorrupt {
		stage, quarantined, err := e.quarantine(ctx, r)
		if err == nil && quarantined {
			quarantinedTotal.Inc()
			logx.Errorf("[Commerce] Contract quarantined as manual (deterministic data error) commerce_id=%d kind=%d status=%d version=%d stage=%q: %v",
				r.CommerceId, r.Kind, r.Status, r.Version, stage, cause)
			return
		}
		if err != nil {
			logx.Errorf("[Commerce] Failed to quarantine contract commerce_id=%d version=%d: %v (step error: %v)",
				r.CommerceId, r.Version, err, cause)
		}
		// 隔离落空(版本已变)或失败:退回普通推迟,下一轮按新版本重判。
	}
	untilMs := tickFailureBackoff(e.now(), cause, rand.Float64)
	deferred, err := e.Repo.Defer(ctx, r.CommerceId, r.Version, untilMs)
	if err != nil {
		logx.Errorf("[Commerce] Failed to defer contract commerce_id=%d version=%d after step error (%v): %v",
			r.CommerceId, r.Version, cause, err)
		return
	}
	if class == failureTooManyPending {
		// 积压背压是预期内的状态(离线玩家的未决资产指令),指标已计数,日志不升到 ERROR。
		logx.Infof("[Commerce] Contract step deferred by pending-op backpressure commerce_id=%d kind=%d deferred=%t until_ms=%d: %v",
			r.CommerceId, r.Kind, deferred, untilMs, cause)
		return
	}
	logx.Errorf("[Commerce] Contract step failed commerce_id=%d kind=%d status=%d class=%s deferred=%t until_ms=%d: %v",
		r.CommerceId, r.Kind, r.Status, class, deferred, untilMs, cause)
}

// tickFailureBackoff 算失败行的下次到期时刻:nowMs + 中值 ×(0.8 + 0.4×rnd()),复用 assetop.NextAttemptMs 的抖动口径
// (attempts=0、max=base:不做指数增长 —— 合同表没有失败次数列,推迟有界即可)。rnd 由调用方注入,单测要确定性。
func tickFailureBackoff(nowMs uint64, cause error, rnd func() float64) uint64 {
	base := tickRetryBackoff
	if errors.Is(cause, assetop.ErrTooManyPending) {
		base = tickPendingBackoff
	}
	return assetop.NextAttemptMs(nowMs, 0, base, base, rnd)
}

// quarantine 把确定性坏数据的合同隔离为 Manual(移出 Due),返回原 stage 供日志取证。只在行仍是 r.Version 时生效。
//
//   - workflow 解得开:stage 改成 "manual"(与 APPLIED_PARTIAL / 撮合子单坏账的既有隔离同形),
//     这样 Mutate / PlaceBid / 撮合的 stage 判断都不会再把它拉回推进;
//   - 解不开:只改状态列(CommerceRepo.SaveStatus),坏字节原样保留作人工取证。
//
// 托管中的资产仍记在合同里,没有任何资产被发出或退回;人工处理后按合同证据补发 / 退回。
func (e *Engine) quarantine(ctx context.Context, r *tradepb.TradeCommerceRecord) (string, bool, error) {
	stage, quarantined := "", false
	err := e.Repo.Tx(ctx, func(tx *sql.Tx) error {
		stage, quarantined = "", false
		cur, err := e.Repo.Lock(ctx, tx, r.CommerceId)
		if errors.Is(err, sql.ErrNoRows) {
			return nil
		}
		if err != nil {
			return err
		}
		if cur.Version != r.Version || cur.Status == Manual {
			return nil
		}
		w, decodeErr := data.Workflow(cur)
		if decodeErr != nil {
			stage = "<undecodable>"
			if err := e.Repo.SaveStatus(ctx, tx, cur, Manual, e.now()); err != nil {
				return err
			}
		} else {
			stage = w.Stage
			cur.Status = Manual
			w.Stage = "manual"
			if err := e.Repo.Save(ctx, tx, cur, w, e.now()); err != nil {
				return err
			}
		}
		quarantined = true
		return nil
	})
	return stage, quarantined, err
}

// ackReceipts 轮转发一页收据 ACK。按 op_id 翻页(CommerceRepo.AckDue 的 afterOpID):离线卖家的 ACK 一直 done=false,
// 只从表头取会让它们永远占住名额,后面的 ACK 永远轮不到。每尝试一条就把游标推到它;本页不满说明到了表尾,下一轮从头。
// RPC / SQL 依赖错误保留待办、推进游标并继续本页,页末汇总给 Tick 调用者统一记录;
// 正常离线(done=false、err=nil)不算故障。预算用完时游标停在最后一条已尝试的 op 上。
// payload 解不开的行(AckDue 回 Bundle=nil)跳过并告警、游标照常越过它:托管原件仍在 escrow_snapshot 里,
// 只是这条 ACK 发不出去,要人工按 op_id 查;不能让它挡住同页其余卖家的 ACK。
func (e *Engine) ackReceipts(ctx context.Context) error {
	if e.Applier == nil || e.Ledger == nil {
		return nil
	}
	ops, err := e.Repo.AckDue(ctx, e.ackAfterOpID, tickAckLimit)
	if err != nil {
		return err
	}
	var failures error
	for _, op := range ops {
		if err := ctx.Err(); err != nil {
			return errors.Join(failures, err)
		}
		if op.Bundle == nil {
			logx.Errorf("[Commerce] Receipt ack skipped: asset op payload undecodable op_id=%d stream=%d", op.OpID, int32(op.Stream))
			e.ackAfterOpID = op.OpID
			continue
		}
		child, cancel := context.WithTimeout(ctx, 1500*time.Millisecond)
		ok, err := assetop.AcknowledgeReceipt(child, e.Applier, e.Ledger, op.Request())
		cancel()
		if err != nil {
			failures = errors.Join(failures, fmt.Errorf("交易收据确认依赖失败 op_id=%d: %w", op.OpID, err))
		} else if ok {
			if err := e.Repo.Acked(ctx, op.OpID); err != nil {
				failures = errors.Join(failures, fmt.Errorf("交易收据待办清理失败 op_id=%d: %w", op.OpID, err))
			}
		}
		e.ackAfterOpID = op.OpID
	}
	if len(ops) < tickAckLimit {
		e.ackAfterOpID = 0
	}
	return failures
}

// logOffbandLegOutcome 记下"合同腿的终局不是 scene 落盘给的"(人工终结或自动放弃)。只是观察日志:
// 所在事务可能随后回滚重试,所以不计指标(放弃与人工终结各自已在 assetop 计数:assetop_abandoned_total、
// assetop_manual_resolve_total)。
func logOffbandLegOutcome(commerceID uint64, l *tradepb.TradeCommerceLeg, out data.LegOutcome) {
	source := out.Source()
	if source == "scene" {
		return
	}
	logx.Infof("[Commerce] Leg outcome observed from %s resolution commerce_id=%d role=%s op_id=%d status=%d",
		source, commerceID, l.Role, l.OpId, int32(out.Status))
}
