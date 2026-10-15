package team

// 落地摘要 bump 的回归(设计文档 docs/design/landing-recovery-sync-digest.md §3.3 TEAM / TEAM_INVITE 两行)。
//
// 三层:规则层的两个标注(ExpiryOnly / InvitesExpired)、syncDigestTargets 纯函数、
// 服务层经 publish 的真实 RPC 流程(记录型假 Evaler 断言给谁 bump 了哪个域)。
// 最后一条在 miniredis 上经 go-zero 适配器真实执行 BumpScript,钉住装配形态。

import (
	"context"
	"slices"
	"strconv"
	"strings"
	"sync"
	"testing"

	"match/internal/config"

	scenepb "proto/scene"
	teampb "proto/team"
	"shared/playersync"

	"github.com/stretchr/testify/require"
)

const (
	domTeam       = scenepb.SyncDomain_SYNC_DOMAIN_TEAM
	domTeamInvite = scenepb.SyncDomain_SYNC_DOMAIN_TEAM_INVITE
)

// ---- 规则层标注 ----

// 纯按时过期的清理:ExpiryOnly,被清掉的邀请记进 InvitesExpired。
func TestRulesMarkExpiryOnlyCleanup(t *testing.T) {
	rec := newRecord(1, 2)
	withInvite(rec, 9, ruleZone, ruleNow-InviteTTLMs, ruleNow) // 恰好过期(expire <= now)
	withApplication(rec, 8, ruleZone, ruleNow-ApplicationTTLMs, ruleNow-1)

	d := Apply(RefreshOp(1), rec, ruleNow, allOnline(1, 2), RuleConfig{})
	require.True(t, d.Changed)
	require.True(t, d.ExpiryOnly, "只清了过期项,没有操作也没有转让")
	require.Equal(t, []uint64{9}, d.InvitesRemoved)
	require.Equal(t, []uint64{9}, d.InvitesExpired)
}

// 操作变更顺带清过期项:不是 ExpiryOnly;InvitesExpired 只含过期的那条,解散撤销的未过期邀请不算过期。
func TestRulesSeparateExpiredFromRevokedInvites(t *testing.T) {
	rec := newRecord(1, 2)
	// 9 已过期,10 仍有效。
	withInvite(rec, 9, ruleZone, ruleNow-InviteTTLMs, ruleNow)
	withInvite(rec, 10, ruleZone, ruleNow-1000, ruleNow+1000)

	d := Apply(DisbandOp(1), rec, ruleNow, allOnline(1, 2), RuleConfig{})
	require.True(t, d.Changed)
	require.False(t, d.ExpiryOnly)
	require.ElementsMatch(t, []uint64{9, 10}, d.InvitesRemoved)
	require.Equal(t, []uint64{9}, d.InvitesExpired)
	require.Equal(t, []uint64{10}, d.RevokedInvitees)

	// 新发邀请同时清掉一条过期邀请。
	rec = newRecord(1, 2)
	withInvite(rec, 9, ruleZone, ruleNow-InviteTTLMs, ruleNow)
	d = Apply(InviteOp(1, 11, ruleZone), rec, ruleNow, allOnline(1, 2), RuleConfig{})
	require.False(t, d.ExpiryOnly)
	require.Equal(t, []uint64{9}, d.InvitesExpired)
	require.Equal(t, uint64(11), d.InvitedPlayer)
}

// 惰性转让队长是真实变化(队长变了),即使同时只清了过期项也不是 ExpiryOnly。
func TestRulesLeaderTransferIsNotExpiryOnly(t *testing.T) {
	rec := newRecord(1, 2)
	withApplication(rec, 8, ruleZone, ruleNow-ApplicationTTLMs, ruleNow-1)
	sessions := Sessions{1: SessionAbsent, 2: SessionOnline}

	d := Apply(RefreshOp(2), rec, ruleNow, sessions, RuleConfig{})
	require.True(t, d.Changed)
	require.True(t, d.LeaderOfflineTransferred)
	require.False(t, d.ExpiryOnly)
}

