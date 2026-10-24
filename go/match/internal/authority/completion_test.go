package authority

import (
	"context"
	"crypto/sha256"
	"encoding/binary"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	battlepb "proto/battle"
	kafkapb "proto/contracts/kafka"
	pb "proto/match"
	inboxpb "proto/player_inbox"
	"shared/serviceauth"
)

func completeResult() *kafkapb.BattleResultEvent {
	return &kafkapb.BattleResultEvent{
		BattleId: 901, Outcome: battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN,
		SettlementFormatVersion: 1,
		Teams:                   []*kafkapb.BattleResultTeam{{TeamIndex: 0, PlayerIds: []uint64{11}}, {TeamIndex: 1, PlayerIds: []uint64{22}}},
		Settlements: []*battlepb.BattleSettlementData{
			{BattleId: 901, PlayerId: 11, Outcome: battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, PlayerTeamIndex: 0, GoldGain: 5, ExpGain: 8, Health: 100,
				ItemsConsumed: []*battlepb.BattleItemEntry{{ItemTableId: 31, Count: 1}}, ItemsGained: []*battlepb.BattleItemEntry{{ItemTableId: 32, Count: 2}},
				Pets: []*battlepb.BattlePetSettlementData{{PetId: 41, Health: 50, Mana: 20}}, DefeatedMonsters: []*battlepb.BattleMonsterDefeat{{MonsterConfigId: 51, Count: 1}}},
			{BattleId: 901, PlayerId: 22, Outcome: battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, PlayerTeamIndex: 1, Health: 25},
		},
	}
}

func resultBytes(t *testing.T, e *kafkapb.BattleResultEvent) []byte {
	t.Helper()
	raw, err := proto.Marshal(e)
	require.NoError(t, err)
	return raw
}

const terminalFixtureSecret = "fixture-only-battle-inbox-outcome-secret-000000000"

func terminalResultFixture(t *testing.T, s *Store, expired bool) (*pb.MatchFormationRecord, *pb.CommitBattleOutcomeRequest) {
	t.Helper()
	ctx := context.Background()
	a, b := testJoin(t, s, 11), testJoin(t, s, 22)
	stamp, err := s.Now(ctx)
	require.NoError(t, err)
	f := &pb.MatchFormationRecord{BattleId: 901, OperationId: "terminal-original-op", State: 1, TargetNodeId: 7, TargetInstanceId: "10000000-0000-4000-8000-000000000007", TargetEndpoint: "127.0.0.1:1", PhysicalRegion: 1, Mode: 3, ConfigId: 1, CreatedMs: stamp - 2000, DeadlineMs: stamp + 300000, RetryAtMs: stamp - 2000, Members: []*pb.MatchFormationMember{{PlayerId: 11, Ticket: a.Ticket}, {PlayerId: 22, Ticket: b.Ticket}}}
	if expired {
		f.DeadlineMs = stamp - 1000
	}
	members, err := proto.Marshal(&pb.MatchFormationRecord{Members: f.Members})
	require.NoError(t, err)
	_, err = s.DB.ExecContext(ctx, "INSERT INTO match_formation("+formationColumns+") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", f.BattleId, f.OperationId, f.State, f.PhysicalRegion, f.TargetNodeId, f.TargetInstanceId, f.TargetEndpoint, f.Mode, f.ConfigId, members, []byte{}, f.CreatedMs, f.DeadlineMs, f.RetryAtMs, []byte{}, false, []byte{}, uint32(0), uint32(0), "", uint32(0), false)
	require.NoError(t, err)
	for _, r := range []*pb.MatchAuthorityTicketRecord{a, b} {
		_, err = s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET owner_account=?,owner_revision=1,state=2,formation_id=901 WHERE player_id=? AND ticket=?", "original-owner", r.PlayerId, r.Ticket)
		require.NoError(t, err)
	}
	work, err := s.AcquireWork(ctx, f.BattleId)
	require.NoError(t, err)
	request := &battlepb.CreateBattleRequest{BattleId: f.BattleId, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId, BattleConfigId: f.ConfigId, MatchMode: f.Mode, OutcomeProtocolVersion: 1, CreatedAtMs: f.CreatedMs, DeadlineMs: f.DeadlineMs, Players: []*battlepb.BattlePlayerSnapshot{{PlayerId: 11, TeamIndex: 0}, {PlayerId: 22, TeamIndex: 1}}}
	raw, err := proto.MarshalOptions{Deterministic: true}.Marshal(request)
	require.NoError(t, err)
	_, err = s.SaveSupportedIntent(ctx, work, raw, &battlepb.QueryBattleCreationResponse{State: battlepb.BattleCreationState_BATTLE_CREATION_ABSENT, TargetInstanceId: f.TargetInstanceId, SupportedOutcomeProtocolVersion: 1})
	require.NoError(t, err)
	f, err = s.Formation(ctx, f.BattleId)
	require.NoError(t, err)
	e := completeResult()
	e.MatchMode = f.Mode
	e.BattleConfigId = f.ConfigId
	e.FinishedAtMs = stamp
	return f, &pb.CommitBattleOutcomeRequest{BattleId: f.BattleId, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId, RawResultPayload: resultBytes(t, e)}
}

