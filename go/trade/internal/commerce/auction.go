package commerce

import (
	"context"
	"database/sql"
	"errors"
	"math"
	tradepb "proto/trade"
	"trade/internal/data"
)

func (e *Engine) PlaceBid(ctx context.Context, me, lotID uint64, in *tradepb.PlaceAuctionBidRequest) (*tradepb.TradeCommerceRecord, error) {
	lot, err := e.Repo.Get(ctx, lotID)
	if err != nil {
		return nil, err
	}
	w, err := data.Workflow(lot)
	if err != nil {
		return nil, err
	}
	if lot.Kind != 5 || w.Stage != "lot_open" || lot.ExpiresMs <= e.now() {
		return nil, ErrEnded
	}
	if me == lot.SellerPlayerId {
		return nil, ErrSelf
	}
	if in.Amount < lot.UnitPrice || in.Amount > math.MaxInt64 || w.HighestBid > 0 && (w.MinimumIncrement > math.MaxInt64-w.HighestBid || in.Amount < w.HighestBid+w.MinimumIncrement) {
		return nil, ErrBid
	}
	return e.Create(ctx, me, in.ClientRequestId, in, &tradepb.TradeCommerceRecord{Kind: BidKind, Status: Escrowing, SellerPlayerId: me, ParentId: lotID, UnitPrice: in.Amount, CurrencyType: lot.CurrencyType}, &tradepb.TradeCommerceWorkflow{SellerAssets: &tradepb.TradeAssetSelection{CurrencyType: lot.CurrencyType, CurrencyAmount: in.Amount}, Stage: "bid_freeze"})
}

// 每个lot的guard先于任何合同锁。冻结完成只获得参选资格，提交最高价时再次检查结束时间/增幅。
// 多副本/迟到结果输掉竞价时写原币退款待办，不偷换或取消已发出的扣币seq。
func (e *Engine) acceptBid(ctx context.Context, bidID uint64) error {
	candidate, err := e.Repo.Get(ctx, bidID)
	if err != nil {
		return err
	}
	return e.Repo.Tx(ctx, func(tx *sql.Tx) error {
		if _, err := tx.ExecContext(ctx, "INSERT IGNORE INTO trade_commerce_guard(guard_id) VALUES(?)", candidate.ParentId); err != nil {
			return err
		}
		var guard uint64
		if err := tx.QueryRowContext(ctx, "SELECT guard_id FROM trade_commerce_guard WHERE guard_id=? FOR UPDATE", candidate.ParentId).Scan(&guard); err != nil {
			return err
		}
		lot, err := e.Repo.Lock(ctx, tx, candidate.ParentId)
		if err != nil {
			return err
		}
		lw, err := data.Workflow(lot)
		if err != nil {
			return err
		}
		bid, err := e.Repo.Lock(ctx, tx, bidID)
		if err != nil {
			return err
		}
		bw, err := data.Workflow(bid)
		if err != nil {
			return err
		}
		if bw.Stage != "bid_ready" {
			return nil
		}
		var now uint64
		if err := tx.QueryRowContext(ctx, "SELECT CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED)").Scan(&now); err != nil {
			return err
		}
		eligible := lw.Stage == "lot_open" && !lw.CancelRequested && lot.ExpiresMs > now && bid.UnitPrice >= lot.UnitPrice && (lw.HighestBid == 0 || lw.MinimumIncrement <= math.MaxInt64-lw.HighestBid && bid.UnitPrice >= lw.HighestBid+lw.MinimumIncrement)
		// 截止判定用库时钟(多副本同一把尺);updated_ms 用引擎时钟,与其它 Save 一致 —— 它是 Due 的调度键,
		// Due 按引擎时钟取到期行,库时钟领先时用它写 updated_ms 会让刚改好的行晚到期(2026-10-20)。
		saveMs := e.now()
		if !eligible {
			bw.Stage = "bid_refund"
			bid.Status = CancelPending
			return e.Repo.Save(ctx, tx, bid, bw, saveMs)
		}
		if lw.WinningBidId != 0 {
			old, err := e.Repo.Lock(ctx, tx, lw.WinningBidId)
			if err != nil {
				return err
			}
			ow, err := data.Workflow(old)
			if err != nil {
				return err
			}
			if ow.Stage != "bid_held" {
				return errors.New("最高竞价托管状态损坏")
			}
			ow.Stage = "bid_refund"
			old.Status = CancelPending
			if err := e.Repo.Save(ctx, tx, old, ow, saveMs); err != nil {
				return err
			}
		}
		lw.HighestBid = bid.UnitPrice
		lw.HighestBidder = bid.SellerPlayerId
		lw.WinningBidId = bid.CommerceId
		lw.BuyerAssets = bw.SellerAssets
		lot.BuyerPlayerId = bid.SellerPlayerId
		bw.Stage = "bid_held"
		if err := e.Repo.Save(ctx, tx, lot, lw, saveMs); err != nil {
			return err
		}
		return e.Repo.Save(ctx, tx, bid, bw, saveMs)
	})
}
