package logic

// 落地恢复摘要 bump 的单测(docs/design/landing-recovery-sync-digest.md §3.3 / §3.5;不连库)。
//
// 两层分开测:
//   - AsyncGuildSyncBumper(push.go):异步、整批一个预算、去重丢 0、不别名调用方切片。替身有两种:
//     fakePlayerBumper 站在 *playersync.Bumper 的位置上(看批次与预算);fakeDigestEvaler 站在 playersync.Evaler
//     的位置上、下面接真的 playersync.Bumper(看每条 Lua 的键与域号、某人失败不中断整批)。
//   - logic 的 bump 点:受影响玩家与域对不对、提交之前的拒绝不 bump、bump 失败不影响 RPC 结果。
//     替身是 recordingSyncBumper(GuildSyncBumper 的同步记录器,同 recordingNotifier 的套路)。
//
// 管理类写(踢人 / 离帮 / 审批 …)与捐献 / 兑换的预留要 MySQL,完整流程在 economy_flow_integration_test.go
// 的 *SyncDigest* 用例(build tag integration);这里覆盖它们共用的纯函数与不碰库的那一段。

import (
	"context"
	"errors"
	"slices"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"guild/internal/constants"
	"guild/internal/data"
	battlepb "proto/battle"
	pb "proto/guild"
	scenepb "proto/scene"
	"shared/assetop"
	"shared/playersync"
)

const (
	syncGuild       = scenepb.SyncDomain_SYNC_DOMAIN_GUILD
	syncApplication = scenepb.SyncDomain_SYNC_DOMAIN_GUILD_APPLICATION
	syncEconomy     = scenepb.SyncDomain_SYNC_DOMAIN_GUILD_ECONOMY
	syncActivity    = scenepb.SyncDomain_SYNC_DOMAIN_GUILD_ACTIVITY
)

// ── 测试替身 ──────────────────────────────────────────────────

// recordedBump 是一次 GuildSyncBumper.Bump 调用的原样记录。
type recordedBump struct {
	players []uint64
	domains []scenepb.SyncDomain
}

// recordingSyncBumper 记下每一次 bump。加锁:结算与重投循环的回调可能在别的协程里调用它。
type recordingSyncBumper struct {
	mu    sync.Mutex
	calls []recordedBump
}

func (b *recordingSyncBumper) Bump(players []uint64, domains ...scenepb.SyncDomain) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.calls = append(b.calls, recordedBump{players: slices.Clone(players), domains: slices.Clone(domains)})
}

func (b *recordingSyncBumper) count() int {
	b.mu.Lock()
	defer b.mu.Unlock()
	return len(b.calls)
}

// countOf 返回含 domain 的调用次数。
func (b *recordingSyncBumper) countOf(domain scenepb.SyncDomain) int {
	b.mu.Lock()
	defer b.mu.Unlock()
	n := 0
	for _, c := range b.calls {
		if slices.Contains(c.domains, domain) {
			n++
		}
	}
	return n
}

func (b *recordingSyncBumper) reset() {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.calls = nil
}

// playersOf 返回被 bump 过 domain 的玩家,按 GuildSyncBumper 的契约去重、丢 0,升序。
func (b *recordingSyncBumper) playersOf(domain scenepb.SyncDomain) []uint64 {
	b.mu.Lock()
	defer b.mu.Unlock()
	out := []uint64{}
	for _, c := range b.calls {
		if !slices.Contains(c.domains, domain) {
			continue
		}
		for _, id := range c.players {
			if id != 0 && !slices.Contains(out, id) {
				out = append(out, id)
			}
		}
	}
	slices.Sort(out)
	return out
}

// domainsOf 返回 playerID 被 bump 过的域,去重、升序。
func (b *recordingSyncBumper) domainsOf(playerID uint64) []scenepb.SyncDomain {
	b.mu.Lock()
	defer b.mu.Unlock()
	out := []scenepb.SyncDomain{}
	for _, c := range b.calls {
		if !slices.Contains(c.players, playerID) {
			continue
		}
		for _, d := range c.domains {
			if !slices.Contains(out, d) {
				out = append(out, d)
			}
		}
	}
	slices.Sort(out)
	return out
}

// playerBumpBatch 是 PlayerSyncBumper.BumpAll 收到的一批。
type playerBumpBatch struct {
	players     []uint64
	domains     []scenepb.SyncDomain
	hasDeadline bool
	deadline    time.Time
}