func TestTerminalResultRejectsLegacyAndNestedUnknownBeforeSQL(t *testing.T) {
	e := completeResult()
	e.MatchMode = 3
	e.BattleConfigId = 1
	e.FinishedAtMs = 20
	create := &battlepb.CreateBattleRequest{BattleId: 901, PlacementOperationId: "op", TargetInstanceId: "uuid", OutcomeProtocolVersion: 1, MatchMode: 3, BattleConfigId: 1, CreatedAtMs: 1, DeadlineMs: 30, Players: []*battlepb.BattlePlayerSnapshot{{PlayerId: 11, TeamIndex: 0}, {PlayerId: 22, TeamIndex: 1}}}
	b, err := proto.Marshal(create)
	require.NoError(t, err)
	f := &pb.MatchFormationRecord{BattleId: 901, OperationId: "op", TargetInstanceId: "uuid", Mode: 3, ConfigId: 1, CreatedMs: 1, DeadlineMs: 30, OutcomeProtocolVersion: 1, PreflightSupportedVersion: 1, PreflightInstanceId: "uuid", CreateRequest: b, Members: []*pb.MatchFormationMember{{PlayerId: 11}, {PlayerId: 22}}}
	require.NoError(t, validateTerminalResult(f, e, resultBytes(t, e)))
	missing := proto.Clone(e).(*kafkapb.BattleResultEvent)
	missing.Teams, missing.Settlements = missing.Teams[:1], missing.Settlements[:1]
	require.NoError(t, validateCompletion(missing, resultBytes(t, missing)), "旧归档允许保留内部自洽的完整原件")
	require.Error(t, validateTerminalResult(f, missing, resultBytes(t, missing)), "不能以结果自身的单人全集替代原Create的两人全集")
	f.PreflightSupportedVersion = 0
	require.Error(t, validateTerminalResult(f, e, resultBytes(t, e)))
	f.PreflightSupportedVersion = 1
	f.AdoptedOutcomeObserved = true
	require.Error(t, validateTerminalResult(f, e, resultBytes(t, e)), "实际观察adopted0不能借preflight1追认")
	f.AdoptedOutcomeProtocolVersion = 1
	require.NoError(t, validateTerminalResult(f, e, resultBytes(t, e)))
	f.AdoptedOutcomeObserved = false
	f.AdoptedOutcomeProtocolVersion = 0
	e.Settlements[0].Pets[0].ProtoReflect().SetUnknown(protowire.AppendVarint(protowire.AppendTag(nil, 199, protowire.VarintType), 1))
	require.Error(t, validateTerminalResult(f, e, resultBytes(t, e)), "嵌套未知语义不得提交为可执行COMPLETE")
	// 旧Kafka仅保存来源，因此同样未知字段仍可完整归档。
	require.NoError(t, validateCompletion(e, resultBytes(t, e)))
	e = completeResult()
	e.MatchMode = 3
	e.BattleConfigId = 1
	e.FinishedAtMs = 20
	for i := 0; i < 4000; i++ {
		e.Settlements[0].ItemsGained = append(e.Settlements[0].ItemsGained, &battlepb.BattleItemEntry{ItemTableId: uint32(i + 1), Count: 1})
	}
	// 全部字段均已知，证明单人16KiB限制本身，而非借未知字段拒绝。
	require.ErrorContains(t, validateTerminalResult(f, e, resultBytes(t, e)), "16KiB")
}

