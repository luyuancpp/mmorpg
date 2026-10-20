package commerce

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
	tradepb "proto/trade"
	"trade/internal/data"
)

// 价格时间优先只由SQL中的已验证托管订单裁决。Redis深度缓存不是撮合证据。
// 一次事务预留双边数量+写成交交付意图；RPC全在提交后。余额从不进本库。
func (e *Engine) matchMarket(ctx context.Context, id uint64) error {
	taker, err := e.Repo.Get(ctx, id)
	if err != nil {
		return err
	}
	if taker.Kind != 4 || taker.Side == 0 || taker.FilledQuantity == taker.Quantity {
		return nil
	}
	matchID, err := e.IDs.Next(ctx)
	if err != nil {
		return err
	}
	return e.Repo.Tx(ctx, func(tx *sql.Tx) error {
		key := sha256.Sum256([]byte(fmt.Sprintf("market:%d:%d:%d", taker.MarketId, taker.ItemConfigId, taker.CurrencyType)))
		guardID := binary.BigEndian.Uint64(key[:8]) | 1<<63
		if _, err := tx.ExecContext(ctx, "INSERT IGNORE INTO trade_commerce_guard(guard_id) VALUES(?)", guardID); err != nil {
			return err
		}
		var guard uint64
		if err := tx.QueryRowContext(ctx, "SELECT guard_id FROM trade_commerce_guard WHERE guard_id=? FOR UPDATE", guardID).Scan(&guard); err != nil {
			return err
		}
		priceFilter, order := "unit_price>=?", "unit_price DESC,created_ms,commerce_id"
		opposite := uint32(1)
		if taker.Side == 1 {
			priceFilter = "unit_price<=?"
			order = "unit_price ASC,created_ms,commerce_id"
			opposite = 2
		}
		var otherID uint64
		err := tx.QueryRowContext(ctx, "SELECT commerce_id FROM trade_commerce WHERE kind=4 AND side=? AND market_id=? AND item_config_id=? AND currency_type=? AND status=3 AND filled_quantity<quantity AND seller_player_id<>? AND "+priceFilter+" ORDER BY "+order+" LIMIT 1", opposite, taker.MarketId, taker.ItemConfigId, taker.CurrencyType, taker.SellerPlayerId, taker.UnitPrice).Scan(&otherID)
		if errors.Is(err, sql.ErrNoRows) {
			return nil
		}
		if err != nil {
			return err
		}
		first, second := id, otherID
		if first > second {
			first, second = second, first
		}
		a, err := e.Repo.Lock(ctx, tx, first)
		if err != nil {
			return err
		}
		b, err := e.Repo.Lock(ctx, tx, second)
		if err != nil {
			return err
		}
		aw, err := data.Workflow(a)
		if err != nil {
			return err
		}
		bw, err := data.Workflow(b)
		if err != nil {
			return err
		}
		if aw.Stage != "market_open" || bw.Stage != "market_open" || aw.CancelRequested || bw.CancelRequested || a.FilledQuantity >= a.Quantity || b.FilledQuantity >= b.Quantity {
			return nil
		}
		buy, sell := a, b
		if buy.Side != 1 {
			buy, sell = sell, buy
		}
		if buy.UnitPrice < sell.UnitPrice || buy.SellerPlayerId == sell.SellerPlayerId {
			return nil
		}
		maker := a
		if b.CreatedMs < a.CreatedMs || b.CreatedMs == a.CreatedMs && b.CommerceId < a.CommerceId {
			maker = b
		}
		price := maker.UnitPrice
		qty := min(buy.Quantity-buy.FilledQuantity, sell.Quantity-sell.FilledQuantity)
		now := e.now()
		m := &tradepb.TradeCommerceRecord{CommerceId: matchID, RequestKey: hex.EncodeToString(key[:]) + ":" + fmt.Sprint(matchID), Kind: 4, Status: Delivering, SellerPlayerId: sell.SellerPlayerId, BuyerPlayerId: buy.SellerPlayerId, MarketId: buy.MarketId, ItemConfigId: buy.ItemConfigId, UnitPrice: price, Quantity: qty, CurrencyType: buy.CurrencyType, CreatedMs: now, UpdatedMs: now, Version: 1, ParentId: buy.CommerceId, CounterpartyId: sell.CommerceId}
		mw := &tradepb.TradeCommerceWorkflow{Stage: "deliver"}
		addLeg(mw, "buyer_receive", buy.SellerPlayerId, 4, &assetpb.AssetBundle{Items: []*assetpb.ItemGrant{{ConfigId: buy.ItemConfigId, Count: qty}}})
		addLeg(mw, "seller_receive", sell.SellerPlayerId, 4, &assetpb.AssetBundle{Currencies: []*assetpb.CurrencyAmount{{CurrencyType: buy.CurrencyType, Amount: uint64(qty) * price}}})
		if buy.UnitPrice > price {
			addLeg(mw, "buyer_surplus", buy.SellerPlayerId, 4, &assetpb.AssetBundle{Currencies: []*assetpb.CurrencyAmount{{CurrencyType: buy.CurrencyType, Amount: uint64(qty) * (buy.UnitPrice - price)}}})
		}
		m.WorkflowProto, err = proto.MarshalOptions{Deterministic: true}.Marshal(mw)
		if err != nil {
			return err
		}
		if err := e.Repo.Insert(ctx, tx, m); err != nil {
			return err
		}
		a.FilledQuantity += qty
		b.FilledQuantity += qty
		if a.FilledQuantity == a.Quantity {
			a.Status = Delivering
		}
		if b.FilledQuantity == b.Quantity {
			b.Status = Delivering
		}
		if err := e.Repo.Save(ctx, tx, a, aw, now); err != nil {
			return err
		}
		return e.Repo.Save(ctx, tx, b, bw, now)
	})
}

func (e *Engine) marketBarrier(ctx context.Context, tx *sql.Tx, r *tradepb.TradeCommerceRecord, w *tradepb.TradeCommerceWorkflow) error {
	if r.Kind != 4 || r.Side == 0 {
		return nil
	}
	var pending, bad int
	if err := tx.QueryRowContext(ctx, "SELECT COALESCE(SUM(status NOT IN (7,11)),0),COALESCE(SUM(status=10),0) FROM trade_commerce WHERE kind=4 AND side=0 AND (parent_id=? OR counterparty_id=?)", r.CommerceId, r.CommerceId).Scan(&pending, &bad); err != nil {
		return err
	}
	if bad > 0 {
		r.Status = Manual
		w.Stage = "manual"
		return nil
	}
	if w.Stage == "done" && pending > 0 {
		w.Stage = "market_wait"
		r.Status = CancelPending
	}
	if w.Stage == "market_wait" && pending == 0 {
		w.Stage = "done"
		r.Status = Cancelled
	}
	if w.Stage == "market_open" && r.Quantity == r.FilledQuantity && pending == 0 {
		w.Stage = "done"
		r.Status = Settled
	}
	return nil
}