// fakePlayerBumper 站在 *playersync.Bumper 的位置上,只看批次与预算。
// block 为真时阻塞到 ctx 结束再回 ctx.Err()(模拟 Redis 卡住);构造时设好、之后只读。
// batches 由被测协程写,读一律经 snapshot。
type fakePlayerBumper struct {
	block bool

	mu      sync.Mutex
	batches []playerBumpBatch
}

func (f *fakePlayerBumper) BumpAll(ctx context.Context, playerIDs []uint64, domains ...scenepb.SyncDomain) error {
	deadline, ok := ctx.Deadline()
	f.mu.Lock()
	f.batches = append(f.batches, playerBumpBatch{
		players: slices.Clone(playerIDs), domains: slices.Clone(domains), hasDeadline: ok, deadline: deadline,
	})
	f.mu.Unlock()
	if f.block {
		<-ctx.Done()
		return ctx.Err()
	}
	return nil
}

func (f *fakePlayerBumper) snapshot() []playerBumpBatch {
	f.mu.Lock()
	defer f.mu.Unlock()
	return slices.Clone(f.batches)
}

// digestEvalCall 是 playersync.Evaler 收到的一条 Lua。
type digestEvalCall struct {
	keys        []string
	args        []string
	hasDeadline bool
}

// fakeDigestEvaler 站在 playersync.Evaler 的位置上(下面接真的 playersync.Bumper),记录每条 Lua 的键与参数。
// fail 非 nil 时按键答复(构造时设好、之后只读)。
type fakeDigestEvaler struct {
	fail func(key string) error

	mu    sync.Mutex
	calls []digestEvalCall
}

func (e *fakeDigestEvaler) Eval(ctx context.Context, _ string, keys []string, args ...string) error {
	_, ok := ctx.Deadline()
	e.mu.Lock()
	e.calls = append(e.calls, digestEvalCall{keys: slices.Clone(keys), args: slices.Clone(args), hasDeadline: ok})
	e.mu.Unlock()
	if e.fail != nil && len(keys) > 0 {
		return e.fail(keys[0])
	}
	return nil
}

func (e *fakeDigestEvaler) snapshot() []digestEvalCall {
	e.mu.Lock()
	defer e.mu.Unlock()
	return slices.Clone(e.calls)
}

func errRedisDown(string) error { return errors.New("dial tcp 127.0.0.1:6379: connection refused") }

// ── AsyncGuildSyncBumper ──────────────────────────────────────

// TestNewGuildSyncBumperNilIsNoop:没接摘要 Redis 时退回 Noop(与 NewGuildNotifier 同一收口),
// 接上时预算取设计 §3.5 第 6 条的 2 秒。
func TestNewGuildSyncBumperNilIsNoop(t *testing.T) {
	assert.IsType(t, NoopSyncBumper{}, NewGuildSyncBumper(nil))
	assert.NotPanics(t, func() { NoopSyncBumper{}.Bump([]uint64{42}, syncGuild) })

	b, ok := NewGuildSyncBumper(&fakePlayerBumper{}).(*AsyncGuildSyncBumper)
	require.True(t, ok)
	assert.Equal(t, 2*time.Second, b.budget)
}

// TestAsyncGuildSyncBumperFansOutToWholeGuild:全帮扇出(上限 100 人)是**一批**、同一组域;0 与重复在交出前丢掉;
// 这一批带截止时间(整批共用一个预算,不继承请求 ctx);调用方在 Bump 返回后复用切片不影响已交出的批次。
func TestAsyncGuildSyncBumperFansOutToWholeGuild(t *testing.T) {
	inner := &fakePlayerBumper{}
	b := NewGuildSyncBumper(inner)
	players := make([]uint64, 0, constants.MaxGuildMembersCap+2)
	for id := uint64(1); id <= constants.MaxGuildMembersCap; id++ {
		players = append(players, id)
	}
	players = append(players, 0, 1)
	domains := []scenepb.SyncDomain{syncGuild, syncApplication}
	start := time.Now()

	b.Bump(players, domains...)
	// 调用方复用切片(快照成员、可变参数的底层数组都可能被复用):已交出的批次不得看到这些改动。
	for i := range players {
		players[i] = 999_999
	}
	domains[0] = scenepb.SyncDomain_SYNC_DOMAIN_UNSPECIFIED

	require.Eventually(t, func() bool { return len(inner.snapshot()) == 1 }, 3*time.Second, 5*time.Millisecond)
	batch := inner.snapshot()[0]
	want := make([]uint64, 0, constants.MaxGuildMembersCap)
	for id := uint64(1); id <= constants.MaxGuildMembersCap; id++ {
		want = append(want, id)
	}
	assert.Equal(t, want, batch.players, "全帮每人恰好一次;0 与重复已丢掉;不别名调用方的切片")
	assert.Equal(t, []scenepb.SyncDomain{syncGuild, syncApplication}, batch.domains)
	assert.True(t, batch.hasDeadline, "整批必须有预算,Redis 卡住时不能无限期占着协程")
	assert.True(t, batch.deadline.After(start), "预算从起协程时算起")
	assert.False(t, batch.deadline.After(time.Now().Add(guildSyncBumpBudget)), "预算不得超过 guildSyncBumpBudget")
}

