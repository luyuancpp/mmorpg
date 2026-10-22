package data

import (
	"context"
	"database/sql"

	rankpb "proto/rank"
	"rank/internal/metrics"
	"shared/serviceauth"
)

func (r *Repo) Remove(ctx context.Context, caller string, in *rankpb.RemoveRankEntryRequest, hide bool) (err error) {
	operation := metrics.OperationRemove
	if hide {
		operation = metrics.OperationDelete
	}
	replay := false
	defer func() { observeWrite(operation, err, replay) }()
	if in == nil || in.BoardId == 0 || in.Generation == 0 || in.RequestId == 0 || (hide && in.EntityId != 0) || (!hide && in.EntityId == 0) {
		return ErrInvalid
	}
	digest, err := serviceauth.Digest(in)
	if err != nil {
		return err
	}
	// 同一request_id不能把删除成员与隐藏整代混为同一操作。
	if hide {
		digest = "hide:" + digest
	} else {
		digest = "remove:" + digest
	}
	return r.tx(ctx, func(tx *sql.Tx) error {
		b, err := scanBoard(tx.QueryRowContext(ctx, "SELECT "+boardColumns+" FROM rank_board WHERE board_id=? FOR UPDATE", in.BoardId))
		if err != nil {
			return err
		}
		if b.Producer != caller {
			return ErrDenied
		}
		if b.Generation != in.Generation {
			return ErrConflict
		}
		var existing string
		err = tx.QueryRowContext(ctx, "SELECT request_hash FROM rank_mutation WHERE board_id=? AND generation=? AND request_id=?", in.BoardId, in.Generation, in.RequestId).Scan(&existing)
		if err == nil {
			if existing != digest {
				return ErrConflict
			}
			replay = true
			return nil
		}
		if err != sql.ErrNoRows {
			return err
		}
		if !hide && (b.State != uint32(rankpb.RankGenerationState_RANK_GENERATION_OPEN) || b.DeletedMs != 0) {
			return ErrClosed
		}
		if hide {
			_, err = tx.ExecContext(ctx, "UPDATE rank_board SET deleted_ms=? WHERE board_id=?", uint64(r.now().UnixMilli()), b.BoardId)
		} else {
			_, err = tx.ExecContext(ctx, "DELETE FROM rank_score WHERE board_id=? AND generation=? AND entity_id=?", b.BoardId, b.Generation, in.EntityId)
		}
		if err != nil {
			return err
		}
		_, err = tx.ExecContext(ctx, "INSERT INTO rank_mutation (board_id,generation,request_id,request_hash,created_ms) VALUES (?,?,?,?,?)", b.BoardId, b.Generation, in.RequestId, digest, uint64(r.now().UnixMilli()))
		return err
	})
}