// ---- syncDigestTargets ----

func TestSyncDigestTargets(t *testing.T) {
	cases := []struct {
		name       string
		caller     uint64
		commits    []*CommitResult
		healed     []uint64
		wantTeam   []uint64
		wantInvite []uint64
	}{
		{
			name:   "成员变化:J∪K∪L 加调用者",
			caller: 5,
			commits: []*CommitResult{{Decision: Decision{
				Changed: true, Joined: []uint64{3}, Kept: []uint64{1, 2}, Left: []uint64{4},
			}}},
			wantTeam: []uint64{3, 1, 2, 4, 5},
		},
		{
			name:   "后台提交(caller=0)不加 0",
			caller: 0,
			commits: []*CommitResult{{Decision: Decision{
				Changed: true, Kept: []uint64{1, 2},
			}}},
			wantTeam: []uint64{1, 2},
		},
		{
			name:   "纯过期清理:谁都不 bump,调用者也不算",
			caller: 1,
			commits: []*CommitResult{{Decision: Decision{
				Changed:        true,
				ExpiryOnly:     true,
				Kept:           []uint64{1, 2},
				InvitesRemoved: []uint64{9},
				InvitesExpired: []uint64{9},
			}}},
		},
		{
			name:   "邀请:新增与非过期移除给 TEAM_INVITE,过期移除不给",
			caller: 1,
			commits: []*CommitResult{{Decision: Decision{
				Changed:        true,
				Kept:           []uint64{1},
				InvitesAdded:   []InviteIndexAdd{{InviteeId: 11}},
				InvitesRemoved: []uint64{9, 10},
				InvitesExpired: []uint64{9},
			}}},
			wantTeam:   []uint64{1},
			wantInvite: []uint64{11, 10},
		},
		{
			name:     "孤儿自愈:只给被置空的本人 TEAM",
			healed:   []uint64{7},
			wantTeam: []uint64{7},
		},
		{
			name:   "多条提交去重、跳过 nil",
			caller: 1,
			commits: []*CommitResult{
				nil,
				{Decision: Decision{Changed: true, Kept: []uint64{1, 2}, Left: []uint64{3}}},
				{Decision: Decision{Changed: true, Kept: []uint64{1, 2}}},
			},
			healed:   []uint64{1},
			wantTeam: []uint64{1, 2, 3},
		},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			team, invite := syncDigestTargets(c.caller, c.commits, c.healed)
			require.Equal(t, normalizeIds(c.wantTeam), normalizeIds(team), "TEAM")
			require.Equal(t, normalizeIds(c.wantInvite), normalizeIds(invite), "TEAM_INVITE")
		})
	}
}

func normalizeIds(ids []uint64) []uint64 {
	out := append([]uint64{}, ids...)
	slices.Sort(out)
	return out
}

// ---- 服务层:经 publish 的真实流程 ----

// digestRecorder 是记录型 playersync.Evaler:只记摘要键与域号(ARGV[2..])。
type digestRecorder struct {
	mu    sync.Mutex
	calls []digestBump
}

type digestBump struct {
	key     string
	domains []string
}

func (r *digestRecorder) Eval(_ context.Context, _ string, keys []string, args ...string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	bump := digestBump{key: keys[0]}
	if len(args) > 1 {
		bump.domains = append([]string(nil), args[1:]...)
	}
	r.calls = append(r.calls, bump)
	return nil
}

