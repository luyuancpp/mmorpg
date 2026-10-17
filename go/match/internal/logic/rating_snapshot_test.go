package logic

// 排行榜评分快照(docs/design/leaderboard-system.md §2.4、§8.1 的 T4b / T10 / T13)的 miniredis 测试。
// 发布经包级接缝 ratingSnapshotPublishFn,这里一律替换成替身、不连 Kafka;时钟经 ratingNowMs 替换。
// 本文件的用例都改包级接缝,**不得**加 t.Parallel()。

import (
	"context"
	"errors"
	"strconv"
	"testing"

	"match/internal/metrics"
	"match/internal/svc"

	battlepb "proto/battle"
	kafkapb "proto/contracts/kafka"
	matchpb "proto/match"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/require"
)

// snapshotSink 是发布接缝的替身:记录每次调用收到的事件;failNext > 0 时让接下来那么多次调用失败。
type snapshotSink struct {
	calls    int
	batches  [][]*kafkapb.PlayerRatingSnapshotEvent
	failNext int
}

func withSnapshotSink(t *testing.T) *snapshotSink {
	t.Helper()
	sink := &snapshotSink{}
	prev := ratingSnapshotPublishFn
	ratingSnapshotPublishFn = func(_ context.Context, _ *svc.ServiceContext, events []*kafkapb.PlayerRatingSnapshotEvent) error {
		sink.calls++
		if sink.failNext > 0 {
			sink.failNext--
			return errors.New("kafka down (test)")
		}
		sink.batches = append(sink.batches, events)
		return nil
	}
	t.Cleanup(func() { ratingSnapshotPublishFn = prev })
	return sink
}

// withRatingClock 把入账时钟换成可推进的替身。
func withRatingClock(t *testing.T, start uint64) *uint64 {
	t.Helper()
	now := start
	prev := ratingNowMs
	ratingNowMs = func() uint64 { return now }
	t.Cleanup(func() { ratingNowMs = prev })
	return &now
}

func hgetUint(t *testing.T, mr *miniredis.Miniredis, playerId uint64, field string) uint64 {
	t.Helper()
	v, err := strconv.ParseUint(mr.HGet(matchRatingKey(playerId), field), 10, 64)
	require.NoError(t, err, "player=%d field=%s", playerId, field)
	return v
}

func batchPlayers(batch []*kafkapb.PlayerRatingSnapshotEvent) []uint64 {
	out := make([]uint64, 0, len(batch))
	for _, ev := range batch {
		out = append(out, ev.GetPlayerId())
	}
	return out
}

func markerState(t *testing.T, mr *miniredis.Miniredis, battleId uint64) string {
	t.Helper()
	marker, err := mr.Get(matchRatingAppliedKey(battleId))
	require.NoError(t, err)
	state, _, ok := decodeRatingAppliedMarker(marker)
	require.True(t, ok, "marker=%q", marker)
	return state
}

// T4b:评分按十进制字符串精确解析。表里必须有 ×100 之后略小于整数的值(0.29 → 28.999…、0.57、1.13、2.01):
// 只有它们能让 uint32(f*100) 截断现形;x.15、1523.45 这类 ×100 后略大于整数,截断不出错,测不出来。
func TestParseRatingCenti(t *testing.T) {
	for raw, want := range map[string]uint32{
		"0.29": 29, "0.57": 57, "1.13": 113, "2.01": 201, // 截断会错的值
		"1516.29": 151629, "1523.45": 152345, "0.15": 15, "1500.00": 150000,
		"1500": 150000, "1500.5": 150050, "0": 0, "0.00": 0, "42949672.95": 4294967295,
	} {
		got, err := parseRatingCenti(raw)
		require.NoError(t, err, raw)
		require.Equal(t, want, got, raw)
	}
	for _, bad := range []string{"", "-1.00", "1.234", "abc", "1.2a", ".5", "1e3", " 1.00", "42949672.96"} {
		_, err := parseRatingCenti(bad)
		require.Error(t, err, "%q 应当解析失败", bad)
	}
}

