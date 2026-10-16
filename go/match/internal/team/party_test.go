package team

// 队伍补位匹配的组队域测试(设计文档 docs/design/team-backfill-matching.md §4.1 / §4.3 / §4.5)。
//
// 票据域用 fakeBattlePort 的补位部分(内存里的「谁持有哪支队伍的补位票」);真实实现
// logic.TeamBattleStarter 的入队 / 撤单 / 成组用例在 internal/logic/party_test.go。
// 队伍记录走真 Store + miniredis。沿用 battleHarness:asyncFn 同步、推送 / scene 桩。测试之间不并行。

import (
	"context"
	"errors"
	"testing"
	"time"

	"match/internal/constants"
	"match/internal/playercontract"

	plpb "proto/player_locator"
	teampb "proto/team"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

func (b *battleHarness) startTarget(caller uint64, target uint32) *teampb.TeamResponse {
	b.t.Helper()
	return b.ok(b.server.StartTeamMatch(asPlayer(caller), &teampb.StartTeamMatchRequest{
		BattleConfigId: testBattleConfig, ExpectedTeamId: b.teamId, TargetTeamSize: target,
	}))
}

func (b *battleHarness) myTeam(playerId uint64) *teampb.TeamView {
	b.t.Helper()
	resp := b.ok(b.server.GetMyTeam(asPlayer(playerId), &teampb.GetMyTeamRequest{}))
	requireCode(b.t, 0, resp)
	return resp.GetTeam()
}

func requireQueuedView(t *testing.T, view *teampb.TeamView, target uint32) {
	t.Helper()
	require.Equal(t, teampb.TeamMatchState_TEAM_MATCH_STATE_QUEUED, view.GetMatchState())
	require.Equal(t, target, view.GetMatchTargetSize())
	require.Equal(t, testBattleConfig, view.GetMatchBattleConfigId())
	require.Equal(t, fakePartyQueuedAtMs, view.GetMatchQueuedAtMs())
}

func requireIdleView(t *testing.T, view *teampb.TeamView) {
	t.Helper()
	require.Equal(t, teampb.TeamMatchState_TEAM_MATCH_STATE_IDLE, view.GetMatchState())
	require.Zero(t, view.GetMatchTargetSize())
	require.Zero(t, view.GetMatchQueuedAtMs())
	require.Zero(t, view.GetMatchBattleConfigId())
}

// 目标人数大于当前人数:整队进队列。不加开战锁、不建 matched 票、不跑 gather、队伍记录不写;
// 回包与推给其余队员的视图都是 QUEUED。
func TestStartTeamMatchBackfillEnqueuesParty(t *testing.T) {
	b := newBattleHarness(t, 101, 102)
	ver0 := b.versionOf(b.teamId)

	resp := b.startTarget(101, 4)

	requireCode(t, 0, resp)
	requireQueuedView(t, resp.GetTeam(), 4)
	require.Equal(t, ver0, resp.GetTeam().GetVersion(), "补位不写队伍记录,version 不变")
	require.Equal(t, []partyEnqueue{{configId: testBattleConfig, targetSize: 4, teamId: b.teamId, roster: []uint64{101, 102}}},
		b.port.enqueues, "名单:队长在前再按 join_seq")
	require.Empty(t, b.port.ticketCalls, "不走直接开战的建票")
	require.Empty(t, b.port.gathers)
	rec := loadRecord(t, b.mr, b.teamId)
	require.Empty(t, rec.GetMatchLockToken(), "补位不加开战锁")
	require.Equal(t, ver0, b.versionOf(b.teamId))

	pushes := b.takePushes()
	queued := snapshotsByReason(t, pushes, teampb.TeamChangeReason_TEAM_CHANGE_REASON_MATCH_QUEUED)
	require.ElementsMatch(t, []uint64{102}, pushedTo(queued), "调用者不收推送(回包里已有)")
	require.Equal(t, uint64(101), queued[102].GetActorId())
	requireQueuedView(t, queued[102].GetTeam(), 4)
	require.Len(t, pushes, 1)

	// 断线重连后靠 GetMyTeam 恢复「匹配中」。
	requireQueuedView(t, b.myTeam(102), 4)
	requireQueuedView(t, b.myTeam(101), 4)
}

// 目标人数规则:0 或等于当前人数 = 直接开战(原路径);小于当前人数 / 超过副本上限 = 人数不符;
// 关了开关 = 未开放(直接开战不受影响)。
func TestStartTeamMatchTargetSizeRules(t *testing.T) {
	t.Run("等于当前人数走直接开战", func(t *testing.T) {
		b := newBattleHarness(t, 101, 102, 103)
		resp := b.startTarget(101, 3)
		requireCode(t, 0, resp)
		require.Equal(t, teampb.TeamMatchState_TEAM_MATCH_STATE_STARTING, resp.GetTeam().GetMatchState())
		require.Len(t, b.port.ticketCalls, 1)
		require.Empty(t, b.port.enqueues)
	})
	t.Run("小于当前人数", func(t *testing.T) {
		b := newBattleHarness(t, 101, 102, 103)
		requireTip(t, b.startTarget(101, 2), ErrSizeExceeded, 0)
		require.Empty(t, b.port.enqueues)
		require.Empty(t, b.port.ticketCalls)
	})
	t.Run("超过副本上限", func(t *testing.T) {
		b := newBattleHarness(t, 101, 102)
		b.port.sizes[testBattleConfig] = 3
		requireTip(t, b.startTarget(101, 4), ErrSizeExceeded, 0)
		require.Empty(t, b.port.enqueues)
	})
	t.Run("非队长", func(t *testing.T) {
		b := newBattleHarness(t, 101, 102)
		requireTip(t, b.startTarget(102, 4), ErrNotLeader, 0)
		require.Empty(t, b.port.enqueues)
	})
	t.Run("开关关闭", func(t *testing.T) {
		b := newBattleHarness(t, 101, 102)
		b.svcCtx.Config.Team.FillQueueDisabled = true
		requireTip(t, b.startTarget(101, 4), ErrDungeonNotOpen, 0)
		require.Empty(t, b.port.enqueues)
		// 直接开战不受开关影响。
		requireCode(t, 0, b.startTarget(101, 0))
		require.Len(t, b.port.ticketCalls, 1)
	})
	t.Run("单人队伍也可以补位", func(t *testing.T) {
		b := newBattleHarness(t, 101)
		resp := b.startTarget(101, 3)
		requireCode(t, 0, resp)
		requireQueuedView(t, resp.GetTeam(), 3)
		require.Equal(t, []uint64{101}, b.port.enqueues[0].roster)
	})
}

// 已经在补位时再点开战(不管带不带目标人数):按「进行中」回 TeamInMatch + 当前视图,不重复入队。
func TestStartTeamMatchWhileQueuedIsInMatch(t *testing.T) {
	b := newBattleHarness(t, 101, 102)
	requireCode(t, 0, b.startTarget(101, 4))
	b.takePushes()

	for _, target := range []uint32{4, 5, 0} {
		resp := b.startTarget(101, target)
		requireTip(t, resp, ErrInMatch, 0)
		requireQueuedView(t, resp.GetTeam(), 4)
	}
	require.Len(t, b.port.enqueues, 1)
	require.Empty(t, b.port.ticketCalls)
	require.Empty(t, b.takePushes())

	// 排队态读不到:不放行第二次入队,原样回预检的拒绝原因(票据项拦下的是队长自己)。
	b.port.stateErr = errors.New("match redis down")
	requireTip(t, b.startTarget(101, 4), ErrMemberNotReady, 101)
	require.Len(t, b.port.enqueues, 1)
}

// 只有队员(不是队长)还留着本队的补位票 —— 例如队长那张已被取消路径删掉、撤到队员时出了错:
// 预检照常在该队员的票据项上拦下,回的是「该队员未准备好」,不是「进行中」。
// (真实实现里这一步会先尝试自愈半撤的补位票,见 logic.healPartyTicket;假端口不模拟自愈。)
func TestStartTeamMatchBlockedByMemberPartyTicket(t *testing.T) {
	b := newBattleHarness(t, 101, 102)
	b.port.party = map[uint64]fakePartyTicket{
		102: {teamId: b.teamId, state: constants.PartyQueueQueued, targetSize: 4, configId: testBattleConfig, queuedAtMs: fakePartyQueuedAtMs},
	}
	requireTip(t, b.startTarget(101, 4), ErrMemberNotReady, 102)
	require.Empty(t, b.port.enqueues)
}

// 入队失败:有队员已有票据 → MemberNotReady + 该成员;其它故障 → Internal。都不推送。
func TestBackfillEnqueueFailures(t *testing.T) {
	b := newBattleHarness(t, 101, 102)
	b.port.enqueueFail = 102
	requireTip(t, b.startTarget(101, 4), ErrMemberNotReady, 102)
	requireIdleView(t, b.myTeam(101))

	b.port.enqueueFail = 0
	b.port.enqueueErr = errors.New("lua timeout")
	requireTip(t, b.startTarget(101, 4), ErrInternal, 0)
	require.Empty(t, b.takePushes())
	require.Empty(t, b.port.party)

	// 预检不过(队员离线)根本不会走到入队。
	b.port.enqueueErr = nil
	b.mr.Del(playercontract.SessionKey(102))
	requireTip(t, b.startTarget(101, 4), ErrMemberOffline, 102)
	require.Len(t, b.port.enqueues, 2)
}

// 预检之后、入队期间名单变了:入队后的复核发现成员集合对不上 → 撤单,回 StateChanged,不推「开始匹配」。
func TestBackfillWithdrawnWhenRosterChangesDuringEnqueue(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	b.port.onEnqueue = func(partyEnqueue) {
		requireCode(t, 0, b.ok(b.server.LeaveTeam(asPlayer(103), &teampb.LeaveTeamRequest{ExpectedTeamId: b.teamId})))
	}

	resp := b.startTarget(101, 5)

	requireTip(t, resp, ErrStateChanged, 0)
	require.Empty(t, b.port.party, "这一单必须撤干净")
	queued := snapshotsByReason(t, b.takePushes(), teampb.TeamChangeReason_TEAM_CHANGE_REASON_MATCH_QUEUED)
	require.Empty(t, queued)
	requireIdleView(t, b.myTeam(101))
}

// 名单一变,在排的补位单整单撤出;队长转让不改成员集合,不撤。
func TestRosterChangeCancelsQueuedParty(t *testing.T) {
	cases := []struct {
		name   string
		change func(b *battleHarness)
		cancel bool
		// remaining 操作之后仍在队里的原成员(用来核对视图)。
		remaining []uint64
	}{
		{name: "队员离队", cancel: true, remaining: []uint64{101, 102}, change: func(b *battleHarness) {
			requireCode(b.t, 0, b.ok(b.server.LeaveTeam(asPlayer(103), &teampb.LeaveTeamRequest{ExpectedTeamId: b.teamId})))
		}},
		{name: "队长踢人", cancel: true, remaining: []uint64{101, 103}, change: func(b *battleHarness) {
			requireCode(b.t, 0, b.ok(b.server.KickMember(asPlayer(101),
				&teampb.KickMemberRequest{TargetPlayerId: 102, ExpectedTeamId: b.teamId})))
		}},
		{name: "新人加入", cancel: true, remaining: []uint64{101, 102, 103}, change: func(b *battleHarness) {
			b.addPlayers(testZone, 104)
			b.join(101, b.teamId, 104)
		}},
		{name: "解散", cancel: true, change: func(b *battleHarness) {
			requireCode(b.t, 0, b.ok(b.server.DisbandTeam(asPlayer(101), &teampb.DisbandTeamRequest{ExpectedTeamId: b.teamId})))
		}},
		{name: "队长转让", cancel: false, remaining: []uint64{101, 102, 103}, change: func(b *battleHarness) {
			requireCode(b.t, 0, b.ok(b.server.TransferLeader(asPlayer(101),
				&teampb.TransferLeaderRequest{TargetPlayerId: 102, ExpectedTeamId: b.teamId})))
		}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			b := newBattleHarness(t, 101, 102, 103)
			requireCode(t, 0, b.startTarget(101, 5))
			b.takePushes()

			c.change(b)

			if !c.cancel {
				for _, call := range b.port.partyCancels {
					require.Empty(t, call.canceled)
				}
				require.Len(t, b.port.party, 3, "补位票还在")
				for _, pid := range c.remaining {
					requireQueuedView(t, b.myTeam(pid), 5)
				}
				return
			}
			require.Empty(t, b.port.party, "整单撤出")
			var hit *partyCancel
			for i := range b.port.partyCancels {
				if len(b.port.partyCancels[i].canceled) > 0 {
					require.Nil(t, hit, "只应撤一次")
					hit = &b.port.partyCancels[i]
				}
			}
			require.NotNil(t, hit)
			require.Equal(t, b.teamId, hit.teamId)
			require.Equal(t, constants.PartyEndRosterChanged, hit.reason)
			require.ElementsMatch(t, []uint64{101, 102, 103}, hit.canceled)
			for _, pid := range c.remaining {
				requireIdleView(t, b.myTeam(pid))
			}
		})
	}
}

