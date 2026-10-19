package logic

import (
	"context"
	"testing"

	matchpb "proto/match"

	"github.com/stretchr/testify/require"
)

// 客户端透明重试会重放完全相同的 Lua 参数,每次实际执行都必须刷新版本。
// 从 2^53 边界开始验证:用 INCR 的 Lua 数字返回值再 tostring 会把相邻版本舍入成相同值。
func TestQueuePushReplayRefreshesVersionWithoutLosingPrecision(t *testing.T) {
	for _, tc := range []struct {
		name    string
		script  string
		front   bool
		members []uint64
	}{
		{"单人入队", enqueueScript, false, []uint64{33001}},
		{"单人回队", requeueScript, true, []uint64{33001}},
		{"整队入队", pushMembersScript, false, []uint64{33001, 33002}},
		{"整队回队", pushMembersScript, true, []uint64{33001, 33002}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			svcCtx, mr := newTestSvcCtx(t)
			queueKey := matchSizedQueueKey(pveTeamMode, partyTestConfig, 5)
			rankKey, err := rankKeyForQueue(queueKey)
			require.NoError(t, err)
			require.NoError(t, enqueueAtomic(svcCtx, queueKey, 33000, defaultRating))
			require.NoError(t, mr.Set(matchQueueVersionSequenceKey, "9007199254740991"))
			keys := []string{matchQueueIndexKey, queueKey, rankKey, queueVersionsKey(queueKey), matchQueueVersionSequenceKey}
			args := []any{queueKey}
			if tc.script == pushMembersScript {
				flag := "0"
				if tc.front {
					flag = "1"
				}
				args = append(args, flag)
			}
			for _, pid := range tc.members {
				args = append(args, idString(pid), formatRating(defaultRating))
			}

			_, err = svcCtx.MatchRedis.Eval(tc.script, keys, args...)
			require.NoError(t, err)
			before, err := loadQueueMemberVersions(svcCtx, queueKey, tc.members)
			require.NoError(t, err)
			for _, pid := range tc.members {
				require.Equal(t, "9007199254740992", before[pid])
			}
			// 不经 Go 封装重新造参数,直接重放第一次提交的同一条命令。
			_, err = svcCtx.MatchRedis.Eval(tc.script, keys, args...)
			require.NoError(t, err)
			after, err := loadQueueMemberVersions(svcCtx, queueKey, tc.members)
			require.NoError(t, err)
			for _, pid := range tc.members {
				require.Equal(t, "9007199254740993", after[pid])
			}

			removed, err := removeQueueMembersVersionedCtx(context.Background(), svcCtx, queueKey, tc.members, before)
			require.NoError(t, err)
			require.Empty(t, removed, "旧版本不能删掉重试刷新后的队列项")
			want := append([]uint64{33000}, tc.members...)
			if tc.front {
				want = append(append([]uint64(nil), tc.members...), 33000)
			}
			require.Equal(t, idStrings(want...), queueMembers(t, svcCtx, queueKey), "重放后顺序不变,每个成员严格只有一份")
			assertQueueMirrorConsistent(t, mr, queueKey)

			removed, err = removeQueueMembersVersionedCtx(context.Background(), svcCtx, queueKey, tc.members, after)
			require.NoError(t, err)
			require.Equal(t, tc.members, removed, "当前版本仍可正常清理")
			for _, pid := range tc.members {
				require.Empty(t, mr.HGet(queueVersionsKey(queueKey), idString(pid)))
			}
			require.Equal(t, idStrings(33000), queueMembers(t, svcCtx, queueKey))
			assertQueueMirrorConsistent(t, mr, queueKey)
		})
	}
}

// 清空一条队列可回收版本 hash,但全局序列必须存活,否则重排后的版本可能与旧取消快照相同。
func TestQueuePrunePreservesVersionSequence(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	queueKey := matchSizedQueueKey(pveTeamMode, partyTestConfig, 5)
	members := []uint64{33011}
	require.NoError(t, enqueueAtomic(svcCtx, queueKey, members[0], defaultRating))
	before, err := loadQueueMemberVersions(svcCtx, queueKey, members)
	require.NoError(t, err)
	removed, err := removeQueueMembers(svcCtx, queueKey, members)
	require.NoError(t, err)
	require.Equal(t, members, removed)
	require.False(t, mr.Exists(queueVersionsKey(queueKey)), "普通摘项要同步删除版本字段")
	// 模拟空 list 留下的异常镜像,prune 应清掉整张 hash。
	mr.HSet(queueVersionsKey(queueKey), "孤儿字段", "1")
	pruneEmptyQueue(svcCtx, queueKey, "PVE_TEAM", "1")
	require.False(t, mr.Exists(queueVersionsKey(queueKey)))
	sequence, err := mr.Get(matchQueueVersionSequenceKey)
	require.NoError(t, err)
	require.Equal(t, before[members[0]], sequence)

	require.NoError(t, enqueueAtomic(svcCtx, queueKey, members[0], defaultRating))
	after, err := loadQueueMemberVersions(svcCtx, queueKey, members)
	require.NoError(t, err)
	require.NotEqual(t, before, after)
	removed, err = removeQueueMembersVersionedCtx(context.Background(), svcCtx, queueKey, members, before)
	require.NoError(t, err)
	require.Empty(t, removed)
	require.Equal(t, idStrings(members...), queueMembers(t, svcCtx, queueKey))
	inIndex, err := mr.IsMember(matchQueueIndexKey, queueKey)
	require.NoError(t, err)
	require.True(t, inIndex, "prune 后重排仍须恢复注册集")
	assertQueueMirrorConsistent(t, mr, queueKey)
}

// 旧队列迁移可带入非数字成员,matcher 的原始值清理分支也不能漏掉版本字段。
func TestMatcherDropsInvalidRawQueueVersion(t *testing.T) {
	svcCtx, mr := newTestSvcCtx(t)
	queueKey := matchQueueKey(int32(matchpb.MatchMode_MATCH_MODE_1V1), 0)
	for _, pid := range []uint64{33021, 33022} {
		setPlayerLocation(t, mr, pid, 1)
		require.Zero(t, joinQueue1v1(t, svcCtx, pid).ErrorCode)
	}
	require.NoError(t, pushFrontRegistered(svcCtx, queueKey, "invalid-player", defaultRating))
	require.NotEmpty(t, mr.HGet(queueVersionsKey(queueKey), "invalid-player"))

	members, _, ok := popGroup(svcCtx, queueKey, 2)
	require.True(t, ok)
	require.Equal(t, []uint64{33021, 33022}, members)
	require.False(t, mr.Exists(queueVersionsKey(queueKey)), "非法原始值和正常弹组成员都必须清掉版本")
	assertQueueMirrorConsistent(t, mr, queueKey)
}
