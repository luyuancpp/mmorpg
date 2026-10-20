package data

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
	tradepb "proto/trade"
	"shared/assetop"
)

// CommerceRepo只存合同与托管证据。玩家余额/实例的权威始终在scene玩家SQL存档。
// 所有写者统一锁序：guard(仅撮合)→commerce升序→seq升序→outbox；建seq的独立短事务先完成。
type CommerceRepo struct {
	DB  *sql.DB
	Ops *AssetOpRepo
}

const CommerceColumns = "commerce_id,request_key,request_digest,kind,status,seller_player_id,buyer_player_id,market_id,item_config_id,side,unit_price,quantity,filled_quantity,currency_type,expires_ms,created_ms,updated_ms,version,workflow_proto,parent_id,counterparty_id"

type commerceScanner interface{ Scan(...any) error }

func ScanCommerce(row commerceScanner) (*tradepb.TradeCommerceRecord, error) {
	r := &tradepb.TradeCommerceRecord{}
	err := row.Scan(&r.CommerceId, &r.RequestKey, &r.RequestDigest, &r.Kind, &r.Status, &r.SellerPlayerId, &r.BuyerPlayerId, &r.MarketId, &r.ItemConfigId, &r.Side, &r.UnitPrice, &r.Quantity, &r.FilledQuantity, &r.CurrencyType, &r.ExpiresMs, &r.CreatedMs, &r.UpdatedMs, &r.Version, &r.WorkflowProto, &r.ParentId, &r.CounterpartyId)
	return r, err
}
func (r *CommerceRepo) Get(ctx context.Context, id uint64) (*tradepb.TradeCommerceRecord, error) {
	return ScanCommerce(r.DB.QueryRowContext(ctx, "SELECT "+CommerceColumns+" FROM trade_commerce WHERE commerce_id=?", id))
}
func (r *CommerceRepo) ByRequest(ctx context.Context, key string) (*tradepb.TradeCommerceRecord, error) {
	return ScanCommerce(r.DB.QueryRowContext(ctx, "SELECT "+CommerceColumns+" FROM trade_commerce WHERE request_key=?", key))
}
func (r *CommerceRepo) Lock(ctx context.Context, tx *sql.Tx, id uint64) (*tradepb.TradeCommerceRecord, error) {
	return ScanCommerce(tx.QueryRowContext(ctx, "SELECT "+CommerceColumns+" FROM trade_commerce WHERE commerce_id=? FOR UPDATE", id))
}
func (r *CommerceRepo) Insert(ctx context.Context, tx *sql.Tx, v *tradepb.TradeCommerceRecord) error {
	_, err := tx.ExecContext(ctx, "INSERT INTO trade_commerce ("+CommerceColumns+") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", v.CommerceId, v.RequestKey, v.RequestDigest, v.Kind, v.Status, v.SellerPlayerId, v.BuyerPlayerId, v.MarketId, v.ItemConfigId, v.Side, v.UnitPrice, v.Quantity, v.FilledQuantity, v.CurrencyType, v.ExpiresMs, v.CreatedMs, v.UpdatedMs, v.Version, v.WorkflowProto, v.ParentId, v.CounterpartyId)
	return err
}
func (r *CommerceRepo) Save(ctx context.Context, tx *sql.Tx, v *tradepb.TradeCommerceRecord, w *tradepb.TradeCommerceWorkflow, now uint64) error {
	raw, err := proto.MarshalOptions{Deterministic: true}.Marshal(w)
	if err != nil {
		return err
	}
	result, err := tx.ExecContext(ctx, "UPDATE trade_commerce SET status=?,buyer_player_id=?,filled_quantity=?,expires_ms=?,updated_ms=?,version=version+1,workflow_proto=? WHERE commerce_id=? AND version=?", v.Status, v.BuyerPlayerId, v.FilledQuantity, v.ExpiresMs, now, raw, v.CommerceId, v.Version)
	if err != nil {
		return err
	}
	n, err := result.RowsAffected()
	if err != nil {
		return err
	}
	if n != 1 {
		return errors.New("交易版本已变化")
	}
	v.Version++
	v.WorkflowProto = raw
	v.UpdatedMs = now
	return nil
}
func (r *CommerceRepo) Tx(ctx context.Context, body func(*sql.Tx) error) error {
	return assetop.WithTxRetry(ctx, r.DB, 2, IsRetryableTxError, body)
}
func (r *CommerceRepo) List(ctx context.Context, player, after uint64, limit int) ([]*tradepb.TradeCommerceRecord, error) {
	return r.Query(ctx, "SELECT "+CommerceColumns+" FROM trade_commerce WHERE commerce_id>? AND kind<>1 AND (seller_player_id=? OR buyer_player_id=?) ORDER BY commerce_id LIMIT ?", after, player, player, limit)
}
func (r *CommerceRepo) Due(ctx context.Context, limit int) ([]*tradepb.TradeCommerceRecord, error) {
	return r.Query(ctx, "SELECT "+CommerceColumns+" FROM trade_commerce WHERE status IN (1,3,4,6,8) ORDER BY updated_ms,commerce_id LIMIT ?", limit)
}
func (r *CommerceRepo) Query(ctx context.Context, query string, args ...any) ([]*tradepb.TradeCommerceRecord, error) {
	rows, err := r.DB.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []*tradepb.TradeCommerceRecord
	for rows.Next() {
		v, err := ScanCommerce(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, v)
	}
	return out, rows.Err()
}
func (r *CommerceRepo) LegResult(ctx context.Context, tx *sql.Tx, id uint64) (tradepb.TradeAssetOpStatus, []byte, bool, error) {
	var status int32
	var snapshot []byte
	var durable bool
	err := tx.QueryRowContext(ctx, "SELECT status,escrow_snapshot,durable FROM trade_asset_op WHERE op_id=?", id).Scan(&status, &snapshot, &durable)
	return tradepb.TradeAssetOpStatus(status), snapshot, durable, err
}
func Workflow(v *tradepb.TradeCommerceRecord) (*tradepb.TradeCommerceWorkflow, error) {
	w := &tradepb.TradeCommerceWorkflow{}
	if err := proto.Unmarshal(v.WorkflowProto, w); err != nil {
		return nil, fmt.Errorf("交易合同损坏: %w", err)
	}
	return w, nil
}

// ACK只读已提交的托管快照；完成标志不能先于远端玩家SQL证明写入。
func (r *CommerceRepo) AckDue(ctx context.Context, limit int) ([]assetop.Op, error) {
	rows, err := r.DB.QueryContext(ctx, "SELECT op_id,player_id,stream,stream_epoch,seq,tx_type,payload FROM trade_asset_op WHERE receipt_ack_pending=1 AND status=2 AND durable=1 ORDER BY op_id LIMIT ?", limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []assetop.Op
	for rows.Next() {
		var op assetop.Op
		var payload []byte
		if err := rows.Scan(&op.OpID, &op.PlayerID, &op.Stream, &op.StreamEpoch, &op.Seq, &op.TxType, &payload); err != nil {
			return nil, err
		}
		op.CorrelationID = op.OpID
		op.Bundle = &assetpb.AssetBundle{}
		if err := proto.Unmarshal(payload, op.Bundle); err != nil {
			return nil, err
		}
		out = append(out, op)
	}
	return out, rows.Err()
}
func (r *CommerceRepo) Acked(ctx context.Context, id uint64) error {
	_, err := r.DB.ExecContext(ctx, "UPDATE trade_asset_op SET receipt_ack_pending=0 WHERE op_id=? AND status=2 AND durable=1", id)
	return err
}
