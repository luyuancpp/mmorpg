package authority

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	battlepb "proto/battle"
	pb "proto/match"
)

type nativeSpectatorFixture struct {
	store     *Store
	actor     context.Context
	formation *pb.MatchFormationRecord
	add       *battlepb.AddObserverRequest
}

// 真实TiDB隔离库和原生account_role P围栏；房间4也沿Claim/Finish提交，不用目录fake。
func nativeSpectator(t *testing.T) *nativeSpectatorFixture {
	t.Helper()
	s := nativeRoleStore(t)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	t.Cleanup(cancel)
	_, a, _ := nativeRoleJoin(t, s, ctx, 20)
	_, b, _ := nativeRoleJoin(t, s, ctx, 21)
	f := &pb.MatchFormationRecord{BattleId: 700, OperationId: uuid.NewString(), PhysicalRegion: 1, TargetNodeId: 1, TargetInstanceId: uuid.NewString(), TargetEndpoint: "127.0.0.1:1", Mode: 3, ConfigId: 1}
	require.NoError(t, s.Claim(ctx, f, []*pb.MatchAuthorityTicketRecord{a, b}))
	work, err := s.AcquireWork(ctx, f.BattleId)
	require.NoError(t, err)
	require.NoError(t, s.Finish(ctx, work, true))
	f, err = s.Formation(ctx, f.BattleId)
	require.NoError(t, err)
	_, err = s.DB.ExecContext(ctx, "INSERT INTO account_role VALUES(11,'spectator-original',7,0)")
	require.NoError(t, err)
	actor := nativeRoleActor(t, ctx, 11, "spectator-original", 7)
	return &nativeSpectatorFixture{store: s, actor: actor, formation: f, add: &battlepb.AddObserverRequest{ObserverName: "公开角色名", Routing: &battlepb.BattleRouting{SessionId: 1, GateNodeId: 2, GateInstanceId: uuid.NewString(), ZoneId: 1}}}
}
func (f *nativeSpectatorFixture) request() *pb.WatchBattleRequest {
	return &pb.WatchBattleRequest{RequestId: uuid.NewString()}
}
func (f *nativeSpectatorFixture) begin(t *testing.T, request *pb.WatchBattleRequest) *pb.MatchSpectatorRecord {
	t.Helper()
	_, err := f.store.SpectatorIntent(f.actor, 11, request)
	require.NoError(t, err)
	original, err := f.store.BeginSpectator(f.actor, 11, request, f.formation, f.add)
	require.NoError(t, err)
	return original
}

func TestSpectatorTiDBHeadPrecedesSelectionAndOldUUIDNeverRetakesHead(t *testing.T) {
	f := nativeSpectator(t)
	a, b := f.request(), f.request()
	first, err := f.store.SpectatorIntent(f.actor, 11, a)
	require.NoError(t, err)
	require.Equal(t, uint32(1), first.State)
	require.Empty(t, first.Original)
	_, err = f.store.SpectatorIntent(f.actor, 11, b)
	require.NoError(t, err)
	_, err = f.store.BeginSpectator(f.actor, 11, a, f.formation, f.add)
	require.ErrorIs(t, err, ErrSpectatorBusy)
	original, err := f.store.BeginSpectator(f.actor, 11, b, f.formation, f.add)
	require.NoError(t, err)
	// ACK未知后重放Begin不能重选随机场或刷新准入截止。
	other := proto.Clone(f.formation).(*pb.MatchFormationRecord)
	other.BattleId++
	replay, err := f.store.BeginSpectator(f.actor, 11, b, other, f.add)
	require.NoError(t, err)
	require.True(t, proto.Equal(original, replay))
	old, err := f.store.SpectatorIntent(f.actor, 11, a)
	require.NoError(t, err)
	require.Equal(t, uint32(5), old.State)
	require.Empty(t, old.Original)
	current, err := f.store.SpectatorForActor(f.actor, 11, b.RequestId)
	require.NoError(t, err)
	require.Equal(t, b.RequestId, current.OperationId)
	changed := proto.Clone(b).(*pb.WatchBattleRequest)
	changed.BattleId = f.formation.BattleId
	_, err = f.store.SpectatorIntent(f.actor, 11, changed)
	require.ErrorIs(t, err, ErrOriginalPlayer)
}

