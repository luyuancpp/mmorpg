package logic

import (
	"context"
	"fmt"
	"testing"

	"match/internal/constants"

	matchpb "proto/match"

	"github.com/stretchr/testify/require"
)

// 队员甲撤整单时，队员乙的独立会话可在旧票删除后重新单排。
// 旧撤单最后摘 list 时必须保留乙刚成功入队的新票对应的项。
// 复用现有 miniredis 顶层 EVAL 钩子，确定性插入该跨玩家时序。
func TestCancelPartyPreservesMemberRequeuedBeforeOldDequeue(t *testing.T) {
	testCancelPartyPreservesMemberRequeuedBeforeOldDequeue(t, false)
}

func TestCancelPartyLegacyQueuePreservesRequeuedMember(t *testing.T) {
	testCancelPartyPreservesMemberRequeuedBeforeOldDequeue(t, true)
}

func testCancelPartyPreservesMemberRequeuedBeforeOldDequeue(t *testing.T, legacy bool) {
	t.Helper()
	svcCtx, mr, _ := newPartySvcCtx(t)
	const captain = uint64(32901)
	const member = uint64(32902)
	const third = uint64(32903)
	const teamID = uint64(9399)
	queueKey := matchSizedQueueKey(pveTeamMode, partyTestConfig, 5)
	for _, pid := range []uint64{captain, member, third} {
		setPlayerLocation(t, mr, pid, 1)
	}
	enqueueParty(t, svcCtx, teamID, 5, captain, member, third)
	if legacy {
		// 只兼容停止写入的旧实例留下的无版本数据。
		mr.Del(queueVersionsKey(queueKey))
	}

	type joinResult struct {
		response *matchpb.JoinQueueResponse
		err      error
	}
	joined := make(chan joinResult, 1)
	hook, raced := onceBefore(evalOf(removeQueueMembersScript, queueKey), func() {
		// 此时 cancelParty 已删旧票，尚未执行旧队列的批量摘除。
		// 乙的请求与发起撤单的甲不共享会话单飞锁。
		response, err := NewJoinQueueLogic(context.Background(), svcCtx).JoinQueue(&matchpb.JoinQueueRequest{
			PlayerId:       member,
			Mode:           matchpb.MatchMode_MATCH_MODE_PVE_TEAM,
			BattleConfigId: partyTestConfig,
			TeamSize:       5,
		})
		joined <- joinResult{response: response, err: err}
	})
	installRedisHooks(t, mr, hook)

	_, cancelErr := NewCancelQueueLogic(context.Background(), svcCtx).CancelQueue(&matchpb.CancelQueueRequest{PlayerId: captain})
	require.NoError(t, cancelErr)
	require.True(t, raced(), "必须命中旧撤单的最终批量摘队列步骤")
	result := <-joined
	require.NoError(t, result.err)
	require.NotNil(t, result.response)
	require.Zero(t, result.response.ErrorCode, "乙的新单应成功入队")
	require.NotEmpty(t, result.response.QueueTicket)

	current, err := loadTicket(svcCtx, member)
	require.NoError(t, err)
	require.NotNil(t, current)
	require.Equal(t, result.response.QueueTicket, current.Ticket)
	require.Equal(t, ticketStateQueued, current.State)
	require.Empty(t, current.PartyId, "新票是乙自己的单排")
	require.Equal(t, idStrings(member), queueMembers(t, svcCtx, queueKey), "旧整队撤单不得摘掉乙刚成功重排的新项")
	assertQueueMirrorConsistent(t, mr, queueKey)
}