func TestTerminalTiDBCompleteKeepsBothOutboxesAndReplayBytes(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	_, r := terminalResultFixture(t, s, false)
	d, err := s.CommitBattleOutcome(ctx, r)
	require.NoError(t, err)
	require.Equal(t, pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_COMPLETE, d)
	d, err = s.CommitBattleOutcome(ctx, r)
	require.NoError(t, err)
	require.Equal(t, pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_COMPLETE, d)
	changed := proto.Clone(r).(*pb.CommitBattleOutcomeRequest)
	changed.RawResultPayload = append(changed.RawResultPayload, 0xF8, 0x07, 0x01)
	_, err = s.CommitBattleOutcome(ctx, changed)
	require.Error(t, err, "同语义另字节不能重算替换")
	var pending int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_completion WHERE decision=1 AND result_published=0").Scan(&pending))
	require.Equal(t, 1, pending)
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_settlement_source WHERE dispatch_state=1 AND owner_account='original-owner' AND owner_revision=1 AND issued_at_ms>0").Scan(&pending))
	require.Equal(t, 2, pending)
}

func TestTerminalTiDBObservedZeroCannotBePromotedByLaterReply(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	f, r := terminalResultFixture(t, s, true)
	require.NoError(t, s.RecordBattleAdopted(ctx, f, f.TargetInstanceId, 0))
	observed, err := s.Formation(ctx, f.BattleId)
	require.NoError(t, err)
	require.True(t, observed.AdoptedOutcomeObserved)
	require.Zero(t, observed.AdoptedOutcomeProtocolVersion)
	require.Error(t, s.RecordBattleAdopted(ctx, f, f.TargetInstanceId, 1), "后一次1不能覆盖先前真实0")
	_, err = s.CommitBattleOutcome(ctx, r)
	require.Error(t, err, "明确0不能提交COMPLETE")
	tx, locked, _, err := s.terminalFormation(ctx, f.BattleId)
	require.NoError(t, err)
	defer tx.Rollback()
	d, err := s.abortTerminalTx(ctx, tx, locked)
	require.NoError(t, err)
	require.Equal(t, pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_UNKNOWN, d, "能力1不能追认明确采用0为安全ABORT")
}

