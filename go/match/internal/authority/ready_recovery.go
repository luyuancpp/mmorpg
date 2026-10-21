package authority

import (
	"context"
	"database/sql"
	"errors"

	pb "proto/match"
)

// ReadyAfter 的游标仅调度巡检，不能授权释放。即使前面的房间一直 UNKNOWN，
// 下一页也照常访问；进程重启从头扫描不会改变任何票据的业务状态。
func (s *Store) ReadyAfter(ctx context.Context, afterMs, afterPlayer uint64) ([]*pb.MatchAuthorityTicketRecord, error) {
	rows, err := s.DB.QueryContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE state=3 AND (enqueued_ms>? OR (enqueued_ms=? AND player_id>?)) ORDER BY enqueued_ms,player_id LIMIT 8", afterMs, afterMs, afterPlayer)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []*pb.MatchAuthorityTicketRecord
	for rows.Next() {
		ticket, err := scanTicket(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, ticket)
	}
	return out, rows.Err()
}

// ReleaseCompletedReady 只认已归档且校验完整的本玩家结算原件，不使用客户端
// 状态、缓存或超时猜测房间结束。原角色P锁先于票锁；旧巡检不能释放新一轮票。
func (s *Store) ReleaseCompletedReady(ctx context.Context, observed *pb.MatchAuthorityTicketRecord) (bool, error) {
	if observed == nil || observed.State != 3 || observed.FormationId == 0 || observed.Ticket == "" {
		return false, nil
	}
	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return false, err
	}
	defer tx.Rollback()
	if err = s.lockOriginalPlayers(ctx, tx, []*pb.MatchAuthorityTicketRecord{observed}); err != nil {
		return false, err
	}
	var battle uint64
	err = tx.QueryRowContext(ctx, "SELECT battle_id FROM match_battle_settlement_source WHERE battle_id=? AND player_id=?", observed.FormationId, observed.PlayerId).Scan(&battle)
	if errors.Is(err, sql.ErrNoRows) {
		return false, nil
	}
	if err != nil {
		return false, err
	}
	result, err := tx.ExecContext(ctx, "UPDATE match_authority_ticket SET state=4 WHERE player_id=? AND ticket=? AND formation_id=? AND state=3 AND owner_account=? AND owner_revision=?", observed.PlayerId, observed.Ticket, observed.FormationId, observed.OwnerAccount, observed.OwnerRevision)
	if err != nil {
		return false, err
	}
	changed, err := result.RowsAffected()
	if err != nil {
		return false, err
	}
	if err = tx.Commit(); err != nil {
		return false, err
	}
	return changed != 0, nil
}
