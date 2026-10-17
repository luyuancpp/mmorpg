package data

import (
	"context"
	"database/sql"
	"errors"

	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
	mailpb "proto/mail"
	"shared/assetop"
	table "shared/generated/pb/table"
)

func (r *Repo) ListDue(ctx context.Context, now uint64, limit int) ([]uint64, error) {
	if limit <= 0 || limit > 1000 {
		return nil, ErrInvalid
	}
	var ids []uint64
	seen := map[uint64]bool{}
	for _, comparison := range []string{"<", ">="} {
		remaining := limit - len(ids)
		if remaining == 0 {
			break
		}
		rows, err := r.db.QueryContext(ctx, "SELECT op_id FROM mail_asset_op WHERE status=1 AND next_attempt_ms<=? AND lease_until_ms<? AND attempts "+comparison+" ? ORDER BY next_attempt_ms,op_id LIMIT ?", now, now, assetop.FreshAttemptLimit, remaining)
		if err != nil {
			return nil, err
		}
		for rows.Next() {
			var id uint64
			if err = rows.Scan(&id); err != nil {
				rows.Close()
				return nil, err
			}
			if !seen[id] {
				seen[id] = true
				ids = append(ids, id)
			}
		}
		err = rows.Err()
		_ = rows.Close()
		if err != nil {
			return nil, err
		}
	}
	return ids, nil
}

func (r *Repo) ClaimOp(ctx context.Context, id, now, until, poison, token uint64) (assetop.Op, bool, error) {
	res, err := r.db.ExecContext(ctx, "UPDATE mail_asset_op SET last_send_ms=GREATEST(last_send_ms,lease_until_ms),lease_until_ms=?,lease_token=? WHERE op_id=? AND status=1 AND lease_until_ms<?", until, token, id, now)
	if err != nil {
		return assetop.Op{}, false, err
	}
	n, err := res.RowsAffected()
	if err != nil || n == 0 {
		return assetop.Op{}, false, err
	}
	op := assetop.Op{OpID: id, LeaseToken: token}
	var payload []byte
	var stream uint32
	err = r.db.QueryRowContext(ctx, "SELECT player_id,stream,seq,stream_epoch,tx_type,payload,attempts,last_reason,created_ms,last_send_ms FROM mail_asset_op WHERE op_id=?", id).Scan(&op.PlayerID, &stream, &op.Seq, &op.StreamEpoch, &op.TxType, &payload, &op.Attempts, &op.LastReason, &op.CreatedMs, &op.LastSendMs)
	if err != nil {
		return assetop.Op{}, false, err
	}
	op.Stream = assetpb.AssetOpStream(stream)
	op.CorrelationID = id
	op.Bundle = &assetpb.AssetBundle{}
	if proto.Unmarshal(payload, op.Bundle) != nil {
		err = r.Reschedule(ctx, op, poison, assetop.Result{}, now)
		if err != nil {
			return assetop.Op{}, false, err
		}
		return assetop.Op{}, false, assetop.ErrPoisonRow
	}
	return op, true, nil
}

// StoreAdapter避免客户端Claim与assetop.Store.Claim方法同名且语义不同。
type StoreAdapter struct{ *Repo }

func (s StoreAdapter) Claim(ctx context.Context, id, now, until, poison, token uint64) (assetop.Op, bool, error) {
	return s.Repo.ClaimOp(ctx, id, now, until, poison, token)
}

var _ assetop.Store = StoreAdapter{}

