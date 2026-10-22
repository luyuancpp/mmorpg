package authority

import (
	"context"
	"os"
	"sync"
	"testing"
	"time"

	pb "proto/match"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

func activityCancelStore(t *testing.T) *Store {
	t.Helper()
	if os.Getenv("MATCH_REQUIRE_TIDB") == "1" && os.Getenv("MATCH_IT_TIDB") == "" {
		t.Fatal("缺少MATCH_IT_TIDB")
	}
	return testStore(t)
}
func TestActivityCancelRejectsInvalidOriginalBeforeStorage(t *testing.T) {
	s := &Store{}
	for _, key := range []*pb.MatchDirectIntentRecord{nil, {Kind: 3, OwnerId: 1, RequestLow: 1}, {Kind: 2, OwnerId: 0, RequestLow: 1}, {Kind: 2, OwnerId: 1}} {
		_, err := s.CancelActivity(context.Background(), key, []byte("original"))
		require.Error(t, err)
	}
}
func TestAuthorityTiDBActivityCancelTombstoneRejectsLateStartAndChangedBody(t *testing.T) {
	s := activityCancelStore(t)
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	key := &pb.MatchDirectIntentRecord{Kind: 2, OwnerId: 44, RequestLow: 51}
	body := []byte("entire-original-start")
	first, err := s.CancelActivity(ctx, key, body)
	require.NoError(t, err)
	require.True(t, first.RequestCancelled)
	require.Zero(t, first.BattleId)
	_, err = s.ReserveDirect(ctx, key, body, &pb.MatchFormationRecord{BattleId: 900}, []*pb.MatchAuthorityTicketRecord{{PlayerId: 11}, {PlayerId: 22}})
	require.ErrorIs(t, err, ErrDirectCancelled)
	_, _, err = s.FindDirect(ctx, key)
	require.ErrorIs(t, err, ErrDirectCancelled)
	replayed, err := s.CancelActivity(ctx, key, body)
	require.NoError(t, err)
	require.Equal(t, first, replayed)
	_, err = s.CancelActivity(ctx, key, []byte("changed-roster"))
	require.ErrorIs(t, err, ErrDirectConflict)
	var formations, tickets int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_formation").Scan(&formations))
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_authority_ticket").Scan(&tickets))
	require.Zero(t, formations)
	require.Zero(t, tickets)
}
func TestAuthorityTiDBActivityCancelAndReserveHaveOneCommitDecision(t *testing.T) {
	s := activityCancelStore(t)
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	for n := uint64(1); n <= 8; n++ {
		a, b := testJoin(t, s, 100+n*2), testJoin(t, s, 101+n*2)
		require.NoError(t, s.Cancel(ctx, a.PlayerId, a.Ticket))
		require.NoError(t, s.Cancel(ctx, b.PlayerId, b.Ticket))
		key := &pb.MatchDirectIntentRecord{Kind: 2, OwnerId: 44, RequestLow: 1000 + n}
		body := []byte("same-entire-start")
		formation := &pb.MatchFormationRecord{BattleId: 900 + n, OperationId: uuid.NewString(), PhysicalRegion: 1, TargetNodeId: 3, TargetInstanceId: "instance", TargetEndpoint: "endpoint", Mode: 5, ConfigId: 1}
		a.Mode, b.Mode = formation.Mode, formation.Mode
		start := make(chan struct{})
		var wg sync.WaitGroup
		var reserveErr, cancelErr error
		var receipt *pb.CancelActivityBattleResponse
		wg.Add(2)
		go func() {
			defer wg.Done()
			<-start
			_, reserveErr = s.ReserveDirect(ctx, key, body, formation, []*pb.MatchAuthorityTicketRecord{a, b})
		}()
		go func() { defer wg.Done(); <-start; receipt, cancelErr = s.CancelActivity(ctx, key, body) }()
		close(start)
		wg.Wait()
		require.NoError(t, cancelErr)
		if receipt.RequestCancelled {
			require.ErrorIs(t, reserveErr, ErrDirectCancelled)
			require.Zero(t, receipt.BattleId)
		} else {
			require.NoError(t, reserveErr)
			require.Equal(t, formation.BattleId, receipt.BattleId)
			require.EqualValues(t, 1, receipt.FormationState)
			work, err := s.AcquireWork(ctx, formation.BattleId)
			require.NoError(t, err)
			require.NoError(t, s.Finish(ctx, work, false))
			again, err := s.CancelActivity(ctx, key, body)
			require.NoError(t, err)
			require.False(t, again.RequestCancelled)
			require.Equal(t, formation.BattleId, again.BattleId)
			require.EqualValues(t, 5, again.FormationState, "只报告实际已安全中止状态，不把Cancel请求当中止证明")
		}
	}
}
