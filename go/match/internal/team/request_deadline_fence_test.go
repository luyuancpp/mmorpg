package team

// 请求截止之后不再提交(docs/design/client-request-ordering.md §6.1 / O11 / §14 第 4 条)的回归测试。
//
// gate 对同一会话的路由类请求单飞:路由服回超时,gate 就放行下一条。team 的 handler 若在截止之后仍发出
// S_COMMIT / 开战锁,这条写就会晚于下一条请求落地(§1.3 的 CancelQueue → StartTeamMatch、RespondInvite → LeaveTeam)。
//
// Mutate 的「读完之后截止 → 不提交」已由 service_test.go 的 TestExpiredContextSkipsCommit(#25)钉住。
// 本文件补两处它够不到的地方:
//   - 开战锁(CommitMatchLock)不走 Mutate,有自己的提交前 ctx 检查;
//   - 进入 handler 之前就已截止时,全部写 RPC 一个键都不写(读、自愈、续期、建票都不发生)。
//
// 判据来自 go-redis v9.17.3:取连接前先检查 ctx(internal/pool/pool.go 的 waitTurn),ctx 已结束时命令不会发出;
// 所以这里能抓住的回归是「请求路径上某一处写改用了 context.Background() / WithoutCancel」或「去掉了提交前的 ctx 检查」。

import (
	"context"
	"testing"

	teampb "proto/team"

	"github.com/stretchr/testify/require"
)

// 预检通过、开战锁提交之前请求截止:CommitMatchLock 必须放弃提交(ErrStateChanged),不得加锁、不得建票、不得推送。
// 若这一步放行,锁会在 gate 放行下一条之后才落地,全队随后的名单操作全部回 TeamInMatch,直到锁自然过期。
func TestStartTeamMatchExpiredContextSkipsLock(t *testing.T) {
	b := newBattleHarness(t, 101, 102, 103)
	ver0 := b.versionOf(b.teamId)

	ctx, cancel := context.WithCancel(asPlayer(101))
	defer cancel()
	var beforeLock string
	fired := false
	afterPreflightHook = func(teamId uint64, _ []uint64) {
		if fired || teamId != b.teamId {
			return
		}
		fired = true
		// 请求预算在预检之后耗尽:go-zero 超时拦截器已回错,handler 仍在跑(§D.6 迟到执行)。
		cancel()
		beforeLock = b.mr.Dump()
	}
	resp := b.ok(b.server.StartTeamMatch(ctx,
		&teampb.StartTeamMatchRequest{BattleConfigId: testBattleConfig, ExpectedTeamId: b.teamId}))
	afterPreflightHook = nil

	require.True(t, fired, "用例必须真正走到加锁之前")
	requireCode(t, ErrStateChanged, resp)
	require.Equal(t, beforeLock, b.mr.Dump(), "截止之后不得发出开战锁提交")
	require.Equal(t, ver0, b.versionOf(b.teamId))
	require.Empty(t, loadRecord(t, b.mr, b.teamId).GetMatchLockToken())
	require.Empty(t, b.port.ticketCalls, "没加锁就不得建票")
	require.Empty(t, b.port.gathers)
	require.Empty(t, b.takePushes())
	b.requireSceneRefresh()
}

// 进入 handler 之前就已截止:每个写 RPC 都必须回非 0 码,且 Redis 里一个字节都不变、不推送、不发 scene 信号、不建票。
func TestWriteRPCsAfterRequestDeadlineWriteNothing(t *testing.T) {
	// 101 队长、102 队员;103 在线无队(申请 / 被邀请 / 建队的一方)。
	const leader, member, outsider uint64 = 101, 102, 103
	cases := []struct {
		name   string
		caller uint64
		call   func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error)
	}{
		{"CreateTeam", outsider, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.CreateTeam(ctx, &teampb.CreateTeamRequest{})
		}},
		{"GetMyTeam", leader, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.GetMyTeam(ctx, &teampb.GetMyTeamRequest{NotifyOnline: true})
		}},
		{"ApplyJoinTeam", outsider, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.ApplyJoinTeam(ctx, &teampb.ApplyJoinTeamRequest{TargetPlayerId: leader})
		}},
		{"HandleApplication", leader, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.HandleApplication(ctx,
				&teampb.HandleApplicationRequest{ApplicantId: outsider, Approve: true, ExpectedTeamId: b.teamId})
		}},
		{"InviteToTeam", leader, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.InviteToTeam(ctx, &teampb.InviteToTeamRequest{TargetPlayerId: outsider, ExpectedTeamId: b.teamId})
		}},
		{"RespondInvite", outsider, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.RespondInvite(ctx, &teampb.RespondInviteRequest{TeamId: b.teamId, Accept: true})
		}},
		{"LeaveTeam", member, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.LeaveTeam(ctx, &teampb.LeaveTeamRequest{ExpectedTeamId: b.teamId})
		}},
		{"KickMember", leader, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.KickMember(ctx, &teampb.KickMemberRequest{TargetPlayerId: member, ExpectedTeamId: b.teamId})
		}},
		{"TransferLeader", leader, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.TransferLeader(ctx, &teampb.TransferLeaderRequest{TargetPlayerId: member, ExpectedTeamId: b.teamId})
		}},
		{"DisbandTeam", leader, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.DisbandTeam(ctx, &teampb.DisbandTeamRequest{ExpectedTeamId: b.teamId})
		}},
		{"StartTeamMatch", leader, func(b *battleHarness, ctx context.Context) (*teampb.TeamResponse, error) {
			return b.server.StartTeamMatch(ctx,
				&teampb.StartTeamMatchRequest{BattleConfigId: testBattleConfig, ExpectedTeamId: b.teamId})
		}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			b := newBattleHarness(t, leader, member)
			b.addPlayers(testZone, outsider)
			before := b.mr.Dump()
			ctx, cancel := context.WithCancel(asPlayer(tc.caller))
			cancel() // 已经截止的请求 ctx(go-zero 超时拦截器到期后 handler 拿到的就是这种 ctx)

			resp := b.ok(tc.call(b, ctx))

			require.NotZero(t, resp.GetErrorMessage().GetId(), "截止之后不得回成功 resp=%v", resp)
			require.Equal(t, before, b.mr.Dump(), "截止之后不得写 Redis")
			require.Empty(t, b.port.ticketCalls)
			require.Empty(t, b.port.gathers)
			require.Empty(t, b.takePushes())
			b.requireSceneRefresh()
		})
	}
}