// 局数两种类型都接受(写入分支整数 / 老脚本与续写的字符串);0、负数、缺元素都报错,不回落成 0。
func TestParseRatingSnapshotShapes(t *testing.T) {
	ok, snap, err := parseRatingSnapshot(1, []any{int64(1), "1516.29", int64(3), int64(1800000000123), int64(1800000000000)})
	require.NoError(t, err)
	require.True(t, ok)
	require.Equal(t, ratingSnapshot{raw: "1516.29", ratingCenti: 151629, games: 3, updatedAtMs: 1800000000123, ratingGen: 1800000000000}, snap)

	ok, snap, err = parseRatingSnapshot(1, []any{int64(0), "1500.00", "5", "1800000000123", "1800000000000"})
	require.NoError(t, err)
	require.False(t, ok)
	require.Equal(t, uint64(5), snap.games, "续写分支的字符串局数也要解析,不能当 0")

	for name, res := range map[string]any{
		"少于 5 个元素": []any{int64(1), "1500.00", int64(1)},
		"局数为 0":    []any{int64(0), "1500.00", int64(0), int64(1), int64(1)},
		"局数为负":     []any{int64(0), "1500.00", int64(-1), int64(1), int64(1)},
		"代际为 0":    []any{int64(1), "1500.00", int64(1), int64(1), int64(0)},
		"评分非法":     []any{int64(1), "", int64(1), int64(1), int64(1)},
		"不是数组":     "oops",
	} {
		_, _, err := parseRatingSnapshot(1, res)
		require.Error(t, err, name)
	}
}

// 开关关闭:不调用接缝,评分照常入账(行为与加快照之前一致)。
func TestRatingSnapshotNotPublishedWhenDisabled(t *testing.T) {
	svcCtx, _ := newTestSvcCtx(t)
	sink := withSnapshotSink(t)
	require.False(t, svcCtx.Config.RatingSnapshotPublishEnabled)

	outcome, err := ApplyBattleResult(svcCtx, resultEvent(9501, matchpb.MatchMode_MATCH_MODE_1V1,
		battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, []uint64{15001}, []uint64{15002}))
	require.NoError(t, err)
	require.Equal(t, ratingOutcomeApplied, outcome)
	require.Zero(t, sink.calls)
	require.InDelta(t, 1516, ratingOf(t, svcCtx, 15001), 0.01)
}

// 开关打开:一局发一批,每人一条,字段与评分 hash 同源(Lua 原子读出),顺序 teamA 再 teamB。
// rating_gen 与 updated_at_ms 刻意取**不同**的值(T0 先打一局建立代际,T1 打被测的这局):两者取同一个时间戳时,
// 把两个字段写反(Lua 返回顺序或事件组装)不会被发现,而写反会让 rank 每局都按 gen_reset 覆盖、失去拒旧能力。
func TestRatingSnapshotPublishedWithSameSourceFields(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	svcCtx.Config.RatingSnapshotPublishEnabled = true
	sink := withSnapshotSink(t)
	clock := withRatingClock(t, 1_800_000_000_000)
	t0 := *clock
	_, err := ApplyBattleResult(svcCtx, resultEvent(9500, matchpb.MatchMode_MATCH_MODE_1V1,
		battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, []uint64{15011}, []uint64{15012}))
	require.NoError(t, err)

	*clock += 7_000
	t1 := *clock
	okBefore := metrics.RatingSnapshotPublishValue(metrics.RatingSnapshotPublishOK)
	event := resultEvent(9502, matchpb.MatchMode_MATCH_MODE_1V1, battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN,
		[]uint64{15011}, []uint64{15012})
	event.FinishedAtMs = t1 - 500
	outcome, err := ApplyBattleResult(svcCtx, event)
	require.NoError(t, err)
	require.Equal(t, ratingOutcomeApplied, outcome)
	require.Equal(t, 2, sink.calls, "每局只发一批(前一局一批 + 本局一批)")
	batch := sink.batches[1]
	require.Equal(t, []uint64{15011, 15012}, batchPlayers(batch), "全员各一条,顺序 teamA 再 teamB")
	require.Equal(t, okBefore+2, metrics.RatingSnapshotPublishValue(metrics.RatingSnapshotPublishOK), "按消息条数计")

	for _, ev := range batch {
		pid := ev.GetPlayerId()
		centi, err := parseRatingCenti(mr.HGet(matchRatingKey(pid), ratingFieldRating))
		require.NoError(t, err)
		require.Equal(t, centi, ev.GetRatingCenti())
		require.Equal(t, uint64(2), ev.GetGames())
		require.Equal(t, hgetUint(t, mr, pid, ratingFieldGames), ev.GetGames())
		require.Equal(t, t1, ev.GetUpdatedAtMs(), "updated_at_ms 是本局写入时刻")
		require.Equal(t, hgetUint(t, mr, pid, ratingFieldUpdatedAt), ev.GetUpdatedAtMs())
		require.Equal(t, t0, ev.GetRatingGen(), "rating_gen 是首次写入时建立的代际,不是本局时刻")
		require.Equal(t, hgetUint(t, mr, pid, ratingFieldGen), ev.GetRatingGen())
		require.Equal(t, uint64(9502), ev.GetBattleId())
		require.Equal(t, t1-500, ev.GetFinishedAtMs())
	}
}