// 改了成员集合的提交,回包与推送都不装饰成「匹配中」—— 即使主动撤单失败、票据一时还在。
// 之后的普通读取如实反映票据(仍是 QUEUED),等弹组前复核 / 自愈把这一单撤掉。
func TestRosterChangeViewsAreNotDecoratedEvenIfCancelFails(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	requireCode(t, 0, b.startTarget(101, 5))
	b.takePushes()
	b.port.cancelErr = errors.New("match redis down")

	resp := b.ok(b.server.KickMember(asPlayer(101), &teampb.KickMemberRequest{TargetPlayerId: 103, ExpectedTeamId: b.teamId}))

	requireCode(t, 0, resp)
	requireIdleView(t, resp.GetTeam())
	kicked := snapshotsByReason(t, b.takePushes(), teampb.TeamChangeReason_TEAM_CHANGE_REASON_MEMBER_KICKED)
	require.Contains(t, kicked, uint64(102))
	requireIdleView(t, kicked[102].GetTeam())
	require.Len(t, b.port.party, 3, "撤单失败:票据还在")
	requireQueuedView(t, b.myTeam(102), 5)
}

// 入队之后复读成员失败:复核不了就不留单(fail-closed)。
func TestBackfillWithdrawnWhenMembersCannotBeReReadAfterEnqueue(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	b.port.onEnqueue = func(partyEnqueue) { b.mr.SetError("ERR injected") }
	t.Cleanup(func() { b.mr.SetError("") })

	resp := b.startTarget(101, 5)
	b.mr.SetError("")

	requireTip(t, resp, ErrInternal, 0)
	require.Empty(t, b.port.party, "这一单必须撤干净")
	queued := snapshotsByReason(t, b.takePushes(), teampb.TeamChangeReason_TEAM_CHANGE_REASON_MATCH_QUEUED)
	require.Empty(t, queued)
}

