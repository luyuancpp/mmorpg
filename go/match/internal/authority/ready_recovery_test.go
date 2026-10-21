package authority

import (
	"context"
	"testing"

	"github.com/stretchr/testify/require"
)

func TestReadyTiDBScanDoesNotStarveAfterUnknownPrefix(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	for pid := uint64(1); pid <= 19; pid++ {
		ticket := testJoin(t, s, pid)
		_, err := s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET state=3,formation_id=?,enqueued_ms=100 WHERE player_id=? AND ticket=?", 900+pid, pid, ticket.Ticket)
		require.NoError(t, err)
	}
	var seen []uint64
	var afterMs, afterPlayer uint64
	for page := 0; page < 4; page++ {
		rows, err := s.ReadyAfter(ctx, afterMs, afterPlayer)
		require.NoError(t, err)
		require.LessOrEqual(t, len(rows), 8)
		for _, row := range rows {
			// 不释放任何前项，模拟其权威房间一直UNKNOWN。
			seen = append(seen, row.PlayerId)
			afterMs, afterPlayer = row.EnqueuedMs, row.PlayerId
		}
	}
	require.Len(t, seen, 19)
	require.Equal(t, uint64(19), seen[len(seen)-1])
}

func TestReadyTiDBCompletionEvidenceAndOriginalTicketCAS(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	ticket := testJoin(t, s, 11)
	ticket.State, ticket.FormationId = 3, 901
	_, err := s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET state=3,formation_id=901 WHERE player_id=11")
	require.NoError(t, err)
	released, err := s.ReleaseCompletedReady(ctx, ticket)
	require.NoError(t, err)
	require.False(t, released, "未收到真实结算原件时不能按时间释放")
	event := completeResult()
	require.NoError(t, s.ArchiveCompletion(ctx, event, resultBytes(t, event), "match-results-fixture", 0, 1))
	released, err = s.ReleaseCompletedReady(ctx, ticket)
	require.NoError(t, err)
	require.True(t, released)
	// 同一个玩家下一轮已持有另一原票，旧扫描不能释放它。
	_, err = s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET state=3,ticket='next-round',formation_id=902 WHERE player_id=11")
	require.NoError(t, err)
	released, err = s.ReleaseCompletedReady(ctx, ticket)
	require.NoError(t, err)
	require.False(t, released)
	var state uint32
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT state FROM match_authority_ticket WHERE player_id=11").Scan(&state))
	require.Equal(t, uint32(3), state)
}
