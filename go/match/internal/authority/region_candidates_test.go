package authority

import (
	"context"
	"database/sql"
	"math"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	pb "proto/match"
)

func fixtureRegionTicket(t *testing.T, s *Store, id, enqueued, now uint64, first, second uint32) *pb.MatchAuthorityTicketRecord {
	t.Helper()
	ticket := testJoin(t, s, id)
	profile := &pb.MatchNetworkProfile{PlanId: "region-fixture", SessionBinding: "bound"}
	for region, rtt := range map[uint32]uint32{1: first, 2: second} {
		profile.Receipts = append(profile.Receipts, &pb.NetworkProbeReceipt{Grant: &pb.NetworkProbeGrant{PlayerId: id, PhysicalRegion: region, ExpiresAtMs: now + 120000}, RttMs: rtt, SampleCount: 3, MeasuredAtMs: now, ExpiresAtMs: now + 120000})
	}
	payload, err := proto.Marshal(profile)
	require.NoError(t, err)
	ticket.NetworkProfile, ticket.ExpiresMs, ticket.EnqueuedMs = payload, now+120000, enqueued
	tx, err := s.DB.BeginTx(context.Background(), nil)
	require.NoError(t, err)
	defer tx.Rollback()
	_, err = tx.Exec("UPDATE match_authority_ticket SET network_profile=?,expires_ms=?,enqueued_ms=? WHERE player_id=?", payload, ticket.ExpiresMs, enqueued, id)
	require.NoError(t, err)
	require.NoError(t, s.replaceTicketRegions(context.Background(), tx, ticket, now))
	require.NoError(t, tx.Commit())
	return ticket
}

func TestAuthorityTiDBRegionRepairFixedHighWaterIgnoresLaterJoins(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	stamp, err := s.Now(ctx)
	require.NoError(t, err)
	for player := uint64(1); player <= 3; player++ {
		fixtureRegionTicket(t, s, player, stamp, stamp, 20, 80)
	}
	throughMs, throughPlayer, err := s.CandidateRegionRepairHighWater(ctx)
	require.NoError(t, err)
	require.Equal(t, uint64(3), throughPlayer)
	_, err = s.DB.ExecContext(ctx, "DELETE FROM match_ticket_region WHERE player_id<=3")
	require.NoError(t, err)
	for player := uint64(4); player <= 40; player++ {
		fixtureRegionTicket(t, s, player, stamp+1, stamp, 20, 80)
	}
	nextMs, nextPlayer, err := s.RepairCandidateRegionsThrough(ctx, 0, 0, throughMs, throughPlayer)
	require.NoError(t, err)
	require.Equal(t, throughMs, nextMs)
	require.Equal(t, throughPlayer, nextPlayer)
	nextMs, nextPlayer, err = s.RepairCandidateRegionsThrough(ctx, nextMs, nextPlayer, throughMs, throughPlayer)
	require.NoError(t, err)
	require.Zero(t, nextMs)
	require.Zero(t, nextPlayer, "持续新Join不能延长旧票修复尾界")
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_ticket_region WHERE player_id<=3").Scan(&count))
	require.Equal(t, 6, count)
}

func TestAuthorityTiDBRegionPeersCrossArbitraryAnchorPages(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	n, err := s.Now(ctx)
	require.NoError(t, err)
	a := fixtureRegionTicket(t, s, 1, n-60000, n, 20, 100)
	b := fixtureRegionTicket(t, s, 2, n-59999, n, 100, 20)
	c := fixtureRegionTicket(t, s, 3, n-1000, n, 30, 120)
	// 其它lane的长期等待者把本区伙伴推到第十个锚点页；不能充当本区人数不足证明。
	tx, err := s.DB.BeginTx(ctx, nil)
	require.NoError(t, err)
	defer tx.Rollback()
	for id := uint64(1000); id < 1300; id++ {
		_, err = tx.ExecContext(ctx, "INSERT INTO match_authority_ticket("+ticketColumns+") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", id, uuid.NewString(), uuid.NewString(), a.Mode, 999, a.TeamSize, 1, n-50000, 0, 0, a.RatingCenti, 0, 0, a.NetworkProfile, a.ExpiresMs, 0, a.PreferredRegion, "", 0)
		require.NoError(t, err)
	}
	require.NoError(t, tx.Commit())
	anchors, err := s.Candidates(ctx)
	require.NoError(t, err)
	require.Len(t, anchors, 32)
	for _, anchor := range anchors {
		require.NotEqual(t, c.PlayerId, anchor.PlayerId)
	}
	local, err := s.CandidatePeers(ctx, a, 1, true, n, math.Inf(1))
	require.NoError(t, err)
	require.Len(t, local, 1)
	require.Equal(t, c.PlayerId, local[0].PlayerId)
	// 直接查扩区也只能拿自身等待满阈值的b，不能把年轻c带出本区。
	cross, err := s.CandidatePeers(ctx, a, 1, false, n, math.Inf(1))
	require.NoError(t, err)
	require.Len(t, cross, 1)
	require.Equal(t, b.PlayerId, cross[0].PlayerId)
	// 改硬上限无需重建投影；b到region1的100ms不再满足80ms。
	s.Config.HardLatencyMs = 80
	cross, err = s.CandidatePeers(ctx, a, 1, false, n, math.Inf(1))
	require.NoError(t, err)
	require.Empty(t, cross)
}