// 入队期间某名成员的索引改指别的队伍(索引丢失后被拉进别队,本队记录里还挂着他):与 CheckParty 同口径,撤单。
func TestBackfillWithdrawnWhenMemberIndexPointsElsewhere(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	b.port.onEnqueue = func(partyEnqueue) {
		b.mr.HSet(playerIndexKey(103), indexFieldTid, u64s(b.teamId+1))
	}

	resp := b.startTarget(101, 5)

	requireTip(t, resp, ErrStateChanged, 0)
	require.Empty(t, b.port.party, "这一单必须撤干净")
}

// CheckParty:名单里有人的索引不指向本队 → 名单变化(记录里还挂着他,但他已经不算本队的人)。
func TestCheckPartyRejectsMemberWhoseIndexPointsElsewhere(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	b.mr.HSet(playerIndexKey(103), indexFieldTid, u64s(b.teamId+1))

	reason, _, err := b.service.CheckParty(context.Background(), b.teamId, []uint64{101, 102, 103})

	require.NoError(t, err)
	require.Equal(t, constants.PartyEndRosterChanged, reason)
}

// 能力位 match_fill_supported:有队伍的视图都要标(回包、推送、名单变化的提交都一样);关了开关就是 false;
// 无队伍的空视图不标。客户端只在它为 true 时给出目标人数选项。
func TestViewsCarryFillSupportedFlag(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	require.True(t, b.myTeam(101).GetMatchFillSupported())
	require.True(t, b.myTeam(103).GetMatchFillSupported())

	// 名单变化的提交(不做匹配态装饰)同样带能力位:回包与推给其他人的视图。
	resp := b.ok(b.server.KickMember(asPlayer(101), &teampb.KickMemberRequest{TargetPlayerId: 103, ExpectedTeamId: b.teamId}))
	requireCode(t, 0, resp)
	require.True(t, resp.GetTeam().GetMatchFillSupported())
	kicked := snapshotsByReason(t, b.takePushes(), teampb.TeamChangeReason_TEAM_CHANGE_REASON_MEMBER_KICKED)
	require.True(t, kicked[102].GetTeam().GetMatchFillSupported())
	require.Zero(t, kicked[103].GetTeam().GetTeamId(), "被踢者收到的是空视图")
	require.False(t, kicked[103].GetTeam().GetMatchFillSupported(), "无队伍的空视图不标")

	// 入队成功的回包与推送。
	queued := b.startTarget(101, 4)
	requireCode(t, 0, queued)
	require.True(t, queued.GetTeam().GetMatchFillSupported())
	pushed := snapshotsByReason(t, b.takePushes(), teampb.TeamChangeReason_TEAM_CHANGE_REASON_MATCH_QUEUED)
	require.True(t, pushed[102].GetTeam().GetMatchFillSupported())

	// 关掉开关:视图如实反映,客户端不再给出选项。
	b.svcCtx.Config.Team.FillQueueDisabled = true
	require.False(t, b.myTeam(101).GetMatchFillSupported())
	requireQueuedView(t, b.myTeam(101), 4) // 已在排的单不受开关影响,照常显示
}