// take 取走记录并整理成「域 → 排好序的 player_id」。键名按 player:{id}:sync_digest 自己解析,
// 不复用 playersync 的格式化(那是跨语言契约,由 shared/playersync 的测试钉住)。
func (r *digestRecorder) take(t *testing.T) map[scenepb.SyncDomain][]uint64 {
	t.Helper()
	r.mu.Lock()
	calls := r.calls
	r.calls = nil
	r.mu.Unlock()
	out := map[scenepb.SyncDomain][]uint64{}
	for _, c := range calls {
		raw, ok := strings.CutPrefix(c.key, "player:")
		require.True(t, ok, "摘要键 %q", c.key)
		raw, ok = strings.CutSuffix(raw, ":sync_digest")
		require.True(t, ok, "摘要键 %q", c.key)
		pid, err := strconv.ParseUint(raw, 10, 64)
		require.NoError(t, err, "摘要键 %q", c.key)
		for _, d := range c.domains {
			n, err := strconv.Atoi(d)
			require.NoError(t, err, "域号 %q", d)
			out[scenepb.SyncDomain(n)] = append(out[scenepb.SyncDomain(n)], pid)
		}
	}
	for d := range out {
		slices.Sort(out[d])
	}
	return out
}

func (h *serviceHarness) withDigestRecorder() *digestRecorder {
	rec := &digestRecorder{}
	h.service.syncDigest = playersync.NewBumper(rec, "team")
	return rec
}

func (h *serviceHarness) invite(leader, teamId, target uint64) {
	h.t.Helper()
	requireCode(h.t, 0, h.ok(h.server.InviteToTeam(asPlayer(leader),
		&teampb.InviteToTeamRequest{TargetPlayerId: target, ExpectedTeamId: teamId})))
}

// 一条完整的组队流程:每一步 bump 的人与域都必须恰好是「拉取结果会变的人」,且包含调用者。
func TestTeamWritesBumpSyncDigest(t *testing.T) {
	h := newServiceHarness(t, config.TeamConf{})
	h.addPlayers(testZone, 1, 2, 3, 4, 5)
	digest := h.withDigestRecorder()

	// 建队:队长本人。
	tid := h.create(1)
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {1}}, digest.take(t))

	// 申请:队员视图里的 application_count 变了;申请人是调用者,同样 bump(推送不发给他)。
	requireCode(t, 0, h.apply(2, 1))
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {1, 2}}, digest.take(t))

	// 邀请:队长视图变(待处理邀请);被邀请人的 ListMyInvites 变,但他的 GetMyTeam 不变。
	h.invite(1, tid, 3)
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {1}, domTeamInvite: {3}}, digest.take(t))

	// 接受邀请:新成员与老成员的队伍视图;新成员的收件箱里少了这条邀请。
	requireCode(t, 0, h.ok(h.server.RespondInvite(asPlayer(3), &teampb.RespondInviteRequest{TeamId: tid, Accept: true})))
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {1, 3}, domTeamInvite: {3}}, digest.take(t))

	// 按时过期:邀请 4 过期后 GetMyTeam 触发的清理提交照常落盘,但谁都不 bump(客户端按 expire_at_ms 自己算)。
	// 申请 2 也同时过期(120 秒 > 60 秒,这里推进到两者都过期)。
	h.invite(1, tid, 4)
	digest.take(t)
	setRedisMs(h.mr, baseMs()+ApplicationTTLMs)
	before := h.versionOf(tid)
	requireCode(t, 0, h.ok(h.server.GetMyTeam(asPlayer(1), &teampb.GetMyTeamRequest{})))
	require.Equal(t, before+1, h.versionOf(tid), "用例前提:过期清理确实提交了一版")
	require.Empty(t, digest.take(t), "纯过期清理不 bump")

	// 解散:全体成员(移出者拿到空视图);未过期邀请的被邀请人收到撤销,收件箱变了。
	h.invite(1, tid, 5)
	digest.take(t)
	requireCode(t, 0, h.ok(h.server.DisbandTeam(asPlayer(1), &teampb.DisbandTeamRequest{ExpectedTeamId: tid})))
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {1, 3}, domTeamInvite: {5}}, digest.take(t))
}

