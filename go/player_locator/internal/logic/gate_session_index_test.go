package logic

import (
	"context"
	"fmt"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"player_locator/internal/svc"
	pb "proto/player_locator"
)

// 写路径行为测试:只经公开的 Logic RPC 驱动,断言 gate 会话索引(player:gate_index:*)随会话写入原子维护,
// 即不变量 I1「会话 ONLINE 且 gate_instance_id 非空 ⇔ player_id 是该 gate 索引的成员」。
// 修复前(会话 Lua 不维护索引)这些断言因索引键不存在而失败。
//
// 本文件只用修复前就有的符号(Logic RPC、newTestSvcCtx、readSession 等),索引键名写字面量、不引用实现里的常量:
// 单独拷到修复前的基线(360d56ae5)上也能编译,TestGateIndex_* 拿到的是断言红,而不只是「新函数不存在」的编译红。
// 字面量同时钉住键形这条跨版本契约(清库脚本、运维命令也按字面量写)。确定性序列测试要驱动对账,放在
// session_reconciler_test.go。

const (
	idxGateA = "inst-gate-a"
	idxGateB = "inst-gate-b"
	idxGateC = "inst-gate-c"

	testGateIndexSessionsPrefix = "player:gate_index:sessions:"
	testGateIndexRegistryKey    = "player:gate_index:gates"
)

// indexTestSession 构造一条 ONLINE 会话。SceneId 为 0 且不种场景位置,MarkOffline 的 LeaveScene
// 快路径因此直接返回(没有权威位置就不需要 SceneManager 客户端),测试只看会话与索引。
func indexTestSession(pid uint64, sessionID uint32, gate string, version uint32) *pb.PlayerSession {
	return &pb.PlayerSession{
		PlayerId:       pid,
		SessionId:      sessionID,
		GateId:         "1",
		GateInstanceId: gate,
		SessionVersion: version,
		State:          pb.PlayerSessionState_SESSION_STATE_ONLINE,
		Account:        "acc",
		RequestId:      fmt.Sprintf("req-%d-%d-%d", pid, sessionID, version),
	}
}

func callSetSession(sc *svc.ServiceContext, s *pb.PlayerSession) error {
	_, err := NewSetSessionLogic(context.Background(), sc).SetSession(&pb.SetSessionRequest{Session: s})
	return err
}

// indexMembers 返回 gate 索引的成员(排序后);索引键不存在时为 nil。
func indexMembers(t *testing.T, mr *miniredis.Miniredis, gate string) []string {
	t.Helper()
	key := testGateIndexSessionsPrefix + gate
	if !mr.Exists(key) {
		return nil
	}
	members, err := mr.Members(key)
	require.NoError(t, err)
	return members
}

func registryMembers(t *testing.T, mr *miniredis.Miniredis) []string {
	t.Helper()
	if !mr.Exists(testGateIndexRegistryKey) {
		return nil
	}
	members, err := mr.Members(testGateIndexRegistryKey)
	require.NoError(t, err)
	return members
}

func TestGateIndex_FirstLoginIndexesOnlineSession(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(7001, 100, idxGateA, 1)))

	assert.Equal(t, []string{"7001"}, indexMembers(t, mr, idxGateA))
	assert.Equal(t, []string{idxGateA}, registryMembers(t, mr))
}

func TestGateIndex_ReplaceLoginMovesMemberAcrossGates(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(7002, 100, idxGateA, 1)))
	require.NoError(t, callSetSession(sc, indexTestSession(7002, 200, idxGateB, 2)))

	assert.Empty(t, indexMembers(t, mr, idxGateA), "顶号换 gate 后旧 gate 的索引里不得残留")
	assert.Equal(t, []string{"7002"}, indexMembers(t, mr, idxGateB))
	assert.ElementsMatch(t, []string{idxGateA, idxGateB}, registryMembers(t, mr),
		"登记集只增不减;空了的 gate 由对账在它死后释放")
}

// 同一 gate 上顶号:旧索引键 == 新索引键。钉住脚本里「先 SADD 后 SREM」这种顺序写错会把成员删掉。
func TestGateIndex_ReplaceLoginOnSameGateKeepsMember(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(7003, 100, idxGateA, 1)))
	require.NoError(t, callSetSession(sc, indexTestSession(7003, 200, idxGateA, 2)))

	assert.Equal(t, []string{"7003"}, indexMembers(t, mr, idxGateA))
}