// 视图装饰读票的熔断:读票出错之后的一小段时间里不再读,一律按没在排下发 ——
// 一批推送逐个接收者各读一次,不熔断的话票据库一慢,与匹配无关的队伍推送也会被拖到超时。
func TestQueueStateReadBreakerSkipsFurtherReads(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	requireCode(t, 0, b.startTarget(101, 5))
	b.takePushes()
	requireQueuedView(t, b.myTeam(102), 5)

	b.port.stateErr = errors.New("match redis down")
	requireIdleView(t, b.myTeam(102)) // 这一次读失败,熔断开始
	before := b.port.stateReads()
	b.port.stateErr = nil
	requireIdleView(t, b.myTeam(102)) // 票据库已恢复,但熔断期内不读:仍按没在排
	requireIdleView(t, b.myTeam(101))
	require.Equal(t, before, b.port.stateReads(), "熔断期内不再读票")
	require.True(t, b.myTeam(101).GetMatchFillSupported(), "能力位不受熔断影响")

	// 熔断到期后恢复。
	b.service.queueStateBreakerUntil.Store(0)
	requireQueuedView(t, b.myTeam(102), 5)
}

// 熔断只认票据库自己的问题:调用方的时间预算已经用完时不读、也不熔断,之后的正常读取照常。
func TestQueueStateBreakerIgnoresCallerDeadline(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	requireCode(t, 0, b.startTarget(101, 5))
	b.takePushes()

	expired, cancel := context.WithCancel(context.Background())
	cancel()
	b.port.stateErr = context.Canceled // 即使读了也只会得到这个错误
	view := &teampb.TeamView{TeamId: b.teamId}
	before := b.port.stateReads()
	b.service.decorateQueueState(expired, 102, view)

	require.Equal(t, teampb.TeamMatchState_TEAM_MATCH_STATE_IDLE, view.GetMatchState())
	require.Equal(t, before, b.port.stateReads(), "预算已用完:不读票")
	require.Zero(t, b.service.queueStateBreakerUntil.Load(), "不算票据库故障")
	b.port.stateErr = nil
	requireQueuedView(t, b.myTeam(102), 5)
}