func TestSpectatorTiDBLateFrontendCannotCloseNewHeadButBackgroundCanFinishOldOp(t *testing.T) {
	f := nativeSpectator(t)
	a, b, c := f.request(), f.request(), f.request()
	original := f.begin(t, a)
	ok, err := f.store.AdvanceSpectatorForHead(f.actor, original, 2, a.RequestId)
	require.NoError(t, err)
	require.True(t, ok)
	_, err = f.store.SpectatorIntent(f.actor, 11, b)
	require.NoError(t, err)
	ok, err = f.store.AdvanceSpectatorForHead(f.actor, original, 3, b.RequestId)
	require.NoError(t, err)
	require.True(t, ok)
	_, err = f.store.SpectatorIntent(f.actor, 11, c)
	require.NoError(t, err)
	// B的Remove回包迟到；不能在C的head下提交前台闭环。
	ok, err = f.store.AdvanceSpectatorForHead(f.actor, original, 5, b.RequestId)
	require.ErrorIs(t, err, ErrSpectatorBusy)
	require.False(t, ok)
	recovering, superseded, err := f.store.SpectatorRecoveryCurrent(context.Background(), original)
	require.NoError(t, err)
	require.True(t, superseded)
	require.Equal(t, uint32(3), recovering.State)
	ok, err = f.store.AdvanceSpectator(context.Background(), original, 2)
	require.NoError(t, err)
	require.False(t, ok, "迟到Add不能复活取消中的op")
	ok, err = f.store.AdvanceSpectator(context.Background(), original, 5)
	require.NoError(t, err)
	require.True(t, ok)
	latest, err := f.store.BeginSpectator(f.actor, 11, c, f.formation, f.add)
	require.NoError(t, err)
	for _, state := range []uint32{2, 3, 5} {
		ok, err = f.store.AdvanceSpectator(context.Background(), original, state)
		require.ErrorIs(t, err, ErrSpectatorBusy)
		require.False(t, ok)
	}
	current, err := f.store.SpectatorForActor(f.actor, 11, c.RequestId)
	require.NoError(t, err)
	require.True(t, proto.Equal(latest, current))
}

func TestSpectatorTiDBOriginalOwnerTransferAndFreezeRejectWithoutChangingIntents(t *testing.T) {
	for _, freeze := range []bool{false, true} {
		t.Run(fmt.Sprint(freeze), func(t *testing.T) {
			f := nativeSpectator(t)
			request := f.request()
			original := f.begin(t, request)
			if freeze {
				_, err := f.store.DB.ExecContext(f.actor, "UPDATE account_role SET trade_listing_id=99 WHERE player_id=11")
				require.NoError(t, err)
			} else {
				_, err := f.store.DB.ExecContext(f.actor, "UPDATE account_role SET account='new-owner',owner_revision=8 WHERE player_id=11")
				require.NoError(t, err)
			}
			_, err := f.store.SpectatorIntent(f.actor, 11, request)
			require.ErrorIs(t, err, ErrOriginalPlayer)
			_, err = f.store.BeginSpectator(f.actor, 11, request, f.formation, f.add)
			require.ErrorIs(t, err, ErrOriginalPlayer)
			ok, err := f.store.AdvanceSpectator(context.Background(), original, 2)
			require.ErrorIs(t, err, ErrOriginalPlayer)
			require.False(t, ok)
			_, err = f.store.AuthorizeBattleTicket(f.actor, 11, f.formation.BattleId)
			require.ErrorIs(t, err, ErrOriginalPlayer)
			if !freeze {
				newActor := nativeRoleActor(t, f.actor, 11, "new-owner", 8)
				_, err = f.store.SpectatorIntent(newActor, 11, request)
				require.ErrorIs(t, err, ErrOriginalPlayer)
			}
			var state uint32
			var head string
			require.NoError(t, f.store.DB.QueryRowContext(f.actor, "SELECT state FROM match_spectator WHERE player_id=11").Scan(&state))
			require.Equal(t, uint32(1), state)
			require.NoError(t, f.store.DB.QueryRowContext(f.actor, "SELECT operation_id FROM match_spectator_head WHERE player_id=11").Scan(&head))
			require.Equal(t, request.RequestId, head)
		})
	}
}