// TestAsyncGuildSyncBumperDoesNotBlockCaller:Redis 卡住时 Bump 照样立即返回(不占 RPC 预算),
// 卡住的协程最迟在预算到期时被 ctx 放出来。
func TestAsyncGuildSyncBumperDoesNotBlockCaller(t *testing.T) {
	inner := &fakePlayerBumper{block: true}
	b := &AsyncGuildSyncBumper{inner: inner, budget: time.Second}

	begin := time.Now()
	b.Bump([]uint64{1, 2, 3}, syncEconomy)
	assert.Less(t, time.Since(begin), 200*time.Millisecond, "Bump 必须异步:同步实现在这里至少阻塞一整个预算")

	require.Eventually(t, func() bool { return len(inner.snapshot()) == 1 }, 2*time.Second, 5*time.Millisecond)
	batch := inner.snapshot()[0]
	require.True(t, batch.hasDeadline)
	assert.False(t, batch.deadline.After(begin.Add(time.Second+200*time.Millisecond)), "卡住的协程只能占一个预算")
}

// TestAsyncGuildSyncBumperSkipsEmptyBatches:没有人或没有域时什么都不做(连协程都不起)。
// 末尾那一批正常的作栅栏:它到达时,前面几批若起过协程也早该到了。
func TestAsyncGuildSyncBumperSkipsEmptyBatches(t *testing.T) {
	inner := &fakePlayerBumper{}
	b := &AsyncGuildSyncBumper{inner: inner, budget: time.Second}

	b.Bump(nil, syncGuild)
	b.Bump([]uint64{0, 0}, syncGuild)
	b.Bump([]uint64{1})
	b.Bump([]uint64{7}, syncGuild)

	require.Eventually(t, func() bool { return len(inner.snapshot()) >= 1 }, 2*time.Second, 5*time.Millisecond)
	batches := inner.snapshot()
	require.Len(t, batches, 1)
	assert.Equal(t, []uint64{7}, batches[0].players)
}

// TestGuildSyncBumperThroughPlayersync:接上真的 playersync.Bumper(下面是假 Evaler),看落到 Redis 的那一层:
// 每人一条 Lua、键是 player:{id}:sync_digest、ARGV[2..] 是域号十进制(GUILD=6、GUILD_APPLICATION=7,§3.3 表);
// 某人失败不中断整批(其余人照样 bump),也不 panic;每条命令都带着整批的预算。
func TestGuildSyncBumperThroughPlayersync(t *testing.T) {
	eval := &fakeDigestEvaler{fail: func(key string) error {
		if key == playersync.Key(2) {
			return errRedisDown(key)
		}
		return nil
	}}
	b := NewGuildSyncBumper(playersync.NewBumper(eval, "guild"))

	b.Bump([]uint64{1, 2, 3, 0, 1}, syncGuild, syncApplication)

	require.Eventually(t, func() bool { return len(eval.snapshot()) == 3 }, 2*time.Second, 5*time.Millisecond,
		"2 失败之后 3 照样 bump")
	for i, id := range []uint64{1, 2, 3} {
		call := eval.snapshot()[i]
		assert.Equal(t, []string{playersync.Key(id)}, call.keys)
		require.Len(t, call.args, 3, "ARGV = TTL 秒 + 两个域号")
		assert.Equal(t, []string{"6", "7"}, call.args[1:])
		assert.True(t, call.hasDeadline)
	}
}

