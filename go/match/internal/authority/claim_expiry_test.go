package authority

import (
	"context"
	"database/sql"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
	pb "proto/match"
)

func TestAuthorityTiDBClaimRechecksTicketExpiryAndRollsBackWholeGroup(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	a, b := testJoin(t, s, 1), testJoin(t, s, 2)
	// 保留候选扫描得到的原票和仍有效的机房回执，只让 SQL 里的票在成局前到期。
	// 这也覆盖多个回执有效期不同时，所选机房尚未过期而整份 profile 已到期的情形。
	_, err := s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET expires_ms=1 WHERE player_id=?", b.PlayerId)
	require.NoError(t, err)
	f := &pb.MatchFormationRecord{BattleId: 201, OperationId: uuid.NewString(), PhysicalRegion: 1,
		TargetNodeId: 3, TargetInstanceId: "instance", TargetEndpoint: "127.0.0.1:1", Mode: 3, ConfigId: 1}
	require.Error(t, s.Claim(ctx, f, []*pb.MatchAuthorityTicketRecord{a, b}))
	// Ticket 会主动终结过期票；直接只读 SQL 才能观察 Claim 的原子回滚结果。
	for _, original := range []*pb.MatchAuthorityTicketRecord{a, b} {
		var state uint32
		var formation uint64
		var ticket string
		err := s.DB.QueryRowContext(ctx, "SELECT state,formation_id,ticket FROM match_authority_ticket WHERE player_id=?", original.PlayerId).Scan(&state, &formation, &ticket)
		require.NoError(t, err)
		require.Equal(t, uint32(1), state)
		require.Zero(t, formation)
		require.Equal(t, original.Ticket, ticket)
	}
	_, err = s.Formation(ctx, f.BattleId)
	require.ErrorIs(t, err, sql.ErrNoRows)
}