// T10:发布失败 → 返回 error、标记仍 applying;推进时钟后重试 → 走续写路径(所有人 written=0),
// 仍把**全员**的当前快照发一批:局数 > 0 且等于 hash 里的值,updated_at_ms 是本局首次写入时刻(不是重试时刻),
// rating_gen 是更早一局建立的代际(三个时刻 T0 < T1 < T2 各不相同,字段写反或取错时刻都会被发现)。
func TestRatingSnapshotRepublishedOnResume(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	svcCtx.Config.RatingSnapshotPublishEnabled = true
	sink := withSnapshotSink(t)
	clock := withRatingClock(t, 1_800_000_000_000)
	players := []uint64{15021, 15022, 15023, 15024}

	t0 := *clock // 先打一局,建立四个人的代际
	_, err := ApplyBattleResult(svcCtx, resultEvent(9499, matchpb.MatchMode_MATCH_MODE_5V5, battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN,
		players[:2], players[2:]))
	require.NoError(t, err)

	*clock += 5_000
	t1 := *clock
	sink.failNext = 1
	errBefore := metrics.RatingSnapshotPublishValue(metrics.RatingSnapshotPublishError)
	event := resultEvent(9503, matchpb.MatchMode_MATCH_MODE_5V5, battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_B_WIN,
		players[:2], players[2:])
	outcome, err := ApplyBattleResult(svcCtx, event)
	require.Error(t, err)
	require.Equal(t, ratingOutcomeError, outcome)
	require.Equal(t, errBefore+4, metrics.RatingSnapshotPublishValue(metrics.RatingSnapshotPublishError))
	require.Equal(t, ratingAppliedStateApplying, markerState(t, mr, 9503), "发布失败不能置 done,否则没有重试机会")

	*clock += 60_000 // 重试发生在一分钟后
	okBefore := metrics.RatingSnapshotPublishValue(metrics.RatingSnapshotPublishOK)
	outcome, err = ApplyBattleResult(svcCtx, event)
	require.NoError(t, err)
	require.Equal(t, ratingOutcomeApplied, outcome)
	require.Equal(t, 3, sink.calls)
	require.Len(t, sink.batches, 2, "前一局一批 + 本局重试成功的一批")
	retry := sink.batches[1]
	require.Equal(t, players, batchPlayers(retry), "续写路径所有人 written=0,仍要把全员的当前快照发出(漏发或空批 = 快照永久丢失)")
	require.Equal(t, okBefore+4, metrics.RatingSnapshotPublishValue(metrics.RatingSnapshotPublishOK))
	require.Equal(t, errBefore+4, metrics.RatingSnapshotPublishValue(metrics.RatingSnapshotPublishError), "重试成功后错误计数不再增长")
	for _, ev := range retry {
		pid := ev.GetPlayerId()
		require.Equal(t, uint64(2), ev.GetGames(), "续写路径的局数不能是 0")
		require.Equal(t, hgetUint(t, mr, pid, ratingFieldGames), ev.GetGames())
		require.Equal(t, t1, ev.GetUpdatedAtMs(), "updated_at_ms 取 Lua 读出的本局写入时刻,不是重试时刻")
		require.Equal(t, hgetUint(t, mr, pid, ratingFieldUpdatedAt), ev.GetUpdatedAtMs())
		require.Equal(t, t0, ev.GetRatingGen(), "rating_gen 是首局建立的代际")
		require.Equal(t, hgetUint(t, mr, pid, ratingFieldGen), ev.GetRatingGen())
	}
	marker, err := mr.Get(matchRatingAppliedKey(9503))
	require.NoError(t, err)
	require.Equal(t, ratingAppliedStateDone, marker)
}