// TestBumpSyncWithoutBumperDoesNotPanic:未经 NewGuildLogic 构造(syncBumper 为 nil)时只记日志 ——
// bumpSync 跑在提交之后,panic 等于把一次写成功的操作报成 RPC 失败。
func TestBumpSyncWithoutBumperDoesNotPanic(t *testing.T) {
	l := &GuildLogic{}
	assert.NotPanics(t, func() { l.bumpSync([]uint64{42}, syncGuild) })
}

// TestWithSyncBumper:默认 Noop;传 nil 被忽略(不覆盖成 nil);显式值原样保留。
func TestWithSyncBumper(t *testing.T) {
	assert.IsType(t, NoopSyncBumper{}, NewGuildLogic(nil, nil, nil, nil, nil).syncBumper)
	assert.IsType(t, NoopSyncBumper{}, NewGuildLogic(nil, nil, nil, nil, nil, WithSyncBumper(nil)).syncBumper)

	rec := &recordingSyncBumper{}
	assert.Same(t, rec, NewGuildLogic(nil, nil, nil, nil, nil, WithSyncBumper(rec)).syncBumper)
}

// ── 纯函数 ────────────────────────────────────────────────────

// TestMembersPlusAndReviewersIn:membersPlus = 快照全体 + 已不在快照里的当事人;reviewersIn 只取长老与帮主
// (与 guildInfoFor 判"看不看得到待审数"同一个判据)。
func TestMembersPlusAndReviewersIn(t *testing.T) {
	g := &data.GuildData{GuildID: 9, Members: []data.MemberData{
		{PlayerID: 42, Role: constants.RoleLeader},
		{PlayerID: 43, Role: constants.RoleOfficer},
		{PlayerID: 44, Role: constants.RoleMember},
	}}

	assert.Equal(t, []uint64{42, 43, 44, 7}, membersPlus(g, 7))
	assert.Equal(t, []uint64{42, 43, 44}, membersPlus(g))
	assert.Equal(t, []uint64{7}, membersPlus(nil, 7), "快照缺失时至少还有当事人")
	assert.Equal(t, []uint64{42, 43}, reviewersIn(g))
	assert.Empty(t, reviewersIn(nil))
}

// TestAssetFinalizedSyncDomains:捐献 / 兑换 → 经济页;活动奖励 → 活动页(两个经济页按 kind 过滤掉它);
// 未知 kind 两个域都 bump —— 多拉一次无害,漏拉才违反 L-1。
func TestAssetFinalizedSyncDomains(t *testing.T) {
	cases := []struct {
		kind pb.GuildAssetOpKind
		want []scenepb.SyncDomain
	}{
		{pb.GuildAssetOpKind_GUILD_ASSET_OP_KIND_DONATE, []scenepb.SyncDomain{syncEconomy}},
		{pb.GuildAssetOpKind_GUILD_ASSET_OP_KIND_SHOP, []scenepb.SyncDomain{syncEconomy}},
		{pb.GuildAssetOpKind_GUILD_ASSET_OP_KIND_ACTIVITY_REWARD, []scenepb.SyncDomain{syncActivity}},
		{pb.GuildAssetOpKind(99), []scenepb.SyncDomain{syncEconomy, syncActivity}},
	}
	for _, tc := range cases {
		assert.Equal(t, tc.want, assetFinalizedSyncDomains(tc.kind), "kind=%d", tc.kind)
	}
}

// ── 资产指令终结(§3.3 表第 8 行)───────────────────────────────

// TestOnAssetFinalizedBumpsOwnerOnEveryPath:终结后只 bump 本人;**同步投递路径同样 bump**
// (推送在那条路上被压掉,因为调用方拿着回包;但回包可能在断线时丢,摘要不能跟着压)。
func TestOnAssetFinalizedBumpsOwnerOnEveryPath(t *testing.T) {
	kinds := []pb.GuildAssetOpKind{
		pb.GuildAssetOpKind_GUILD_ASSET_OP_KIND_DONATE,
		pb.GuildAssetOpKind_GUILD_ASSET_OP_KIND_SHOP,
		pb.GuildAssetOpKind_GUILD_ASSET_OP_KIND_ACTIVITY_REWARD,
	}
	for _, kind := range kinds {
		for _, syncDelivery := range []bool{false, true} {
			rec := &recordingSyncBumper{}
			notifier := &recordingNotifier{}
			l := NewGuildLogic(nil, nil, nil, nil, nil, WithSyncBumper(rec), WithNotifier(notifier))
			ctx := context.Background()
			if syncDelivery {
				ctx = withSyncDelivery(ctx)
			}

			l.OnAssetFinalized(ctx, data.FinalizedOp{OpID: 1, PlayerID: 42, GuildID: 9, Kind: kind, Status: assetop.StatusApplied})

			require.Equal(t, 1, rec.count(), "kind=%s sync=%t", kind, syncDelivery)
			assert.Equal(t, []uint64{42}, rec.calls[0].players, "只 bump 本人,不扇出全帮")
			assert.Equal(t, assetFinalizedSyncDomains(kind), rec.calls[0].domains)
			if syncDelivery {
				assert.Zero(t, notifier.total(), "同步投递路径的推送仍然压掉")
			} else {
				assert.Equal(t, 1, notifier.total())
			}
		}
	}
}