// 读后的判断按这一次读自己的耗时:读得不慢、只是调用方预算到期 → 不熔断;
// 读得慢(票据库慢到把调用方拖到期)→ 熔断,这正是熔断要防的情形。
func TestQueueStateBreakerJudgesTheReadItself(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	requireCode(t, 0, b.startTarget(101, 5))
	b.takePushes()

	fastCtx, cancelFast := context.WithTimeout(context.Background(), 10*time.Millisecond)
	defer cancelFast()
	b.port.stateDelay = 40 * time.Millisecond
	b.port.stateErr = context.DeadlineExceeded
	b.service.decorateQueueState(fastCtx, 102, &teampb.TeamView{TeamId: b.teamId})
	require.Zero(t, b.service.queueStateBreakerUntil.Load(), "读得不慢,只是调用方预算到期:不熔断")

	slowCtx, cancelSlow := context.WithTimeout(context.Background(), queueStateSlowThreshold/3)
	defer cancelSlow()
	b.port.stateDelay = 2 * queueStateSlowThreshold
	view := &teampb.TeamView{TeamId: b.teamId}
	b.service.decorateQueueState(slowCtx, 102, view)
	require.Equal(t, teampb.TeamMatchState_TEAM_MATCH_STATE_IDLE, view.GetMatchState())
	require.NotZero(t, b.service.queueStateBreakerUntil.Load(), "读得慢:熔断,哪怕调用方预算因此到期")

	b.port.stateDelay = 0
	b.port.stateErr = nil
	b.service.queueStateBreakerUntil.Store(0)
	requireQueuedView(t, b.myTeam(102), 5)
}