func TestGateIndex_SetDisconnectingRemovesMember(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(7004, 100, idxGateA, 1)))

	_, err := NewSetDisconnectingLogic(context.Background(), sc).SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId: 7004, SessionId: 100, GateInstanceId: idxGateA,
	})
	require.NoError(t, err)

	assert.Empty(t, indexMembers(t, mr, idxGateA))
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 7004).State)
	leases, err := mr.ZMembers(LeaseZSetKey)
	require.NoError(t, err)
	assert.Contains(t, leases, "7004", "原有行为:断线进租约 ZSET")
}

func TestGateIndex_ReconnectIndexesNewGate(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	ctx := context.Background()
	require.NoError(t, callSetSession(sc, indexTestSession(7005, 100, idxGateA, 1)))
	_, err := NewSetDisconnectingLogic(ctx, sc).SetDisconnecting(&pb.SetDisconnectingRequest{PlayerId: 7005, SessionId: 100})
	require.NoError(t, err)

	resp, err := NewReconnectLogic(ctx, sc).Reconnect(&pb.ReconnectRequest{
		PlayerId: 7005, NewSessionId: 200, GateId: "2", GateInstanceId: idxGateB, RequestId: "rc-1",
	})
	require.NoError(t, err)
	require.True(t, resp.Success, resp.ErrorMessage)

	assert.Empty(t, indexMembers(t, mr, idxGateA))
	assert.Equal(t, []string{"7005"}, indexMembers(t, mr, idxGateB))
}

// gate_instance_id 为空(旧版 login 或早期记录)的会话不进任何索引;空串哨兵永不落成名为 "" 的键。
func TestGateIndex_EmptyGateInstanceNotIndexed(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(7006, 100, "", 1)))
	_, err := NewSetDisconnectingLogic(context.Background(), sc).SetDisconnecting(&pb.SetDisconnectingRequest{PlayerId: 7006, SessionId: 100})
	require.NoError(t, err)

	assert.Empty(t, registryMembers(t, mr))
	for _, key := range mr.Keys() {
		assert.NotContains(t, key, testGateIndexSessionsPrefix, "空 gate 不得建出任何索引键")
	}
	assert.False(t, mr.Exists(""), "空串哨兵不得落成名为 \"\" 的键")
}

func TestGateIndex_StaleVersionLeavesIndexUntouched(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(7007, 100, idxGateA, 1)))

	// 版本没有恰好 +1:SetSession 判 stale 拒绝,索引一个字节都不动。
	require.Error(t, callSetSession(sc, indexTestSession(7007, 200, idxGateB, 5)))

	assert.Equal(t, []string{"7007"}, indexMembers(t, mr, idxGateA))
	assert.Nil(t, indexMembers(t, mr, idxGateB))
	assert.Equal(t, []string{idxGateA}, registryMembers(t, mr))
}

func TestGateIndex_ReplayIsIdempotent(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	s := indexTestSession(7008, 100, idxGateA, 1)
	require.NoError(t, callSetSession(sc, s))
	require.NoError(t, callSetSession(sc, s), "RPC 响应丢失后的同请求重放")

	assert.Equal(t, []string{"7008"}, indexMembers(t, mr, idxGateA))
}

// missing 模式下旧清理仍在 processing:SetSession fail-closed 返回 ErrSessionCleanupPending,不得建任何索引。
func TestGateIndex_CleanupPendingDoesNotIndex(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	_, err := mr.ZAdd(LeaseProcessingZSetKey, 9999999999, "7009")
	require.NoError(t, err)

	err = callSetSession(sc, indexTestSession(7009, 100, idxGateA, 1))
	require.ErrorIs(t, err, ErrSessionCleanupPending)

	assert.Nil(t, indexMembers(t, mr, idxGateA))
	assert.Empty(t, registryMembers(t, mr))
}

func TestGateIndex_MarkOfflineRemovesMember(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	ctx := context.Background()
	require.NoError(t, callSetSession(sc, indexTestSession(7010, 100, idxGateA, 1)))

	// 带陈旧 expected 的 MarkOffline 是 no-op:会话与成员都保留。
	_, err := NewMarkOfflineLogic(ctx, sc).MarkOffline(&pb.MarkOfflineRequest{
		PlayerId: 7010, ExpectedSessionId: 100, ExpectedSessionVersion: 9,
	})
	require.NoError(t, err)
	assert.Equal(t, []string{"7010"}, indexMembers(t, mr, idxGateA))

	_, err = NewMarkOfflineLogic(ctx, sc).MarkOffline(&pb.MarkOfflineRequest{
		PlayerId: 7010, ExpectedSessionId: 100, ExpectedSessionVersion: 1, ExpectedGateInstanceId: idxGateA,
	})
	require.NoError(t, err)
	assert.False(t, mr.Exists(sessionKey(7010)))
	assert.Empty(t, indexMembers(t, mr, idxGateA))
}