// 开关打开 + 部分玩家写失败 → **不发布**(发布只在全员写完之后);修好后重试 → 一批全员,修好的那人 games=1。
// 把发布挪到 missing 检查之前的实现会给写失败的人发一条零值快照(games=0、rating_gen=0),本用例能抓到。
func TestRatingSnapshotNotPublishedOnPartialWrite(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	svcCtx.Config.RatingSnapshotPublishEnabled = true
	withZeroRatingWriteBackoff(t)
	sink := withSnapshotSink(t)
	withRatingClock(t, 1_800_000_000_000)
	players := []uint64{15041, 15042, 15043, 15044}
	require.NoError(t, mr.Set(matchRatingKey(15044), "not-a-hash")) // B 队第 2 人的 key 类型不对,Lua 报 WRONGTYPE

	event := resultEvent(9520, matchpb.MatchMode_MATCH_MODE_5V5, battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN,
		players[:2], players[2:])
	outcome, err := ApplyBattleResult(svcCtx, event)
	require.Error(t, err)
	require.Equal(t, ratingOutcomePartial, outcome)
	require.Zero(t, sink.calls, "有人没写成时一条快照都不能发")
	require.Equal(t, ratingAppliedStateApplying, markerState(t, mr, 9520))

	mr.Del(matchRatingKey(15044))
	outcome, err = ApplyBattleResult(svcCtx, event)
	require.NoError(t, err)
	require.Equal(t, ratingOutcomeApplied, outcome)
	require.Equal(t, 1, sink.calls)
	require.Equal(t, players, batchPlayers(sink.batches[0]))
	for _, ev := range sink.batches[0] {
		require.Equal(t, uint64(1), ev.GetGames(), "player=%d", ev.GetPlayerId())
		require.NotZero(t, ev.GetRatingGen(), "player=%d", ev.GetPlayerId())
	}
}

// presetResumeWithCorruptRating 造一个"本局已对两人都入账、标记仍 applying"的续写现场,其中 bad 的评分串被手工改坏(三位小数)。
func presetResumeWithCorruptRating(t *testing.T, mr *miniredis.Miniredis, battleId, good, bad uint64) {
	t.Helper()
	require.NoError(t, mr.Set(matchRatingAppliedKey(battleId), encodeRatingAppliedMarker(ratingAppliedStateApplying, 16)))
	bid := strconv.FormatUint(battleId, 10)
	for pid, rating := range map[uint64]string{good: "1516.00", bad: "1600.123"} {
		key := matchRatingKey(pid)
		mr.HSet(key, ratingFieldRating, rating)
		mr.HSet(key, ratingFieldGames, "1")
		mr.HSet(key, ratingFieldUpdatedAt, "1800000000000")
		mr.HSet(key, ratingFieldGen, "1800000000000")
		mr.HSet(key, ratingFieldRecentBattles, bid)
	}
}

// T4b(入账层):开关打开时快照字段解析失败 → 返回 error、不发布、标记仍 applying(不能拿残缺快照去发)。
func TestRatingSnapshotCorruptRatingBlocksPublishWhenEnabled(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	svcCtx.Config.RatingSnapshotPublishEnabled = true
	sink := withSnapshotSink(t)
	presetResumeWithCorruptRating(t, mr, 9530, 15051, 15052)

	_, err := ApplyBattleResult(svcCtx, resultEvent(9530, matchpb.MatchMode_MATCH_MODE_1V1,
		battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, []uint64{15051}, []uint64{15052}))
	require.Error(t, err)
	require.Zero(t, sink.calls)
	require.Equal(t, ratingAppliedStateApplying, markerState(t, mr, 9530))
}

// 开关关闭时同样的损坏数据只记日志、照常入账(快照字段没人用,不能阻断评分;与加快照之前的行为一致)。
func TestRatingSnapshotCorruptRatingIgnoredWhenDisabled(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	sink := withSnapshotSink(t)
	presetResumeWithCorruptRating(t, mr, 9531, 15053, 15054)

	outcome, err := ApplyBattleResult(svcCtx, resultEvent(9531, matchpb.MatchMode_MATCH_MODE_1V1,
		battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, []uint64{15053}, []uint64{15054}))
	require.NoError(t, err)
	require.Equal(t, ratingOutcomeApplied, outcome)
	require.Zero(t, sink.calls)
	marker, err := mr.Get(matchRatingAppliedKey(9531))
	require.NoError(t, err)
	require.Equal(t, ratingAppliedStateDone, marker)
}