// ── 灯会 / 团圆(§3.3 表第 9 行)───────────────────────────────

// TestAfterActivityCommitBumps:每次参与只 bump 本人;本档期首次达阈值时扇出全帮(含本人)。
// 事务要 MySQL,这里直接调提交后的那一段(与 LightGuildLantern / ClaimGuildReunion 同一个函数)。
func TestAfterActivityCommitBumps(t *testing.T) {
	g := activityGuild(1, 42, 43, 44)
	a := activityActor{playerID: 43, guild: &g, member: g.Members[1]}

	t.Run("未达阈值只给本人", func(t *testing.T) {
		rec := &recordingSyncBumper{}
		l := NewGuildLogic(nil, nil, nil, nil, nil, WithSyncBumper(rec))

		l.afterActivityCommit(context.Background(), &ActivityDeps{}, a, activityReward{}, data.ActivityTxResult{}, activityTypeLantern)

		require.Equal(t, 1, rec.count())
		assert.Equal(t, []uint64{43}, rec.playersOf(syncActivity))
		assert.Equal(t, []scenepb.SyncDomain{syncActivity}, rec.domainsOf(43), "灯会进度是帮会级数值,不 bump 其它域")
	})
	t.Run("首次达阈值扇出全帮", func(t *testing.T) {
		rec := &recordingSyncBumper{}
		l := NewGuildLogic(nil, nil, nil, nil, nil, WithSyncBumper(rec))

		l.afterActivityCommit(context.Background(), &ActivityDeps{}, a, activityReward{},
			data.ActivityTxResult{ReachedNow: true, FundsGranted: true}, activityTypeReunion)

		assert.Equal(t, []uint64{42, 43, 44}, rec.playersOf(syncActivity), "全帮含操作者本人")
	})
}

// ── 审批(§3.3 表第 6、7 行)────────────────────────────────────

// TestBumpReviewed:通过 → 申请人 GUILD_APPLICATION + 全帮(含申请人与审批人)GUILD;
// 拒绝 → 申请人只有 GUILD_APPLICATION(他仍不在帮,GetPlayerGuild 不变),审批人(长老 / 帮主)GUILD(待审数少了一条)。
func TestBumpReviewed(t *testing.T) {
	const applicant uint64 = 45
	members := []data.MemberData{
		{PlayerID: 42, Role: constants.RoleLeader},
		{PlayerID: 43, Role: constants.RoleOfficer},
		{PlayerID: 44, Role: constants.RoleMember},
	}

	t.Run("通过", func(t *testing.T) {
		rec := &recordingSyncBumper{}
		l := NewGuildLogic(nil, nil, nil, nil, nil, WithSyncBumper(rec))
		snapshot := &data.GuildData{GuildID: 9, Members: append(slices.Clone(members), data.MemberData{PlayerID: applicant, Role: constants.RoleMember})}

		l.bumpReviewed(data.ReviewResult{Guild: snapshot, Approved: true}, applicant)

		assert.Equal(t, []uint64{applicant}, rec.playersOf(syncApplication))
		assert.Equal(t, []uint64{42, 43, 44, applicant}, rec.playersOf(syncGuild))
		assert.Equal(t, []scenepb.SyncDomain{syncGuild, syncApplication}, rec.domainsOf(applicant))
	})
	t.Run("拒绝", func(t *testing.T) {
		rec := &recordingSyncBumper{}
		l := NewGuildLogic(nil, nil, nil, nil, nil, WithSyncBumper(rec))
		snapshot := &data.GuildData{GuildID: 9, Members: slices.Clone(members)}

		l.bumpReviewed(data.ReviewResult{Guild: snapshot, Approved: false}, applicant)

		assert.Equal(t, []uint64{applicant}, rec.playersOf(syncApplication))
		assert.Equal(t, []uint64{42, 43}, rec.playersOf(syncGuild), "只有能看到待审数的人")
		assert.Equal(t, []scenepb.SyncDomain{syncApplication}, rec.domainsOf(applicant))
		assert.Empty(t, rec.domainsOf(44), "普通成员看不到待审数")
	})
}

