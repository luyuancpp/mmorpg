package authority

import (
	"bytes"
	"context"
	"crypto/sha256"
	"database/sql"
	"errors"
	"fmt"
	"math"
	"sort"

	"github.com/go-sql-driver/mysql"
	"google.golang.org/protobuf/proto"
	rankpb "proto/rank"
)

// Rating 默认1500.00。SQL不存在与读故障严格区分，由只影响凑单质量的调用方决定是否用缺省。
func (s *Store) Rating(ctx context.Context, pid uint64) (int64, error) {
	var rating int64
	e := s.DB.QueryRowContext(ctx, "SELECT rating_centi FROM match_rating WHERE player_id=?", pid).Scan(&rating)
	if errors.Is(e, sql.ErrNoRows) {
		return 150000, nil
	}
	return rating, e
}

// ApplyRating 把整局评级、重复结果摘要、连续投榜游标及 outbox 放在同一 SQL 提交点。
// 锁序：本局applied行→全局代次guard→player_id升序。结果未知重放同battle/digest，不会再加一局。
func (s *Store) ApplyRating(ctx context.Context, battle uint64, payload proto.Message, mode uint32, teamA, teamB []uint64, scoreA float64) (bool, error) {
	if battle == 0 || len(teamA) == 0 || len(teamB) == 0 {
		return false, errors.New("评级结果缺少对局/队伍")
	}
	encoded, e := proto.MarshalOptions{Deterministic: true}.Marshal(payload)
	if e != nil {
		return false, e
	}
	digest := sha256.Sum256(encoded)
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return false, e
	}
	defer tx.Rollback()
	stamp, e := now(ctx, tx)
	if e != nil {
		return false, e
	}
	_, e = tx.ExecContext(ctx, "INSERT INTO match_rating_applied(battle_id,payload_digest,applied_ms) VALUES(?,?,?)", battle, digest[:], stamp)
	if e != nil {
		var duplicate *mysql.MySQLError
		if !errors.As(e, &duplicate) || duplicate.Number != 1062 {
			return false, e
		}
		var prior []byte
		if e = tx.QueryRowContext(ctx, "SELECT payload_digest FROM match_rating_applied WHERE battle_id=?", battle).Scan(&prior); e != nil {
			return false, e
		}
		if !bytes.Equal(prior, digest[:]) {
			return false, errors.New("同对局重复结果内容不一致")
		}
		return false, nil
	}
	if _, e = tx.ExecContext(ctx, "INSERT INTO match_rank_source(source_epoch,produced_cursor,committed_cursor,board_id,stopped) VALUES(1,0,0,0,0) ON DUPLICATE KEY UPDATE source_epoch=source_epoch"); e != nil {
		return false, e
	}
	var cursor uint64
	var stopped bool
	if e = tx.QueryRowContext(ctx, "SELECT produced_cursor,stopped FROM match_rank_source WHERE source_epoch=1 FOR UPDATE").Scan(&cursor, &stopped); e != nil {
		return false, e
	}
	if stopped {
		return false, errors.New("评级代次已停写，必须显式协调后开启新代")
	}
	members := append(append([]uint64{}, teamA...), teamB...)
	sort.Slice(members, func(i, j int) bool { return members[i] < members[j] })
	ratings := map[uint64]int64{}
	for i, pid := range members {
		if pid == 0 || i > 0 && members[i-1] == pid {
			return false, errors.New("评级名单为空或重复")
		}
		if _, e = tx.ExecContext(ctx, "INSERT INTO match_rating(player_id,rating_centi,games,updated_ms) VALUES(?,150000,0,?) ON DUPLICATE KEY UPDATE player_id=player_id", pid, stamp); e != nil {
			return false, e
		}
		var rating int64
		if e = tx.QueryRowContext(ctx, "SELECT rating_centi FROM match_rating WHERE player_id=? FOR UPDATE", pid).Scan(&rating); e != nil {
			return false, e
		}
		ratings[pid] = rating
	}
	average := func(team []uint64) float64 {
		sum := 0.
		for _, pid := range team {
			sum += float64(ratings[pid]) / 100
		}
		return sum / float64(len(team))
	}
	delta := int64(math.Round(3200 * (scoreA - 1/(1+math.Pow(10, (average(teamB)-average(teamA))/400)))))
	sides := map[uint64]bool{}
	for _, pid := range teamA {
		sides[pid] = true
	}
	batch := &rankpb.SubmitRankScoresRequest{Generation: 1, SourceEpoch: 1, SourceCursor: cursor + 1, RequestId: battle, Mode: rankpb.RankSubmitMode_RANK_SUBMIT_SET}
	for _, pid := range members {
		change := -delta
		if sides[pid] {
			change = delta
		}
		next := max(int64(0), ratings[pid]+change)
		if _, e = tx.ExecContext(ctx, "UPDATE match_rating SET rating_centi=?,games=games+1,updated_ms=? WHERE player_id=?", next, stamp, pid); e != nil {
			return false, e
		}
		batch.Entries = append(batch.Entries, &rankpb.RankScoreInput{EntityId: pid, Score: next, AchievedMs: stamp})
	}
	body, e := proto.MarshalOptions{Deterministic: true}.Marshal(batch)
	if e != nil {
		return false, e
	}
	if _, e = tx.ExecContext(ctx, "INSERT INTO match_rank_outbox(source_epoch,source_cursor,request_id,entries) VALUES(1,?,?,?)", cursor+1, battle, body); e != nil {
		return false, e
	}
	if _, e = tx.ExecContext(ctx, "UPDATE match_rank_source SET produced_cursor=? WHERE source_epoch=1", cursor+1); e != nil {
		return false, e
	}
	if e = tx.Commit(); e != nil {
		return false, fmt.Errorf("评级提交结果未知，按原局重放: %w", e)
	}
	return true, nil
}

// StopRankSource 与每次评级争同一guard行锁：返回值就是该代真实最终游标，不能用时间猜测。
func (s *Store) StopRankSource(ctx context.Context, epoch uint64) (uint64, error) {
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return 0, e
	}
	defer tx.Rollback()
	var cursor uint64
	if e = tx.QueryRowContext(ctx, "SELECT produced_cursor FROM match_rank_source WHERE source_epoch=? FOR UPDATE", epoch).Scan(&cursor); e != nil {
		return 0, e
	}
	if _, e = tx.ExecContext(ctx, "UPDATE match_rank_source SET stopped=1 WHERE source_epoch=?", epoch); e != nil {
		return 0, e
	}
	return cursor, tx.Commit()
}