// T13:rating_gen 首次写入时建立、之后不变;键被删后重建得到更大的代际;存量 hash(没有代际)不论走写入分支
// 还是续写分支,都在升级后的第一次执行时补上。
func TestRatingGenEstablishedAndAdvancesAfterKeyLoss(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	clock := withRatingClock(t, 1_800_000_000_000)
	const p, opp = uint64(15031), uint64(15032)

	play := func(battleId uint64) {
		t.Helper()
		_, err := ApplyBattleResult(svcCtx, resultEvent(battleId, matchpb.MatchMode_MATCH_MODE_1V1,
			battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, []uint64{p}, []uint64{opp}))
		require.NoError(t, err)
	}
	play(9511)
	gen1 := hgetUint(t, mr, p, ratingFieldGen)
	require.Equal(t, *clock, gen1)

	*clock += 1000
	play(9512)
	require.Equal(t, gen1, hgetUint(t, mr, p, ratingFieldGen), "同一代际内第二局不改代际")
	require.Equal(t, uint64(2), hgetUint(t, mr, p, ratingFieldGames))

	mr.Del(matchRatingKey(p)) // 评分键被淘汰 / 清空
	*clock += 1000
	play(9513)
	gen2 := hgetUint(t, mr, p, ratingFieldGen)
	require.Greater(t, gen2, gen1, "键丢失后重建的代际必须更大,排行榜才能覆盖旧条目")
	require.Equal(t, uint64(1), hgetUint(t, mr, p, ratingFieldGames), "键丢失后局数从 1 重计 —— 这正是需要代际的原因")

	// 存量 hash(写入分支):升级前写的,没有 rating_gen。
	const legacy = uint64(15033)
	mr.HSet(matchRatingKey(legacy), ratingFieldRating, "1600.00")
	mr.HSet(matchRatingKey(legacy), ratingFieldGames, "7")
	*clock += 1000
	_, err := ApplyBattleResult(svcCtx, resultEvent(9514, matchpb.MatchMode_MATCH_MODE_1V1,
		battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, []uint64{legacy}, []uint64{opp}))
	require.NoError(t, err)
	require.Equal(t, *clock, hgetUint(t, mr, legacy, ratingFieldGen), "存量 hash 在升级后第一次写入时补上代际")
	require.Equal(t, uint64(8), hgetUint(t, mr, legacy, ratingFieldGames))

	// 存量 hash(续写分支):升级前已对本局入账、标记仍 applying,升级后的续写也要补上代际并发出非 0 的 rating_gen。
	svcCtx.Config.RatingSnapshotPublishEnabled = true
	sink := withSnapshotSink(t)
	const legacyResume, oppResume = uint64(15034), uint64(15035)
	require.NoError(t, mr.Set(matchRatingAppliedKey(9515), encodeRatingAppliedMarker(ratingAppliedStateApplying, 16)))
	for _, pid := range []uint64{legacyResume, oppResume} {
		key := matchRatingKey(pid)
		mr.HSet(key, ratingFieldRating, "1516.00")
		mr.HSet(key, ratingFieldGames, "3")
		mr.HSet(key, ratingFieldUpdatedAt, "1700000000000")
		mr.HSet(key, ratingFieldRecentBattles, "9515")
	}
	*clock += 1000
	_, err = ApplyBattleResult(svcCtx, resultEvent(9515, matchpb.MatchMode_MATCH_MODE_1V1,
		battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN, []uint64{legacyResume}, []uint64{oppResume}))
	require.NoError(t, err)
	require.Equal(t, *clock, hgetUint(t, mr, legacyResume, ratingFieldGen), "续写分支同样补上代际")
	require.Equal(t, uint64(3), hgetUint(t, mr, legacyResume, ratingFieldGames), "续写不再计一局")
	require.Equal(t, 1, sink.calls)
	require.Equal(t, *clock, sink.batches[0][0].GetRatingGen())
	require.Equal(t, uint64(1700000000000), sink.batches[0][0].GetUpdatedAtMs(), "续写发出的是存量 hash 原有的 updated_at_ms")
}