// ── 提交之前的拒绝不 bump(§3.5 第 1 条)────────────────────────

// TestWritesRefusedBeforeCommitDoNotBump:合服闸门在碰库之前拒绝的全部写 RPC(管理 / 申请 / 建帮 / 离帮 / 解散 / 公告)
// 一次都不 bump —— 先 bump 后提交(或不提交)会让客户端把旧数据记成新版本。
func TestWritesRefusedBeforeCommitDoNotBump(t *testing.T) {
	for _, tc := range fenceGuardedWrites() {
		t.Run(tc.name, func(t *testing.T) {
			rec := &recordingSyncBumper{}
			l := NewGuildLogic(nil, nil, nil, &fakeFence{merging: true}, &fakeHomeZones{zone: 2}, WithSyncBumper(rec))

			id, err := tc.run(l, clientCtx(42))

			require.NoError(t, err)
			require.Equal(t, constants.ErrZoneMerging, id)
			assert.Zero(t, rec.count())
		})
	}
}

// ── 同道历练(§3.3 表第 9 行)───────────────────────────────────

// TestTrialLobbyChangesBumpWholeRoster:建房、同意、开战都给名单全员(含操作者本人)bump GUILD_ACTIVITY;
// 名单外的成员不 bump;重复点击(房间没变)不 bump。
func TestTrialLobbyChangesBumpWholeRoster(t *testing.T) {
	useTrialTables(t, nil, nil)
	f := newTrialFixture(t, activityGuild(1, 42, 43, 44, 45), nil)
	rec := &recordingSyncBumper{}
	f.l.syncBumper = rec
	f.setOnline(t, 42, 43, 44, 45)

	lobby := f.mustStart(t, 42, 42, 43, 44)
	require.Equal(t, 1, rec.count(), "建房一次")
	assert.Equal(t, []uint64{42, 43, 44}, rec.playersOf(syncActivity), "名单全员含发起人;名单外的 45 不 bump")
	assert.Equal(t, []scenepb.SyncDomain{syncActivity}, rec.domainsOf(42))

	accepted, err := f.respond(43, lobby.GetLobbyId(), true)
	require.NoError(t, err)
	require.Nil(t, accepted.GetErrorMessage())
	require.Equal(t, 2, rec.count(), "同意一次")
	assert.ElementsMatch(t, []uint64{42, 43, 44}, rec.calls[1].players, "含同意者本人")

	launched, err := f.respond(44, lobby.GetLobbyId(), true)
	require.NoError(t, err)
	require.Nil(t, launched.GetErrorMessage())
	require.Equal(t, pb.GuildTrialLobbyState_GUILD_TRIAL_LOBBY_STATE_LAUNCHED, launched.GetActivity().GetTrialLobby().GetState())
	require.Equal(t, 3, rec.count(), "开战一次")
	assert.ElementsMatch(t, []uint64{42, 43, 44}, rec.calls[2].players)

	repeat, err := f.respond(44, lobby.GetLobbyId(), true)
	require.NoError(t, err)
	require.Nil(t, repeat.GetErrorMessage())
	assert.Equal(t, 3, rec.count(), "重复点击房间没变,不 bump")
}