func TestTerminalTiDBCancelledFormationDistinguishesUnissuedAndPersistedCreate(t *testing.T) {
	for _, kind := range []string{"发房前", "旧创建原件", "明确旧采用", "新协议原件"} {
		t.Run(kind, func(t *testing.T) {
			s := testStore(t)
			ctx := context.Background()
			a, b := testJoin(t, s, 11), testJoin(t, s, 22)
			stamp, err := s.Now(ctx)
			require.NoError(t, err)
			f := &pb.MatchFormationRecord{BattleId: 901, OperationId: "original-cancel-op", PhysicalRegion: 1, TargetNodeId: 3, TargetInstanceId: "original-instance", TargetEndpoint: "endpoint", Mode: a.Mode, ConfigId: a.ConfigId}
			key := &pb.MatchDirectIntentRecord{Kind: 2, OwnerId: 44, RequestLow: 51}
			body := []byte("same-entire-original-start")
			_, err = s.ReserveDirect(ctx, key, body, f, []*pb.MatchAuthorityTicketRecord{a, b})
			require.NoError(t, err)
			work, err := s.AcquireWork(ctx, f.BattleId)
			require.NoError(t, err)
			if kind != "发房前" {
				// 完整原意图先持久才可能发房。deadline只调整本隔离夹具，模拟已过期。
				_, err = s.DB.ExecContext(ctx, "UPDATE match_formation SET created_ms=?,deadline_ms=? WHERE battle_id=?", stamp-2000, stamp-1000, f.BattleId)
				require.NoError(t, err)
				request := &battlepb.CreateBattleRequest{BattleId: f.BattleId, PlacementOperationId: f.OperationId, TargetInstanceId: f.TargetInstanceId, BattleConfigId: f.ConfigId, MatchMode: f.Mode, CreatedAtMs: stamp - 2000, DeadlineMs: stamp - 1000, Players: []*battlepb.BattlePlayerSnapshot{{PlayerId: a.PlayerId}, {PlayerId: b.PlayerId}}}
				newProtocol := kind == "新协议原件" || kind == "明确旧采用"
				if newProtocol {
					request.OutcomeProtocolVersion = 1
				}
				raw, marshalErr := proto.Marshal(request)
				require.NoError(t, marshalErr)
				if newProtocol {
					_, err = s.SaveSupportedIntent(ctx, work, raw, &battlepb.QueryBattleCreationResponse{State: battlepb.BattleCreationState_BATTLE_CREATION_ABSENT, TargetInstanceId: f.TargetInstanceId, SupportedOutcomeProtocolVersion: 1})
				} else {
					_, err = s.SaveIntent(ctx, work, raw)
				}
				require.NoError(t, err)
				if kind == "明确旧采用" {
					require.NoError(t, s.RecordBattleAdopted(ctx, work, f.TargetInstanceId, 0))
				}
			}
			if kind == "发房前" {
				require.NoError(t, s.Finish(ctx, work, false))
			} else {
				require.ErrorContains(t, s.Finish(ctx, work, false), "唯一SQL终局", "新流程不能再生成缺终局的state5")
				// 仅此历史兼容夹具显式重放旧版已经写出的5；不经新入口制造该危险状态。
				_, err = s.DB.ExecContext(ctx, "UPDATE match_formation SET state=5 WHERE battle_id=?", f.BattleId)
				require.NoError(t, err)
			}
			before, err := s.Formation(ctx, f.BattleId)
			require.NoError(t, err)
			beforeDirect, err := s.DirectAdmission(ctx, key)
			require.NoError(t, err)
			var beforeTickets []*pb.MatchAuthorityTicketRecord
			for _, player := range []uint64{a.PlayerId, b.PlayerId} {
				ticket, readErr := s.Ticket(ctx, player)
				require.NoError(t, readErr)
				beforeTickets = append(beforeTickets, ticket)
			}
			receipt, err := s.CancelActivity(ctx, key, body)
			terminal, readErr := readTerminal(ctx, s.DB, f.BattleId)
			require.NoError(t, readErr)
			if kind == "旧创建原件" || kind == "明确旧采用" {
				require.Error(t, err, "旧state5不能冒SQL ABORT签发安全取消收据")
				require.Nil(t, receipt)
				require.Nil(t, terminal)
				after, readErr := s.Formation(ctx, f.BattleId)
				require.NoError(t, readErr)
				require.True(t, proto.Equal(before, after), "拒绝时保原持久成局不变")
				afterDirect, readErr := s.DirectAdmission(ctx, key)
				require.NoError(t, readErr)
				require.True(t, proto.Equal(beforeDirect, afterDirect), "拒绝时保原直接意图不变")
				for _, ticket := range beforeTickets {
					afterTicket, readErr := s.Ticket(ctx, ticket.PlayerId)
					require.NoError(t, readErr)
					require.True(t, proto.Equal(ticket, afterTicket), "拒绝时保全部原玩家票据不变")
				}
				return
			}
			require.NoError(t, err)
			require.EqualValues(t, 5, receipt.FormationState)
			if kind == "发房前" {
				require.Nil(t, terminal)
				require.Equal(t, pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_UNKNOWN, receipt.TerminalDecision)
			} else {
				require.NotNil(t, terminal)
				require.Equal(t, pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_ABORT, terminal.Decision)
				require.Equal(t, terminal.Decision, receipt.TerminalDecision)
			}
		})
	}
}

func TestTerminalTiDBPersonalConflictRollsBackEntireComplete(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	f, r := terminalResultFixture(t, s, false)
	_, err := s.DB.ExecContext(ctx, "INSERT INTO match_battle_settlement_source(battle_id,player_id,settlement_proto,payload_digest,archived_ms) VALUES(901,22,?,?,1)", []byte("original-conflict"), []byte("digest"))
	require.NoError(t, err)
	_, err = s.CommitBattleOutcome(ctx, r)
	require.Error(t, err)
	row, err := readTerminal(ctx, s.DB, f.BattleId)
	require.NoError(t, err)
	require.Nil(t, row, "不能只提交整局决定而少一人成员")
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_settlement_source WHERE battle_id=901 AND player_id=11").Scan(&count))
	require.Zero(t, count)
}