// 名单变化的撤单读不到票据库时,不影响这次提交自己的推送。
func TestRosterChangePushesSurviveCancelFailure(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	requireCode(t, 0, b.startTarget(101, 5))
	b.takePushes()
	b.port.cancelErr = errors.New("match redis down")

	requireCode(t, 0, b.ok(b.server.LeaveTeam(asPlayer(103), &teampb.LeaveTeamRequest{ExpectedTeamId: b.teamId})))

	left := snapshotsByReason(t, b.takePushes(), teampb.TeamChangeReason_TEAM_CHANGE_REASON_MEMBER_LEFT)
	require.Contains(t, left, uint64(101))
	require.Contains(t, left, uint64(102))
	require.Len(t, left[101].GetTeam().GetMembers(), 2)
	require.Len(t, b.port.party, 3, "撤单失败:票据还在,等弹组前复核 / 自愈收尾")
}

// 视图里的匹配态按接收者自己的票据现算。
func TestViewQueueStateFollowsViewerTicket(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	requireIdleView(t, b.myTeam(101))

	// 只有 101、102 持有本队的补位票(103 没有):各看各的。
	b.port.party = map[uint64]fakePartyTicket{
		101: {teamId: b.teamId, state: constants.PartyQueueQueued, targetSize: 5, configId: testBattleConfig, queuedAtMs: fakePartyQueuedAtMs},
		102: {teamId: b.teamId, state: constants.PartyQueueMatched},
		// 104 的票属于别的队伍,与本队无关。
		103: {teamId: b.teamId + 1, state: constants.PartyQueueQueued, targetSize: 4, configId: testBattleConfig, queuedAtMs: fakePartyQueuedAtMs},
	}
	requireQueuedView(t, b.myTeam(101), 5)
	matched := b.myTeam(102)
	require.Equal(t, teampb.TeamMatchState_TEAM_MATCH_STATE_STARTING, matched.GetMatchState(), "已凑满弹出:集合中")
	require.Zero(t, matched.GetMatchTargetSize())
	requireIdleView(t, b.myTeam(103))

	// 读票失败:视图照常下发,按没在排处理。
	b.port.stateErr = errors.New("match redis down")
	requireIdleView(t, b.myTeam(101))
}

