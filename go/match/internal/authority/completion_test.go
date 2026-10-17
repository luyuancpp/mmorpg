package authority

import (
	"context"
	"crypto/sha256"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	battlepb "proto/battle"
	kafkapb "proto/contracts/kafka"
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

func TestCompletionValidationRejectsIncompleteOrInconsistentSources(t *testing.T) {
	cases := []struct {
		name string
		edit func(*kafkapb.BattleResultEvent)
	}{
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