func TestTerminalTiDBAbortAndCompleteHaveOneWinner(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	f, r := terminalResultFixture(t, s, true)
	_, err := s.DB.ExecContext(ctx, "UPDATE match_formation SET retry_at_ms=0 WHERE battle_id=?", f.BattleId)
	require.NoError(t, err)
	f, err = s.Formation(ctx, f.BattleId)
	require.NoError(t, err)
	start := make(chan struct{})
	complete := make(chan error, 1)
	aborted := make(chan error, 1)
	go func() { <-start; _, err := s.CommitBattleOutcome(ctx, r); complete <- err }()
	go func() {
		<-start
		_, err := s.RecoverExpiredOutcome(ctx, f)
		aborted <- err
	}()
	close(start)
	completeErr, abortErr := <-complete, <-aborted
	if completeErr != nil && abortErr != nil {
		t.Fatalf("竞争至少一方应持久成功: %v / %v", completeErr, abortErr)
	}
	row, err := readTerminal(ctx, s.DB, f.BattleId)
	require.NoError(t, err)
	require.NotNil(t, row)
	require.Contains(t, []pb.BattleOutcomeDecision{pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_COMPLETE, pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_ABORT}, row.Decision)
	var people int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_settlement_source WHERE battle_id=?", f.BattleId).Scan(&people))
	if row.Decision == pb.BattleOutcomeDecision_BATTLE_OUTCOME_DECISION_ABORT {
		require.Zero(t, people)
		d, err := s.CommitBattleOutcome(ctx, r)
		require.NoError(t, err)
		require.Equal(t, row.Decision, d)
	} else {
		require.Equal(t, 2, people)
	}
}

type terminalInboxFixture struct {
	inboxpb.PlayerInboxClient
	state      inboxpb.PlayerInboxItemState
	failPlayer uint64
	seen       []uint64
}

func (c *terminalInboxFixture) response(method string, r *inboxpb.BattleSettlementInboxRequest) (*inboxpb.BattleSettlementInboxResponse, error) {
	c.seen = append(c.seen, r.PlayerId)
	if c.failPlayer == r.PlayerId {
		return nil, errors.New("fixture离线")
	}
	digest, err := serviceauth.Digest(r)
	if err != nil {
		return nil, err
	}
	var head [12]byte
	binary.BigEndian.PutUint32(head[:4], 4)
	binary.BigEndian.PutUint64(head[4:], r.IssuedAtMs)
	h := sha256.New()
	h.Write(head[:])
	h.Write(r.SettlementPayload)
	out := &inboxpb.BattleSettlementInboxResponse{PlayerId: r.PlayerId, BattleId: r.BattleId, Seq: r.PlayerId + 100, State: c.state, ContentSha256: h.Sum(nil), RequestDigest: digest}
	err = serviceauth.Sign(method+"/reply", out, "inbox", terminalFixtureSecret, uint64(time.Now().UnixMilli()))
	return out, err
}
func (c *terminalInboxFixture) EnqueueBattleSettlement(_ context.Context, r *inboxpb.BattleSettlementInboxRequest, _ ...grpc.CallOption) (*inboxpb.BattleSettlementInboxResponse, error) {
	return c.response(inboxpb.PlayerInbox_EnqueueBattleSettlement_FullMethodName, r)
}
func (c *terminalInboxFixture) LookupBattleSettlement(_ context.Context, r *inboxpb.BattleSettlementInboxRequest, _ ...grpc.CallOption) (*inboxpb.BattleSettlementInboxResponse, error) {
	return c.response(inboxpb.PlayerInbox_LookupBattleSettlement_FullMethodName, r)
}

func TestTerminalTiDBOfflineRecipientDoesNotBlockLaterAndAppliedOnly(t *testing.T) {
	s := testStore(t)
	ctx := context.Background()
	_, r := terminalResultFixture(t, s, false)
	_, err := s.CommitBattleOutcome(ctx, r)
	require.NoError(t, err)
	client := &terminalInboxFixture{state: inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING, failPlayer: 11}
	resolve := func(context.Context) (inboxpb.PlayerInboxClient, error) { return client, nil }
	var published []byte
	publish := func(_ context.Context, _ uint64, raw []byte) error {
		published = append([]byte(nil), raw...)
		return nil
	}
	require.Error(t, s.PublishBattleOutcomesOnce(ctx, resolve, terminalFixtureSecret, publish))
	require.Equal(t, r.RawResultPayload, published)
	require.Contains(t, client.seen, uint64(22), "离线首项不能挡后项")
	var state uint32
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT dispatch_state FROM match_battle_settlement_source WHERE battle_id=901 AND player_id=22").Scan(&state))
	require.Equal(t, uint32(1), state, "入箱但未APPLIED不销账")
	client.failPlayer = 0
	client.state = inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_APPLIED
	_, err = s.DB.ExecContext(ctx, "UPDATE match_battle_settlement_source SET next_retry_ms=0")
	require.NoError(t, err)
	require.NoError(t, s.PublishBattleOutcomesOnce(ctx, resolve, terminalFixtureSecret, publish))
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_settlement_source WHERE dispatch_state=2 AND inbox_state=2 AND inbox_seq>0").Scan(&count))
	require.Equal(t, 2, count)
}