func TestSpectatorTiDBActiveTicketAndSpectatorExcludeEachOtherAndUnknownNeverMeansFree(t *testing.T) {
	f := nativeSpectator(t)
	stamp, err := f.store.Now(f.actor)
	require.NoError(t, err)
	profile := &pb.MatchNetworkProfile{PlanId: "spectator-plan", SessionBinding: "bound", Receipts: []*pb.NetworkProbeReceipt{{Grant: &pb.NetworkProbeGrant{PlayerId: 11, PhysicalRegion: 1, ExpiresAtMs: stamp + 120000}, RttMs: 20, SampleCount: 3, MeasuredAtMs: stamp, ExpiresAtMs: stamp + 120000}}}
	require.NoError(t, f.store.SavePlan(f.actor, 11, profile, stamp+120000))
	request := f.request()
	_, err = f.store.SpectatorIntent(f.actor, 11, request)
	require.NoError(t, err)
	ticket, created, err := f.store.Join(f.actor, &pb.MatchAuthorityTicketRecord{PlayerId: 11, Ticket: uuid.NewString(), RequestId: uuid.NewString(), Mode: 3, ConfigId: 1, TeamSize: 2}, profile.PlanId, profile.SessionBinding)
	require.NoError(t, err)
	require.True(t, created)
	for _, state := range []uint32{1, 0, 99} {
		_, err = f.store.DB.ExecContext(f.actor, "UPDATE match_authority_ticket SET state=? WHERE player_id=11", state)
		require.NoError(t, err)
		_, err = f.store.BeginSpectator(f.actor, 11, request, f.formation, f.add)
		require.ErrorIs(t, err, ErrSpectatorBusy)
	}
	_, err = f.store.DB.ExecContext(f.actor, "UPDATE match_authority_ticket SET state=1 WHERE player_id=11")
	require.NoError(t, err)
	require.NoError(t, f.store.CancelIntent(f.actor, 11, ticket.Ticket, ticket.RequestId))
	_, err = f.store.BeginSpectator(f.actor, 11, request, f.formation, f.add)
	require.NoError(t, err)
	for _, state := range []uint32{1, 0, 99} {
		_, err = f.store.DB.ExecContext(f.actor, "UPDATE match_spectator SET state=? WHERE player_id=11", state)
		require.NoError(t, err)
		_, _, err = f.store.Join(f.actor, &pb.MatchAuthorityTicketRecord{PlayerId: 11, Ticket: uuid.NewString(), RequestId: uuid.NewString(), Mode: 3, ConfigId: 1, TeamSize: 2}, profile.PlanId, profile.SessionBinding)
		require.ErrorIs(t, err, ErrSpectatorBusy)
	}
}

func TestSpectatorTiDBTicketRefreshRequiresActiveOriginalHeadAndHistory(t *testing.T) {
	f := nativeSpectator(t)
	a := f.request()
	original := f.begin(t, a)
	_, err := f.store.AuthorizeBattleTicket(f.actor, 11, f.formation.BattleId)
	require.ErrorIs(t, err, ErrOriginalPlayer)
	ok, err := f.store.AdvanceSpectatorForHead(f.actor, original, 2, a.RequestId)
	require.NoError(t, err)
	require.True(t, ok)
	op, err := f.store.AuthorizeBattleTicket(f.actor, 11, f.formation.BattleId)
	require.NoError(t, err)
	require.Equal(t, a.RequestId, op)
	_, err = f.store.AuthorizeBattleTicket(f.actor, 11, f.formation.BattleId+1)
	require.ErrorIs(t, err, ErrOriginalPlayer)
	_, err = f.store.SpectatorIntent(f.actor, 11, f.request())
	require.NoError(t, err)
	_, err = f.store.AuthorizeBattleTicket(f.actor, 11, f.formation.BattleId)
	require.ErrorIs(t, err, ErrSpectatorBusy)
}

func TestSpectatorTiDBClosedOriginalAllowsNewOwnerNewIntentWithoutReauthorizingOldUUID(t *testing.T) {
	f := nativeSpectator(t)
	oldRequest := f.request()
	original := f.begin(t, oldRequest)
	ok, err := f.store.AdvanceSpectator(context.Background(), original, 5)
	require.NoError(t, err)
	require.True(t, ok)
	_, err = f.store.DB.ExecContext(f.actor, "UPDATE account_role SET account='new-owner',owner_revision=8 WHERE player_id=11")
	require.NoError(t, err)
	actor := nativeRoleActor(t, f.actor, 11, "new-owner", 8)
	_, err = f.store.SpectatorIntent(actor, 11, oldRequest)
	require.ErrorIs(t, err, ErrOriginalPlayer)
	request := f.request()
	_, err = f.store.SpectatorIntent(actor, 11, request)
	require.NoError(t, err)
	current, err := f.store.SpectatorForActor(actor, 11, request.RequestId)
	require.NoError(t, err)
	require.Nil(t, current)
	current, err = f.store.BeginSpectator(actor, 11, request, f.formation, f.add)
	require.NoError(t, err)
	require.Equal(t, "new-owner", current.OwnerAccount)
	require.EqualValues(t, 8, current.OwnerRevision)
}

func TestSpectatorTiDBConcurrentUnpreparedRequestsOnlyCurrentHeadCanPrepare(t *testing.T) {
	f := nativeSpectator(t)
	requests := []*pb.WatchBattleRequest{f.request(), f.request()}
	var workers sync.WaitGroup
	failures := make(chan error, 2)
	start := make(chan struct{})
	for _, request := range requests {
		workers.Add(1)
		go func(request *pb.WatchBattleRequest) {
			defer workers.Done()
			<-start
			_, err := f.store.SpectatorIntent(f.actor, 11, request)
			failures <- err
		}(request)
	}
	close(start)
	workers.Wait()
	close(failures)
	for err := range failures {
		require.NoError(t, err)
	}
	var head string
	require.NoError(t, f.store.DB.QueryRowContext(f.actor, "SELECT operation_id FROM match_spectator_head WHERE player_id=11").Scan(&head))
	for _, request := range requests {
		_, err := f.store.BeginSpectator(f.actor, 11, request, f.formation, f.add)
		if request.RequestId == head {
			require.NoError(t, err)
		} else {
			require.ErrorIs(t, err, ErrSpectatorBusy)
		}
	}
}

