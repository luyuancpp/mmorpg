package data

import (
	"context"
	"strings"

	rankpb "proto/rank"
)

func (r *Repo) lookup(ctx context.Context, key *rankpb.RankBoardKey) (*rankpb.RankBoardRecord, error) {
	if key == nil || key.BoardType == 0 || key.Scope < rankpb.RankScope_RANK_SCOPE_GLOBAL || key.Scope > rankpb.RankScope_RANK_SCOPE_CUSTOM || len(key.Period) > 64 {
		return nil, ErrInvalid
	}
	query := "SELECT " + boardColumns + " FROM rank_board WHERE board_type=? AND scope=? AND scope_id=? AND deleted_ms=0"
	args := []any{key.BoardType, uint32(key.Scope), key.ScopeId}
	if key.Period!=""{query+=" AND period=?";args=append(args,key.Period)}
	if key.Generation > 0 {
		query += " AND generation=?"
		args = append(args, key.Generation)
	}
	if key.Period==""{query+=" ORDER BY created_ms DESC,board_id DESC LIMIT 1"}else{query += " ORDER BY generation DESC LIMIT 1"}
	return scanBoard(r.db.QueryRowContext(ctx, query, args...))
}
func boardKey(b *rankpb.RankBoardRecord) *rankpb.RankBoardKey {
	return &rankpb.RankBoardKey{BoardType: b.BoardType, Scope: rankpb.RankScope(b.Scope), ScopeId: b.ScopeId, Period: b.Period, Generation: b.Generation}
}
func rankOrder(b *rankpb.RankBoardRecord) string {
	if b.Ascending != 0 {
		return "score ASC,achieved_ms ASC,entity_id ASC"
	}
	return "score DESC,achieved_ms ASC,entity_id ASC"
}

// Page的页、总数和本人来自同一条只读SQL快照；不会拼出旧总数配新名次。
func (r *Repo) Page(ctx context.Context, player uint64, in *rankpb.GetRankPageRequest) (*rankpb.GetRankPageResponse, error) {
	if in == nil || player == 0 {
		return nil, ErrInvalid
	}
	b, err := r.lookup(ctx, in.Board)
	if err != nil {
		return nil, err
	}
	size := in.PageSize
	if size == 0 {
		size = 20
	}
	if size > 50 {
		size = 50
	}
	page := in.Page
	if page == 0 {
		page = 1
	}
	query := `WITH numbered AS (
SELECT entity_id,score,achieved_ms,ROW_NUMBER() OVER (ORDER BY $ORDER) AS place
FROM rank_score WHERE board_id=? AND generation=?),
totals AS (SELECT COUNT(*) AS total FROM numbered),
bounds AS (SELECT total,GREATEST(1,CEIL(total / ?)) AS pages,
LEAST(?,GREATEST(1,CEIL(total / ?))) AS selected_page FROM totals)
SELECT b.state,p.total,p.pages,p.selected_page,COALESCE(n.entity_id,0),COALESCE(n.score,0),COALESCE(n.achieved_ms,0),COALESCE(n.place,0)
FROM rank_board b CROSS JOIN bounds p LEFT JOIN numbered n
ON (n.place BETWEEN (p.selected_page-1)*?+1 AND p.selected_page*? OR n.entity_id=?)
WHERE b.board_id=? AND b.generation=? AND b.deleted_ms=0 ORDER BY n.place`
	query = strings.Replace(query, "$ORDER", rankOrder(b), 1)
	rows, err := r.db.QueryContext(ctx, query, b.BoardId, b.Generation, size, page, size, size, size, player, b.BoardId, b.Generation)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	result := &rankpb.GetRankPageResponse{Board: boardKey(b), Page: 1, PageSize: size, PageCount: 1}
	for rows.Next() {
		entry := &rankpb.RankEntry{}
		var state uint32
		var selected uint64
		if err := rows.Scan(&state, &result.TotalCount, &result.PageCount, &selected, &entry.EntityId, &entry.Score, &entry.AchievedMs, &entry.Rank); err != nil {
			return nil, err
		}
		result.State = rankpb.RankGenerationState(state)
		result.Page = uint32(selected)
		if entry.Rank == 0 {
			continue
		}
		if entry.EntityId == player {
			result.Self = entry
		}
		if entry.Rank >= (selected-1)*uint64(size)+1 && entry.Rank <= selected*uint64(size) {
			result.Entries = append(result.Entries, entry)
		}
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	return result, nil
}

func (r *Repo) Around(ctx context.Context, player uint64, in *rankpb.GetRankAroundRequest) (*rankpb.GetRankAroundResponse, error) {
	if in == nil || player == 0 {
		return nil, ErrInvalid
	}
	b, err := r.lookup(ctx, in.Board)
	if err != nil {
		return nil, err
	}
	radius := in.Radius
	if radius == 0 {
		radius = 10
	}
	if radius > 25 {
		radius = 25
	}
	query := `WITH numbered AS (SELECT entity_id,score,achieved_ms,ROW_NUMBER() OVER (ORDER BY $ORDER) AS place FROM rank_score WHERE board_id=? AND generation=?),
mine AS (SELECT place FROM numbered WHERE entity_id=?)
SELECT entity_id,score,achieved_ms,place FROM numbered WHERE place BETWEEN GREATEST(1,(SELECT place FROM mine)-?) AND (SELECT place FROM mine)+? ORDER BY place`
	query = strings.Replace(query, "$ORDER", rankOrder(b), 1)
	rows, err := r.db.QueryContext(ctx, query, b.BoardId, b.Generation, player, radius, radius)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	result := &rankpb.GetRankAroundResponse{Board: boardKey(b)}
	for rows.Next() {
		entry := &rankpb.RankEntry{}
		if err := rows.Scan(&entry.EntityId, &entry.Score, &entry.AchievedMs, &entry.Rank); err != nil {
			return nil, err
		}
		result.Entries = append(result.Entries, entry)
		if entry.EntityId == player {
			result.Self = entry
		}
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	return result, nil
}