// 孤儿自愈把索引置 0:本人的 GetMyTeam 从「有队」变成「无队」,只给他 bump TEAM。
func TestOrphanHealBumpsSyncDigest(t *testing.T) {
	h := newServiceHarness(t, config.TeamConf{})
	h.addPlayers(testZone, 7)
	digest := h.withDigestRecorder()

	tid := h.create(7)
	digest.take(t)
	h.mr.Del(recordKey(tid))

	resp := h.ok(h.server.GetMyTeam(asPlayer(7), &teampb.GetMyTeamRequest{}))
	requireCode(t, 0, resp)
	require.Zero(t, resp.GetTeam().GetTeamId())
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {7}}, digest.take(t))
}

// 被拒的写没有提交,不 bump(§3.5 第 1 条)。
func TestRejectedTeamWriteDoesNotBump(t *testing.T) {
	h := newServiceHarness(t, config.TeamConf{})
	h.addPlayers(testZone, 1, 2)
	digest := h.withDigestRecorder()

	tid := h.create(1)
	h.join(1, tid, 2)
	digest.take(t)
	// 队员踢队长:规则拒绝(不是队长),不写。
	before := h.versionOf(tid)
	resp := h.ok(h.server.KickMember(asPlayer(2), &teampb.KickMemberRequest{TargetPlayerId: 1, ExpectedTeamId: tid}))
	requireCode(t, ErrKickNotLeader, resp)
	require.Equal(t, before, h.versionOf(tid), "用例前提:被拒的写没有提交")
	require.Empty(t, digest.take(t))
}

// 踢人与拒绝邀请:
//   - 被踢者在 Left 里,与留下的人一起 bump TEAM(新旧成员全集,被踢者的 GetMyTeam 从「有队」变成「无队」);
//   - 被邀请人拒绝邀请不是按时过期,他的 ListMyInvites 变了,bump TEAM_INVITE;
//     他作为调用者同样 bump TEAM —— 操作者一律在 bump 集合里(§3.5 第 3 条),多拉一次无害。
func TestTeamKickAndDeclineBumpSyncDigest(t *testing.T) {
	h := newServiceHarness(t, config.TeamConf{})
	h.addPlayers(testZone, 1, 2, 3)
	digest := h.withDigestRecorder()

	tid := h.create(1)
	h.join(1, tid, 2)
	digest.take(t)

	requireCode(t, 0, h.ok(h.server.KickMember(asPlayer(1), &teampb.KickMemberRequest{TargetPlayerId: 2, ExpectedTeamId: tid})))
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {1, 2}}, digest.take(t))

	h.invite(1, tid, 3)
	digest.take(t)
	requireCode(t, 0, h.ok(h.server.RespondInvite(asPlayer(3), &teampb.RespondInviteRequest{TeamId: tid, Accept: false})))
	require.Equal(t, map[scenepb.SyncDomain][]uint64{domTeam: {1, 3}, domTeamInvite: {3}}, digest.take(t))
}

// 装配形态:SharedRedis(go-zero)上真实执行 BumpScript。
func TestTeamBumpsSharedRedis(t *testing.T) {
	h := newServiceHarness(t, config.TeamConf{})
	h.addPlayers(testZone, 1, 3)
	h.service.syncDigest = playersync.NewBumper(playersync.NewGoZeroEvaler(h.svcCtx.SharedRedis), "team")

	tid := h.create(1)
	h.invite(1, tid, 3)

	leader := playersync.Key(1)
	require.Equal(t, strconv.FormatUint(baseMs(), 10), h.mr.HGet(leader, playersync.GenerationField), "g 以 Redis TIME 起种")
	require.Equal(t, "2", h.mr.HGet(leader, strconv.Itoa(int(domTeam))), "建队 + 发邀请各一次")
	require.Equal(t, "1", h.mr.HGet(playersync.Key(3), strconv.Itoa(int(domTeamInvite))))
	require.Empty(t, h.mr.HGet(playersync.Key(3), strconv.Itoa(int(domTeam))), "被邀请人的队伍视图没变")
	require.Equal(t, playersync.DefaultTTL, h.mr.TTL(leader))
}