func TestSpectatorTiDBFixedRecoveryWatermarkAndBoundedStatePages(t *testing.T) {
	f := nativeSpectator(t)
	var active []uint64
	for player := uint64(30); player < 48; player++ {
		account := fmt.Sprintf("observer-%d", player)
		_, err := f.store.DB.ExecContext(f.actor, "INSERT INTO account_role VALUES(?,?,7,0)", player, account)
		require.NoError(t, err)
		actor := nativeRoleActor(t, f.actor, player, account, 7)
		request := f.request()
		_, err = f.store.SpectatorIntent(actor, player, request)
		require.NoError(t, err)
		original, err := f.store.BeginSpectator(actor, player, request, f.formation, f.add)
		require.NoError(t, err)
		state := uint32(4)
		if player%2 == 0 {
			state = 5
		} else {
			active = append(active, player)
		}
		ok, err := f.store.AdvanceSpectator(context.Background(), original, state)
		require.NoError(t, err)
		require.True(t, ok)
	}
	through, err := f.store.SpectatorRecoveryHighWater(f.actor)
	require.NoError(t, err)
	require.Equal(t, uint64(47), through)
	_, err = f.store.DB.ExecContext(f.actor, "INSERT INTO account_role VALUES(90,'observer-later',7,0)")
	require.NoError(t, err)
	laterActor := nativeRoleActor(t, f.actor, 90, "observer-later", 7)
	laterRequest := f.request()
	_, err = f.store.SpectatorIntent(laterActor, 90, laterRequest)
	require.NoError(t, err)
	_, err = f.store.BeginSpectator(laterActor, 90, laterRequest, f.formation, f.add)
	require.NoError(t, err)
	var found []uint64
	var cursor uint64
	for round := 0; round < 3; round++ {
		rows, err := f.store.SpectatorsThrough(f.actor, cursor, through)
		require.NoError(t, err)
		require.LessOrEqual(t, len(rows), 8)
		for _, row := range rows {
			found = append(found, row.PlayerId)
			cursor = row.PlayerId
			require.Equal(t, uint32(4), row.State)
		}
		if len(rows) == 0 {
			break
		}
	}
	require.Equal(t, active, found)
}

func TestSpectatorTiDBBusinessExitCaptureClosesUnpreparedAndRollsBackWithOriginalIntent(t *testing.T) {
	f := nativeSpectator(t)
	unprepared := f.request()
	_, err := f.store.SpectatorIntent(f.actor, 11, unprepared)
	require.NoError(t, err)
	tx, err := f.store.DB.BeginTx(f.actor, nil)
	require.NoError(t, err)
	require.NoError(t, f.store.lockActor(f.actor, tx, 11))
	exits, err := f.store.CaptureSpectatorExitsTx(f.actor, tx, []*pb.MatchAuthorityTicketRecord{originalPlayer(f.actor, 11)})
	require.NoError(t, err)
	require.Empty(t, exits)
	require.NoError(t, tx.Commit())
	intent, err := f.store.SpectatorIntent(f.actor, 11, unprepared)
	require.NoError(t, err)
	require.Equal(t, uint32(5), intent.State)
	_, err = f.store.BeginSpectator(f.actor, 11, unprepared, f.formation, f.add)
	require.ErrorIs(t, err, ErrSpectatorBusy)
	request := f.request()
	original := f.begin(t, request)
	tx, err = f.store.DB.BeginTx(f.actor, nil)
	require.NoError(t, err)
	require.NoError(t, f.store.lockActor(f.actor, tx, 11))
	exits, err = f.store.CaptureSpectatorExitsTx(f.actor, tx, []*pb.MatchAuthorityTicketRecord{originalPlayer(f.actor, 11)})
	require.NoError(t, err)
	require.Len(t, exits, 1)
	require.Equal(t, uint32(3), exits[0].State)
	require.NoError(t, tx.Rollback(), "业务原意图拒绝时，不能提前提交观战清退")
	current, err := f.store.SpectatorForActor(f.actor, 11, request.RequestId)
	require.NoError(t, err)
	require.True(t, proto.Equal(original, current))
	closed, err := f.store.SpectatorExitClosed(f.actor, original)
	require.NoError(t, err)
	require.False(t, closed)
}
