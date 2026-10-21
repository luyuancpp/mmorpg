package authority

import (
	"bytes"
	"context"
	"database/sql"
	"errors"

	pb "proto/match"
)

var ErrDirectCancelled = errors.New("原活动开战请求已持久取消")
var ErrDirectConflict = errors.New("原直接开局请求正文冲突")

// CancelActivity与ReserveDirect竞争同一PK锁。零battle墓碑永久保留；已有成局仅报告固定原ID/真实状态。
// 事务内不调用运行时，Commit未知时返回错误，不给调用方伪造安全撤销证明。
func (s *Store) CancelActivity(ctx context.Context, key *pb.MatchDirectIntentRecord, payload []byte) (*pb.CancelActivityBattleResponse, error) {
	if s == nil || s.DB == nil || key == nil || key.Kind != 2 || key.OwnerId == 0 || key.RequestHigh != 0 || key.RequestLow == 0 || len(payload) == 0 || len(payload) > 64*1024 {
		return nil, errors.New("活动取消原件无效或权威未装配")
	}
	// 首次读与Start可竞争；只有发现刚提交的新原件时重取一次完整P集合，
	// 原取消PK永久保留，因此第二次不会再次由缺失变为存在。
	for attempt := 0; attempt < 2; attempt++ {
		out, changed, err := s.cancelActivityAttempt(ctx, key, payload)
		if err != nil || !changed {
			return out, err
		}
	}
	return nil, errors.New("活动原件并发变化，需复用原取消请求重试")
}

func (s *Store) cancelActivityAttempt(ctx context.Context, key *pb.MatchDirectIntentRecord, payload []byte) (*pb.CancelActivityBattleResponse, bool, error) {
	observed, err := s.DirectAdmission(ctx, key)
	if err != nil {
		return nil, false, err
	}
	var start *pb.MatchDirectStartOriginal
	if observed != nil && observed.ExitPrepared {
		if start, err = DecodeDirectStart(observed); err != nil {
			return nil, false, err
		}
	}
	tx, err := s.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return nil, false, err
	}
	defer tx.Rollback()
	if start != nil {
		if err = s.lockOriginalPlayers(ctx, tx, start.Tickets); err != nil {
			return nil, false, err
		}
	}
	if _, err = tx.ExecContext(ctx, "INSERT IGNORE INTO match_direct_intent(kind,owner_id,request_high,request_low,payload,battle_id,cancelled,owner_account,owner_revision) VALUES(?,?,?,?,?,0,1,'',0)", key.Kind, key.OwnerId, key.RequestHigh, key.RequestLow, payload); err != nil {
		return nil, false, err
	}
	current, err := readDirectTx(ctx, tx, key)
	if err != nil {
		return nil, false, err
	}
	if !bytes.Equal(current.Payload, payload) {
		return nil, false, ErrDirectConflict
	}
	if current.ExitPrepared && (start == nil || !bytes.Equal(current.StartOriginal, observed.StartOriginal)) {
		return nil, true, nil
	}
	battle, cancelled := current.BattleId, current.Cancelled
	out := &pb.CancelActivityBattleResponse{BattleId: battle}
	if battle == 0 {
		if start != nil {
			err = closeDirectPendingTx(ctx, tx, current, start.Tickets)
		} else {
			_, err = tx.ExecContext(ctx, "UPDATE match_direct_intent SET cancelled=1 WHERE kind=? AND owner_id=? AND request_high=? AND request_low=?", key.Kind, key.OwnerId, key.RequestHigh, key.RequestLow)
		}
		if err != nil {
			return nil, false, err
		}
		out.RequestCancelled = true
	} else {
		if cancelled {
			return nil, false, errors.New("已取消活动意图却带有成局，保留原件等待处理")
		}
		if err = tx.QueryRowContext(ctx, "SELECT state FROM match_formation WHERE battle_id=? FOR UPDATE", battle).Scan(&out.FormationState); err != nil {
			return nil, false, err
		}
		if out.FormationState < 1 || out.FormationState > 5 {
			return nil, false, errors.New("活动原成局状态非法")
		}
	}
	if err = tx.Commit(); err != nil {
		return nil, false, err
	}
	return out, false, nil
}