func TestCompletionValidationRejectsIncompleteOrInconsistentSources(t *testing.T) {
	cases := []struct {
		name string
		edit func(*kafkapb.BattleResultEvent)
	}{
		{"空原字节", func(e *kafkapb.BattleResultEvent) { *e = kafkapb.BattleResultEvent{} }},
		{"未知版本", func(e *kafkapb.BattleResultEvent) { e.SettlementFormatVersion = 2 }},
		{"旧版携带新个人原件", func(e *kafkapb.BattleResultEvent) { e.SettlementFormatVersion = 0 }},
		{"无对局", func(e *kafkapb.BattleResultEvent) { e.BattleId = 0 }},
		{"未结束", func(e *kafkapb.BattleResultEvent) { e.Outcome = battlepb.EBattleOutcome_BATTLE_OUTCOME_ONGOING }},
		{"未知终局", func(e *kafkapb.BattleResultEvent) { e.Outcome = 99 }},
		{"无队伍", func(e *kafkapb.BattleResultEvent) { e.Teams = nil }},
		{"空队伍", func(e *kafkapb.BattleResultEvent) { e.Teams[0].PlayerIds = nil }},
		{"重复队伍", func(e *kafkapb.BattleResultEvent) { e.Teams[1].TeamIndex = 0 }},
		{"空成员", func(e *kafkapb.BattleResultEvent) { e.Teams[0].PlayerIds[0] = 0 }},
		{"重复成员", func(e *kafkapb.BattleResultEvent) { e.Teams[1].PlayerIds[0] = 11 }},
		{"缺少个人原件", func(e *kafkapb.BattleResultEvent) { e.Settlements = e.Settlements[:1] }},
		{"重复个人原件", func(e *kafkapb.BattleResultEvent) { e.Settlements[1].PlayerId = 11 }},
		{"空个人玩家", func(e *kafkapb.BattleResultEvent) { e.Settlements[1].PlayerId = 0 }},
		{"多出非成员", func(e *kafkapb.BattleResultEvent) { e.Settlements[1].PlayerId = 33 }},
		{"个人局号不符", func(e *kafkapb.BattleResultEvent) { e.Settlements[0].BattleId++ }},
		{"个人终局不符", func(e *kafkapb.BattleResultEvent) {
			e.Settlements[0].Outcome = battlepb.EBattleOutcome_BATTLE_OUTCOME_DRAW
		}},
		{"个人队伍不符", func(e *kafkapb.BattleResultEvent) { e.Settlements[0].PlayerTeamIndex = 1 }},
		{"个人回合数不符", func(e *kafkapb.BattleResultEvent) { e.Settlements[0].TotalRounds++ }},
	}
	for _, test := range cases {
		t.Run(test.name, func(t *testing.T) {
			e := completeResult()
			test.edit(e)
			require.Error(t, (&Store{}).ArchiveCompletion(context.Background(), e, resultBytes(t, e), "results", 0, 3))
		})
	}
}

func TestCompletionValidationLegacyAndPVE(t *testing.T) {
	legacy := &kafkapb.BattleResultEvent{BattleId: 1}
	require.NoError(t, (*Store)(nil).ArchiveCompletion(context.Background(), legacy, resultBytes(t, legacy), "", 0, 0), "旧结果不冒造完整来源，也不依赖新数据库")
	e := completeResult()
	e.Teams = e.Teams[:1]
	e.Settlements = e.Settlements[:1]
	require.NoError(t, validateCompletion(e, resultBytes(t, e)), "PVE只有玩家队伍，怪物不能被当作缺失玩家")
	require.Error(t, validateCompletion(nil, nil))
	require.Error(t, validateCompletion(e, make([]byte, maxCompletionBytes+1)))
	require.Error(t, validateCompletion(e, []byte{255}))
	other := proto.Clone(e).(*kafkapb.BattleResultEvent)
	other.FinishedAtMs++
	require.Error(t, validateCompletion(e, resultBytes(t, other)))
	for _, metadata := range []struct {
		topic string
		part  int
		off   int64
	}{{"", 0, 1}, {"results", -1, 1}, {"results", 0, -1}, {"results", 1 << 31, 1}} {
		require.Error(t, (&Store{}).ArchiveCompletion(context.Background(), e, resultBytes(t, e), metadata.topic, metadata.part, metadata.off))
	}
}