// TestTrialDeclineBumpsAndRejectedStartDoesNot:拒绝让房间结束 → 名单全员 bump;
// 名单不合法在建房之前就拒绝 → 不 bump。
func TestTrialDeclineBumpsAndRejectedStartDoesNot(t *testing.T) {
	useTrialTables(t, nil, nil)
	f := newTrialFixture(t, activityGuild(1, 42, 43, 44), nil)
	rec := &recordingSyncBumper{}
	f.l.syncBumper = rec
	f.setOnline(t, 42, 43, 44)

	refused, err := f.start(42, 42, 43, 43)
	require.NoError(t, err)
	require.Equal(t, constants.ErrTrialTeamInvalid, refused.GetErrorMessage().GetId())
	assert.Zero(t, rec.count(), "没建房就没有可 bump 的变化")

	lobby := f.mustStart(t, 42, 42, 43, 44)
	declined, err := f.respond(44, lobby.GetLobbyId(), false)
	require.NoError(t, err)
	require.Nil(t, declined.GetErrorMessage())
	require.Equal(t, 2, rec.count())
	assert.ElementsMatch(t, []uint64{42, 43, 44}, rec.calls[1].players, "含拒绝者本人")
}

// TestTrialSettleBumps:负局只 bump 己方参战者;计了帮会资金的胜局扇出全帮;重复结果什么都没改,不 bump。
func TestTrialSettleBumps(t *testing.T) {
	useTrialTables(t, nil, nil)

	t.Run("负局只给参战者", func(t *testing.T) {
		f := newTrialFixture(t, activityGuild(1, 42, 43, 44, 45), nil)
		rec := &recordingSyncBumper{}
		f.l.syncBumper = rec
		ev := trialEvent(501, 42, 43)
		ev.Outcome = battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_B_WIN
		f.putResult(t, ev)

		outcome, err := f.l.SettleTrialResult(context.Background(), ev)

		require.NoError(t, err)
		require.Equal(t, TrialOutcomeSettled, outcome)
		assert.Equal(t, []uint64{42, 43}, rec.playersOf(syncActivity))
	})
	t.Run("计了资金扇出全帮", func(t *testing.T) {
		f := newTrialFixture(t, activityGuild(1, 42, 43, 44, 45), nil)
		rec := &recordingSyncBumper{}
		f.l.syncBumper = rec
		f.store.settle = func(data.TrialSettleInput) (data.TrialSettleResult, error) {
			return data.TrialSettleResult{
				Status: data.TrialSettleSettled, Result: pb.GuildTrialSettleResult_GUILD_TRIAL_SETTLE_RESULT_WIN,
				Rewarded: []uint64{42, 43}, FundsGranted: true, CountedWins: 1,
			}, nil
		}
		ev := trialEvent(502, 42, 43)
		f.putResult(t, ev)

		_, err := f.l.SettleTrialResult(context.Background(), ev)

		require.NoError(t, err)
		assert.Equal(t, []uint64{42, 43, 44, 45}, rec.playersOf(syncActivity))
	})
	t.Run("重复结果不 bump", func(t *testing.T) {
		f := newTrialFixture(t, activityGuild(1, 42, 43), nil)
		rec := &recordingSyncBumper{}
		f.l.syncBumper = rec
		f.store.settle = func(data.TrialSettleInput) (data.TrialSettleResult, error) {
			return data.TrialSettleResult{Status: data.TrialSettleDuplicate}, nil
		}
		ev := trialEvent(503, 42, 43)
		f.putResult(t, ev)

		outcome, err := f.l.SettleTrialResult(context.Background(), ev)

		require.NoError(t, err)
		require.Equal(t, TrialOutcomeDuplicate, outcome)
		assert.Zero(t, rec.count())
	})
}

// ── bump 失败不影响业务结果(§3.5 第 5 条)──────────────────────

// TestSyncBumpFailureDoesNotAffectRPC:摘要 Redis 全部失败时,RPC 照常成功、回包完整;
// 失败的 bump 仍逐人尝试过(由 playersync 计指标),只是结果被丢弃。
func TestSyncBumpFailureDoesNotAffectRPC(t *testing.T) {
	useTrialTables(t, nil, nil)
	f := newTrialFixture(t, activityGuild(1, 42, 43), nil)
	eval := &fakeDigestEvaler{fail: errRedisDown}
	f.l.syncBumper = NewGuildSyncBumper(playersync.NewBumper(eval, "guild"))
	f.setOnline(t, 42, 43)

	resp, err := f.start(42, 42, 43)

	require.NoError(t, err)
	require.Nil(t, resp.GetErrorMessage(), "bump 失败绝不能变成业务拒绝")
	require.NotNil(t, resp.GetActivity().GetTrialLobby(), "回包照常带房间")
	require.Eventually(t, func() bool { return len(eval.snapshot()) == 2 }, 2*time.Second, 5*time.Millisecond,
		"失败的 bump 照样逐人尝试过")
}