// 成员旧票删除后、撤单尚在读取下一位成员时,他已经与新队友成功重新整队入队。
// 回队分支再走真实补偿入口,确认旧撤单也不能摘掉回队时刷新的项。
func TestCancelPartyPreservesNewPartyBeforeOldDequeue(t *testing.T) {
	for _, front := range []bool{false, true} {
		name := "新整队入队"
		if front {
			name = "新整队回队首"
		}
		t.Run(name, func(t *testing.T) {
			svcCtx, mr, _ := newPartySvcCtx(t)
			const captain, member, third, newcomer = uint64(32911), uint64(32912), uint64(32913), uint64(32914)
			const oldTeam, newTeam = uint64(9400), uint64(9401)
			queueKey := matchSizedQueueKey(pveTeamMode, partyTestConfig, 5)
			for _, pid := range []uint64{captain, member, third, newcomer} {
				setPlayerLocation(t, mr, pid, 1)
			}
			enqueueParty(t, svcCtx, oldTeam, 5, captain, member, third)
			roster := []uint64{member, newcomer}
			type joinResult struct {
				failed        uint64
				err           error
				before, after map[uint64]string
			}
			joined := make(chan joinResult, 1)
			hook, raced := onceBefore(func(cmd string, args []string) bool {
				return cmd == "HGETALL" && len(args) == 1 && args[0] == matchTicketKey(third)
			}, func() {
				var result joinResult
				defer func() { joined <- result }()
				result.failed, result.err = NewTeamBattleStarter(svcCtx).EnqueueParty(
					context.Background(), partyTestConfig, 5, newTeam, roster, nil)
				if result.err != nil || result.failed != 0 {
					return
				}
				result.before, result.err = loadQueueMemberVersions(svcCtx, queueKey, roster)
				if result.err != nil || !front {
					return
				}
				// 模拟新单被弹出后开局失败,用真实回队入口刷新版本。
				if _, result.err = removeQueueMembers(svcCtx, queueKey, roster); result.err != nil {
					return
				}
				tickets := make(map[uint64]string, len(roster))
				for _, pid := range roster {
					ticket, err := loadTicket(svcCtx, pid)
					if err != nil {
						result.err = err
						return
					}
					if ticket == nil {
						result.err = fmt.Errorf("新单成员 %d 缺少票据", pid)
						return
					}
					tickets[pid] = ticket.Ticket
					written, err := setTicketMatched(svcCtx, pid, ticket.Ticket, matchedTicketTTLFor(svcCtx, 5))
					if err != nil {
						result.err = err
						return
					}
					if !written {
						result.err = fmt.Errorf("新单成员 %d 未推进 matched", pid)
						return
					}
				}
				requeueFront(svcCtx, queueKey, roster, tickets)
				result.after, result.err = loadQueueMemberVersions(svcCtx, queueKey, roster)
			})
			installRedisHooks(t, mr, hook)

			cancelQueueAs(t, svcCtx, captain)
			require.True(t, raced(), "必须在旧单尚未读完成员票据时插入新整队")
			result := <-joined
			require.NoError(t, result.err)
			require.Zero(t, result.failed)
			requireNoTicket(t, svcCtx, captain, third)
			for _, pid := range roster {
				current, err := loadTicket(svcCtx, pid)
				require.NoError(t, err)
				require.NotNil(t, current)
				require.Equal(t, newTeam, current.TeamId)
				require.Equal(t, ticketStateQueued, current.State)
				require.Equal(t, roster, current.PartyMembers)
				require.NotEmpty(t, result.before[pid])
				wantVersion := result.before[pid]
				if front {
					require.NotEmpty(t, result.after[pid])
					require.NotEqual(t, result.before[pid], result.after[pid], "回队须生成另一版本")
					wantVersion = result.after[pid]
				}
				require.Equal(t, wantVersion, mr.HGet(queueVersionsKey(queueKey), idString(pid)))
			}
			require.Equal(t, idStrings(roster...), queueMembers(t, svcCtx, queueKey), "新单成员必须按原序各保留一份")
			assertQueueMirrorConsistent(t, mr, queueKey)
		})
	}
}

// 正常旧单与静态无版本遗留单都必须真正清理;版本保护不能把所有取消变成无操作。
func TestCancelPartyCleansUnchangedQueueVersions(t *testing.T) {
	for _, legacy := range []bool{false, true} {
		name := "带版本旧单"
		if legacy {
			name = "无版本遗留单"
		}
		t.Run(name, func(t *testing.T) {
			svcCtx, mr, port := newPartySvcCtx(t)
			const captain, member = uint64(32921), uint64(32922)
			queueKey := matchSizedQueueKey(pveTeamMode, partyTestConfig, 5)
			enqueueParty(t, svcCtx, 9402, 5, captain, member)
			if legacy {
				mr.Del(queueVersionsKey(queueKey))
			}
			cancelQueueAs(t, svcCtx, captain)
			requireNoTicket(t, svcCtx, captain, member)
			require.Empty(t, queueMembers(t, svcCtx, queueKey))
			require.False(t, mr.Exists(queueVersionsKey(queueKey)))
			require.True(t, mr.Exists(matchQueueVersionSequenceKey), "取消不能回收全局序列")
			assertQueueMirrorConsistent(t, mr, queueKey)
			require.Len(t, port.endedCalls(), 1)
		})
	}
}

// 版本快照失败时可继续按 ticket id 撤旧票,但绝不能降级成无条件摘队列。
func TestCancelPartyVersionSnapshotFailureDoesNotDequeue(t *testing.T) {
	svcCtx, mr, _ := newPartySvcCtx(t)
	const captain, member = uint64(32931), uint64(32932)
	queueKey := matchSizedQueueKey(pveTeamMode, partyTestConfig, 5)
	members := []uint64{captain, member}
	enqueueParty(t, svcCtx, 9403, 5, members...)
	seed, err := loadTicket(svcCtx, captain)
	require.NoError(t, err)
	before, err := loadQueueMemberVersions(svcCtx, queueKey, members)
	require.NoError(t, err)
	snapshotHook, snapshotted := onceBefore(evalOf(queueMemberVersionsScript, ""), func() {})
	removeHook, removed := onceBefore(evalOf(removeQueueMembersScript, queueKey), func() {})
	installRedisHooks(t, mr, snapshotHook, failWhen(evalOf(queueMemberVersionsScript, "")), removeHook)

	cancelParty(svcCtx, seed, constants.PartyEndCanceled, captain)

	require.True(t, snapshotted(), "必须实际触发版本快照失败")
	require.False(t, removed(), "快照失败时不得发出任何无保护摘队列")
	requireNoTicket(t, svcCtx, members...)
	require.Equal(t, idStrings(members...), queueMembers(t, svcCtx, queueKey), "保留残留项供 matcher 校验收尾")
	for _, pid := range members {
		require.Equal(t, before[pid], mr.HGet(queueVersionsKey(queueKey), idString(pid)))
	}
	assertQueueMirrorConsistent(t, mr, queueKey)
}