func TestCompletionTiDBPreservesOriginalBytesAndIdempotentReplay(t *testing.T) {
	s := testStore(t)
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	e := completeResult()
	// 重复的标量字段仍是合法protobuf；必须原样保全，不能用重序列化抹掉原始字节差别。
	raw := protowire.AppendTag(resultBytes(t, e), 1, protowire.VarintType)
	raw = protowire.AppendVarint(raw, e.BattleId)
	require.NoError(t, s.ArchiveCompletion(ctx, e, raw, "match-results-g1", 2, 15))
	require.NoError(t, s.ArchiveCompletion(ctx, e, raw, "match-results-g1", 1, 99), "提交响应丢失后按原消息重放，保留首次来源位点")
	var archived, digest []byte
	var part int
	var offset int64
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT raw_payload,payload_digest,source_partition,source_offset FROM match_battle_completion WHERE battle_id=?", e.BattleId).Scan(&archived, &digest, &part, &offset))
	require.Equal(t, raw, archived)
	expected := sha256.Sum256(raw)
	require.Equal(t, expected[:], digest)
	require.Equal(t, 2, part)
	require.Equal(t, int64(15), offset)
	require.Error(t, s.ArchiveCompletion(ctx, e, resultBytes(t, e), "match-results-g1", 2, 16), "语义相同但原字节不同，也必须拒绝")
	for _, settlement := range e.Settlements {
		var personal, personalDigest []byte
		require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT settlement_proto,payload_digest FROM match_battle_settlement_source WHERE battle_id=? AND player_id=?", e.BattleId, settlement.PlayerId).Scan(&personal, &personalDigest))
		decoded := &battlepb.BattleSettlementData{}
		require.NoError(t, proto.Unmarshal(personal, decoded))
		require.True(t, proto.Equal(settlement, decoded), "经验/金币/掉落/消耗/宠物/任务事实必须全部保全")
		want := sha256.Sum256(personal)
		require.Equal(t, want[:], personalDigest)
	}
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_completion").Scan(&count))
	require.Equal(t, 1, count)
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_settlement_source").Scan(&count))
	require.Equal(t, 2, count)
}

func TestCompletionTiDBPersonalFailureRollsBackWholeSource(t *testing.T) {
	s := testStore(t)
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	e := completeResult()
	_, err := s.DB.ExecContext(ctx, "INSERT INTO match_battle_settlement_source(battle_id,player_id,settlement_proto,payload_digest,archived_ms) VALUES(?,?,?,?,?)", e.BattleId, 22, []byte("preexisting"), []byte("conflict"), 1)
	require.NoError(t, err)
	require.Error(t, s.ArchiveCompletion(ctx, e, resultBytes(t, e), "match-results-g1", 0, 1))
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_completion WHERE battle_id=?", e.BattleId).Scan(&count))
	require.Zero(t, count, "后续个人原件失败，整局来源必须回滚")
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_settlement_source WHERE battle_id=? AND player_id=11", e.BattleId).Scan(&count))
	require.Zero(t, count, "不能留下只保存一部分玩家的来源")
}

func TestCompletionTiDBConcurrentSameBattleArchivesOnce(t *testing.T) {
	s := testStore(t)
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	e := completeResult()
	raw := resultBytes(t, e)
	start := make(chan struct{})
	results := make(chan error, 8)
	var group sync.WaitGroup
	for i := 0; i < 8; i++ {
		group.Add(1)
		go func() {
			defer group.Done()
			<-start
			results <- s.ArchiveCompletion(ctx, e, raw, "match-results-g1", 0, 1)
		}()
	}
	close(start)
	group.Wait()
	close(results)
	for err := range results {
		require.NoError(t, err)
	}
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_completion").Scan(&count))
	require.Equal(t, 1, count)
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_battle_settlement_source").Scan(&count))
	require.Equal(t, 2, count)
	changed := proto.Clone(e).(*kafkapb.BattleResultEvent)
	changed.Settlements[0].GoldGain++
	require.Error(t, s.ArchiveCompletion(ctx, changed, resultBytes(t, changed), "match-results-g1", 0, 2))
}
