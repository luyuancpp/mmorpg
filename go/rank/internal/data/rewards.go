package data

import (
	"context"
	"database/sql"
	rankpb "proto/rank"
)

func (r *Repo) DueRewards(ctx context.Context, now uint64, limit int) ([]uint64, error) {
	if limit <= 0 || limit > 1000 {
		return nil, ErrInvalid
	}
	rows, err := r.db.QueryContext(ctx, "SELECT reward_id FROM rank_mail_reward WHERE status=1 AND next_attempt_ms<=? AND lease_until_ms<? ORDER BY next_attempt_ms,reward_id LIMIT ?", now, now, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var ids []uint64
	for rows.Next() {
		var id uint64
		if err = rows.Scan(&id); err != nil {
			return nil, err
		}
		ids = append(ids, id)
	}
	return ids, rows.Err()
}
func (r *Repo) ClaimReward(ctx context.Context, id, now, until, token uint64) (*rankpb.RankMailRewardRecord, bool, error) {
	res, err := r.db.ExecContext(ctx, "UPDATE rank_mail_reward SET lease_until_ms=?,lease_token=? WHERE reward_id=? AND status=1 AND lease_until_ms<?", until, token, id, now)
	if err != nil {
		return nil, false, err
	}
	n, err := res.RowsAffected()
	if err != nil || n == 0 {
		return nil, false, err
	}
	p := &rankpb.RankMailRewardRecord{RewardId: id, LeaseToken: token}
	err = r.db.QueryRowContext(ctx, "SELECT settlement_id,player_id,`rank`,attachments,attempts,created_ms FROM rank_mail_reward WHERE reward_id=?", id).Scan(&p.SettlementId, &p.PlayerId, &p.Rank, &p.Attachments, &p.Attempts, &p.CreatedMs)
	return p, err == nil, err
}

// 先主键点锁再更新带索引字段；响应未知保留原意图，下一次同reward_id发信会回放。
func (r *Repo) SettleReward(ctx context.Context, p *rankpb.RankMailRewardRecord, mailID, next, now uint64) error {
	return r.tx(ctx, func(tx *sql.Tx) error {
		var state uint32
		var token uint64
		if err := tx.QueryRowContext(ctx, "SELECT status,lease_token FROM rank_mail_reward WHERE reward_id=? FOR UPDATE", p.RewardId).Scan(&state, &token); err != nil {
			return err
		}
		if state != 1 || token != p.LeaseToken {
			return ErrConflict
		}
		state = 1
		if mailID != 0 {
			state = 2
		}
		_, err := tx.ExecContext(ctx, "UPDATE rank_mail_reward SET status=?,mail_id=?,next_attempt_ms=?,lease_until_ms=0,attempts=attempts+1,updated_ms=? WHERE reward_id=?", state, mailID, next, now, p.RewardId)
		return err
	})
}