func (r *Repo) lockOp(ctx context.Context, tx *sql.Tx, id uint64) (*mailpb.MailAssetOpRecord, error) {
	p := &mailpb.MailAssetOpRecord{}
	err := tx.QueryRowContext(ctx, "SELECT op_id,player_id,mail_id,claim_version,status,lease_token,last_reason FROM mail_asset_op WHERE op_id=? FOR UPDATE", id).Scan(&p.OpId, &p.PlayerId, &p.MailId, &p.ClaimVersion, &p.Status, &p.LeaseToken, &p.LastReason)
	return p, err
}
func (r *Repo) Reschedule(ctx context.Context, op assetop.Op, next uint64, res assetop.Result, now uint64) error {
	return r.tx(ctx, func(tx *sql.Tx) error {
		p, err := r.lockOp(ctx, tx, op.OpID)
		if err != nil {
			return err
		}
		if p.Status != 1 || p.LeaseToken != op.LeaseToken {
			return assetop.ErrLeaseLost
		}
		var durable uint32
		if res.Durable {
			durable = 1
		}
		lastSend := uint64(0)
		if !res.Local {
			lastSend = now
		}
		_, err = tx.ExecContext(ctx, "UPDATE mail_asset_op SET attempts=attempts+1,next_attempt_ms=?,lease_until_ms=0,durable=?,last_outcome=?,last_reason=?,updated_ms=?,last_send_ms=GREATEST(last_send_ms,?) WHERE op_id=?", next, durable, uint32(res.Outcome), res.Reason, now, lastSend, op.OpID)
		return err
	})
}
func (r *Repo) Finalize(ctx context.Context, op assetop.Op, status assetop.Status, res assetop.Result, now uint64) (bool, error) {
	// 此处也复核SQL证据，不能由旧Redis-only回包绕过共享Loop的强制门禁。
	if !res.SQLCommitted {
		return false, errors.New("邮件资产终结缺少SQL提交证据")
	}
	final := uint32(0)
	claim := uint32(0)
	reason := res.Reason
	switch status {
	case assetop.StatusApplied:
		if res.Partial {
			return false, ErrProtected
		}
		final = 2
		claim = 3
	case assetop.StatusRejected, assetop.StatusAborted:
		final = 3
		claim = 1
	case assetop.StatusAppliedPartial:
		final = 4
		claim = 4
		reason = uint32(table.MailError_kMailClaimManual)
	default:
		return false, ErrInvalid
	}
	applied := false
	err := r.tx(ctx, func(tx *sql.Tx) error {
		if e := r.lockBox(ctx, tx, op.PlayerID); e != nil {
			return e
		}
		// 只读取得不可变mail_id，再按全序锁state→op，和领取入口相同。
		var mailID uint64
		if e := tx.QueryRowContext(ctx, "SELECT mail_id FROM mail_asset_op WHERE op_id=?", op.OpID).Scan(&mailID); e != nil {
			return e
		}
		var current, version uint64
		var state uint32
		if e := tx.QueryRowContext(ctx, "SELECT current_op_id,claim_version,claim_state FROM mail_state WHERE player_id=? AND mail_id=? FOR UPDATE", op.PlayerID, mailID).Scan(&current, &version, &state); e != nil {
			return e
		}
		p, e := r.lockOp(ctx, tx, op.OpID)
		if e != nil {
			return e
		}
		if p.Status != 1 {
			return nil
		}
		if current != op.OpID || version != p.ClaimVersion || state != 2 {
			return ErrConflict
		}
		if _, e = tx.ExecContext(ctx, "UPDATE mail_asset_op SET status=?,durable=1,last_outcome=?,last_reason=?,next_attempt_ms=?,lease_until_ms=0,updated_ms=?,reason_tip_id=? WHERE op_id=?", final, uint32(res.Outcome), res.Reason, now, now, reason, op.OpID); e != nil {
			return e
		}
		if _, e = tx.ExecContext(ctx, "UPDATE mail_state SET claim_state=?,reason_tip_id=?,updated_ms=? WHERE player_id=? AND mail_id=? AND current_op_id=? AND claim_version=? AND claim_state=2", claim, reason, now, op.PlayerID, mailID, op.OpID, version); e != nil {
			return e
		}
		applied = true
		return nil
	})
	return applied, err
}
func (r *Repo) OldestPendingCreatedMs(ctx context.Context, stream assetpb.AssetOpStream) (uint64, bool, error) {
	var stamp sql.NullInt64
	err := r.db.QueryRowContext(ctx, "SELECT MIN(created_ms) FROM mail_asset_op WHERE stream=? AND status=1", uint32(stream)).Scan(&stamp)
	return uint64(stamp.Int64), stamp.Valid, err
}