// CheckParty:弹组前 / 自愈时的复核。
func TestCheckParty(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	ctx := context.Background()
	check := func(members ...uint64) (uint32, uint64) {
		t.Helper()
		reason, culprit, err := b.service.CheckParty(ctx, b.teamId, members)
		require.NoError(t, err)
		return reason, culprit
	}

	reason, culprit := check(101, 102, 103)
	require.Equal(t, constants.PartyEndNone, reason)
	require.Zero(t, culprit)
	reason, _ = check(103, 101, 102)
	require.Equal(t, constants.PartyEndNone, reason, "集合语义,顺序无关")

	reason, _ = check(101, 102)
	require.Equal(t, constants.PartyEndRosterChanged, reason, "队里多了人")
	reason, _ = check(101, 102, 103, 104)
	require.Equal(t, constants.PartyEndRosterChanged, reason, "名单里有人已不在队")
	reason, _, err := b.service.CheckParty(ctx, b.teamId+999, []uint64{101, 102, 103})
	require.NoError(t, err)
	require.Equal(t, constants.PartyEndRosterChanged, reason, "队伍不存在")

	setSession := func(pid uint64, state plpb.PlayerSessionState) {
		t.Helper()
		raw, err := proto.Marshal(&plpb.PlayerSession{PlayerId: pid, State: state})
		require.NoError(t, err)
		require.NoError(t, b.mr.Set(playercontract.SessionKey(pid), string(raw)))
	}

	// 断线等重连的宽限期:放行(单人排队本来就不看会话;没连回来的人会在开局时备战失败)。
	setSession(102, plpb.PlayerSessionState_SESSION_STATE_DISCONNECTING)
	reason, _ = check(101, 102, 103)
	require.Equal(t, constants.PartyEndNone, reason)

	// 明确下线 / 会话已不存在:掉线,带出是谁。
	setSession(102, plpb.PlayerSessionState_SESSION_STATE_OFFLINE)
	reason, culprit = check(101, 102, 103)
	require.Equal(t, constants.PartyEndMemberOffline, reason)
	require.Equal(t, uint64(102), culprit)
	b.mr.Del(playercontract.SessionKey(102))
	reason, culprit = check(101, 102, 103)
	require.Equal(t, constants.PartyEndMemberOffline, reason)
	require.Equal(t, uint64(102), culprit)

	// 会话读不出来(解析失败):不做失效判定,返回 err 让调用方把这一单本轮搁置。
	require.NoError(t, b.mr.Set(playercontract.SessionKey(102), "not-a-session"))
	_, _, err = b.service.CheckParty(ctx, b.teamId, []uint64{101, 102, 103})
	require.Error(t, err)
}

// 名单变化的撤单带着「提交后的成员集合」:它是提交之后才异步执行的,不能误杀队长已按新名单重新入队的单。
func TestRosterChangeCancelPassesCommittedRoster(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	requireCode(t, 0, b.startTarget(101, 5))
	b.takePushes()
	b.port.partyCancels = nil // 建队时两次入队提交也会各问一次(没有在排的单),不算在本用例里

	requireCode(t, 0, b.ok(b.server.LeaveTeam(asPlayer(103), &teampb.LeaveTeamRequest{ExpectedTeamId: b.teamId})))

	require.Len(t, b.port.partyCancels, 1)
	call := b.port.partyCancels[0]
	require.ElementsMatch(t, []uint64{101, 102, 103}, call.candidates, "候选是提交前就在队里的人")
	require.ElementsMatch(t, []uint64{101, 102}, call.keep, "提交后的成员集合")
	require.ElementsMatch(t, []uint64{101, 102, 103}, call.canceled, "旧名单的单被撤掉")

	// (「名单等于提交后集合的单不撤」这条判定本身在票据域,用例是 logic.TestCancelPartyByTeam;这里只核对 team 传出的参数。)
	requireCode(t, 0, b.startTarget(101, 5))
	require.Len(t, b.port.party, 2)

	// 解散:提交后没有成员,无条件撤。
	requireCode(t, 0, b.ok(b.server.DisbandTeam(asPlayer(101), &teampb.DisbandTeamRequest{ExpectedTeamId: b.teamId})))
	require.Empty(t, b.port.party)
	last := b.port.partyCancels[len(b.port.partyCancels)-1]
	require.Empty(t, last.keep)
}

