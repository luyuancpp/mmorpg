package data

import (
	"context"
	"database/sql"
	"errors"
)

// Sweep 只回收过期的个人/纯文本广播邮件；冻结名单及其邮件原件保留作资格和幂等审计。
// PENDING/MANUAL领取、未终结资产指令、未知状态、零过期时间永不由普通过期清理删除。
// mail_box、号源、asset-op和campaign清单不在此清理范围。总实删行数（含派生状态）不超过limit。
type SweepStats struct{ Candidates, Deleted, Invalid uint64 }
type SweepReport struct{ Personal, System, SystemState SweepStats }

func (r *Repo) Sweep(ctx context.Context, cutoff uint64, limit int, deleteRows bool) (report SweepReport, err error) {
	if cutoff == 0 || limit < 1 || limit > 10000 {
		return report, ErrInvalid
	}
	const eligible = `m.expire_ms>0 AND m.expire_ms<? AND m.campaign_id=0 AND m.kind IN (1,2,3)
  AND NOT EXISTS(SELECT 1 FROM mail_state s WHERE s.mail_id=m.mail_id AND (s.claim_state IS NULL OR s.claim_state NOT IN (0,1,3)))
  AND NOT EXISTS(SELECT 1 FROM mail_asset_op o WHERE o.mail_id=m.mail_id AND (o.status IS NULL OR o.status NOT IN (2,3,5)))`
	rows, e := r.db.QueryContext(ctx, "SELECT m.mail_id,m.kind FROM mail_message m WHERE "+eligible+" ORDER BY m.expire_ms,m.mail_id LIMIT ?", cutoff, limit)
	if e != nil {
		return report, e
	}
	type candidate struct {
		id   uint64
		kind uint32
	}
	var candidates []candidate
	for rows.Next() {
		var c candidate
		if e = rows.Scan(&c.id, &c.kind); e != nil {
			rows.Close()
			return report, e
		}
		candidates = append(candidates, c)
		if c.kind == 1 {
			report.Personal.Candidates++
		} else {
			report.System.Candidates++
		}
	}
	e = rows.Err()
	rows.Close()
	if e != nil {
		return report, e
	}
	// 非法行只计有界样本，不修复也不删除。
	e = r.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM (SELECT expire_ms FROM mail_message ORDER BY mail_id LIMIT ?) sample WHERE expire_ms=0", limit).Scan(&report.Personal.Invalid)
	if e != nil {
		return report, e
	}
	if !deleteRows {
		return report, nil
	}
	remaining := limit
	for _, c := range candidates {
		if remaining == 0 {
			break
		}
		tx, e := r.db.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
		if e != nil {
			return report, e
		}
		var locked uint64
		e = tx.QueryRowContext(ctx, "SELECT m.mail_id FROM mail_message m WHERE m.mail_id=? AND "+eligible+" FOR UPDATE", c.id, cutoff).Scan(&locked)
		if errors.Is(e, sql.ErrNoRows) {
			_ = tx.Rollback()
			continue
		}
		if e != nil {
			_ = tx.Rollback()
			return report, e
		}
		// 保留期至少一天，已过期请求不再能创建新的领取；仍在途和人工状态在同一事务重新复核。
		deletedStates, e := tx.ExecContext(ctx, "DELETE FROM mail_state WHERE mail_id=? AND claim_state IN (0,1,3) LIMIT ?", c.id, remaining)
		if e != nil {
			_ = tx.Rollback()
			return report, e
		}
		n, e := deletedStates.RowsAffected()
		if e != nil {
			_ = tx.Rollback()
			return report, e
		}
		used := int(n)
		var removed int64
		if used < remaining {
			result, e := tx.ExecContext(ctx, "DELETE FROM mail_message WHERE mail_id=? AND expire_ms>0 AND expire_ms<? AND campaign_id=0 AND NOT EXISTS(SELECT 1 FROM mail_state s WHERE s.mail_id=?) AND NOT EXISTS(SELECT 1 FROM mail_asset_op o WHERE o.mail_id=? AND (o.status IS NULL OR o.status NOT IN (2,3,5)))", c.id, cutoff, c.id, c.id)
			if e != nil {
				_ = tx.Rollback()
				return report, e
			}
			removed, e = result.RowsAffected()
			if e != nil {
				_ = tx.Rollback()
				return report, e
			}
			used += int(removed)
		}
		if e = tx.Commit(); e != nil {
			return report, e
		}
		remaining -= used
		report.SystemState.Deleted += uint64(n)
		if c.kind == 1 {
			report.Personal.Deleted += uint64(removed)
		} else {
			report.System.Deleted += uint64(removed)
		}
	}
	return report, nil
}