func TestAuthorityTiDBRegionProjectionRebuildAndRejectOldTicket(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	n, err := s.Now(ctx)
	require.NoError(t, err)
	a := fixtureRegionTicket(t, s, 1, n-60000, n, 20, 100)
	b := fixtureRegionTicket(t, s, 2, n-50000, n, 30, 120)
	require.NoError(t, s.CheckCandidateIndexes(ctx))
	_, err = s.DB.ExecContext(ctx, "DELETE FROM match_ticket_region WHERE player_id=?", b.PlayerId)
	require.NoError(t, err)
	missing, err := s.CandidatePeers(ctx, a, 1, true, n, math.Inf(1))
	require.NoError(t, err)
	require.Empty(t, missing)
	_, _, err = s.RepairCandidateRegions(ctx, 0, 0)
	require.NoError(t, err)
	restored, err := s.CandidatePeers(ctx, a, 1, true, n, math.Inf(1))
	require.NoError(t, err)
	require.Len(t, restored, 1)
	require.Equal(t, b.Ticket, restored[0].Ticket)
	// 投影中保留旧代次不能授权新票；最终Claim之外，候选读取本身也必须拒旧索引。
	_, err = s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET ticket=? WHERE player_id=?", uuid.NewString(), b.PlayerId)
	require.NoError(t, err)
	stale, err := s.CandidatePeers(ctx, a, 1, true, n, math.Inf(1))
	require.NoError(t, err)
	require.Empty(t, stale)
}

func TestAuthorityTiDBRegionProjectionFollowsCancelClaimAndRequeue(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	n, err := s.Now(ctx)
	require.NoError(t, err)
	a := fixtureRegionTicket(t, s, 1, n-60000, n, 20, 100)
	b := fixtureRegionTicket(t, s, 2, n-50000, n, 30, 120)
	f := &pb.MatchFormationRecord{BattleId: 100, OperationId: uuid.NewString(), PhysicalRegion: 1, TargetNodeId: 3, TargetInstanceId: "instance", TargetEndpoint: "127.0.0.1:1", Mode: a.Mode, ConfigId: a.ConfigId, RequeueOnFailure: true}
	require.NoError(t, s.Claim(ctx, f, []*pb.MatchAuthorityTicketRecord{a, b}))
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_ticket_region").Scan(&count))
	require.Zero(t, count)
	work, err := s.AcquireWork(ctx, f.BattleId)
	require.NoError(t, err)
	require.NoError(t, s.Finish(ctx, work, false))
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_ticket_region").Scan(&count))
	require.Equal(t, 4, count)
	require.NoError(t, s.Cancel(ctx, a.PlayerId, a.Ticket))
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_ticket_region WHERE player_id=?", a.PlayerId).Scan(&count))
	require.Zero(t, count)
}

func TestAuthorityTiDBRegionRepairPreservesDatabaseFailures(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	n, err := s.Now(ctx)
	require.NoError(t, err)
	fixtureRegionTicket(t, s, 1, n-1000, n, 20, 100)
	s.roleGuard = func(context.Context, *sql.Tx, []*pb.MatchAuthorityTicketRecord) error { return sql.ErrConnDone }
	_, _, err = s.RepairCandidateRegions(ctx, 0, 0)
	require.ErrorIs(t, err, sql.ErrConnDone)
}