// PartyEnded:只推给这一单里仍在本队的人;主动取消带取消者、其余原因带 tip。
func TestPartyEndedPushesToPartyMembersStillInTeam(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	canceledReason := teampb.TeamChangeReason_TEAM_CHANGE_REASON_MATCH_QUEUE_CANCELED

	// 这一单是 [101, 102](103 是入队之后才加入的人,不在单里)。
	b.service.PartyEnded(b.teamId, []uint64{101, 102}, constants.PartyEndCanceled, 102)
	pushes := snapshotsByReason(t, b.takePushes(), canceledReason)
	require.ElementsMatch(t, []uint64{101, 102}, pushedTo(pushes), "不在这一单里的队员不收")
	for _, snap := range pushes {
		require.Equal(t, uint64(102), snap.GetActorId())
		require.Nil(t, snap.GetTip())
		requireIdleView(t, snap.GetTeam())
		require.Equal(t, b.teamId, snap.GetTeam().GetTeamId())
	}

	b.service.PartyEnded(b.teamId, []uint64{101, 102}, constants.PartyEndMemberOffline, 102)
	pushes = snapshotsByReason(t, b.takePushes(), canceledReason)
	require.Zero(t, pushes[101].GetActorId())
	require.Equal(t, ErrMemberOffline, pushes[101].GetTip().GetId())
	require.Equal(t, []string{"102"}, pushes[101].GetTip().GetParameters())

	b.service.PartyEnded(b.teamId, []uint64{101, 102}, constants.PartyEndMemberNotReady, 0)
	pushes = snapshotsByReason(t, b.takePushes(), canceledReason)
	require.Equal(t, ErrMemberNotReady, pushes[101].GetTip().GetId())
	require.Empty(t, pushes[101].GetTip().GetParameters())

	b.service.PartyEnded(b.teamId, []uint64{101, 102}, constants.PartyEndRosterChanged, 0)
	pushes = snapshotsByReason(t, b.takePushes(), canceledReason)
	require.Equal(t, ErrStateChanged, pushes[102].GetTip().GetId())

	// 单里的人已经离队:不给他推本队视图(他离队时已收到空视图)。
	requireCode(t, 0, b.ok(b.server.LeaveTeam(asPlayer(102), &teampb.LeaveTeamRequest{ExpectedTeamId: b.teamId})))
	b.takePushes()
	b.service.PartyEnded(b.teamId, []uint64{101, 102}, constants.PartyEndRosterChanged, 0)
	pushes = snapshotsByReason(t, b.takePushes(), canceledReason)
	require.ElementsMatch(t, []uint64{101}, pushedTo(pushes))

	// 队伍已不存在:不推。
	b.service.PartyEnded(b.teamId+999, []uint64{101}, constants.PartyEndRosterChanged, 0)
	require.Empty(t, b.takePushes())
}

// PartyStarted:补位凑满开局成功后,给这一单里仍在本队的人推一次当前视图(MATCH_ENDED),面板从「匹配中」归位。
func TestPartyStartedPushesMatchEndedToPartyMembers(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)

	b.service.PartyStarted(b.teamId, []uint64{101, 102})

	pushes := snapshotsByReason(t, b.takePushes(), teampb.TeamChangeReason_TEAM_CHANGE_REASON_MATCH_ENDED)
	require.ElementsMatch(t, []uint64{101, 102}, pushedTo(pushes), "不在这一单里的队员不收")
	for _, snap := range pushes {
		requireIdleView(t, snap.GetTeam())
		require.Nil(t, snap.GetTip())
	}

	// 队伍已不存在:不推。
	b.service.PartyStarted(b.teamId+999, []uint64{101})
	require.Empty(t, b.takePushes())
}
