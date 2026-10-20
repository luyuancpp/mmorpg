package logic

// Reconnect 的同请求重放必须代次也相等(docs/design/scene-enter-chain-residuals.md §3.2 处置 2、§3.7)。
// 同一幂等键 (session_id, request_id) 配上不同代次 = 失锁时两条并发重连链复用了 request_id,按 stale 拒绝,
// 记录逐字节不变 —— 否则会话记录的代次落后于第二条链写进 location 的代次,到期清理的 LeaveScene 判 keep_newer 漏删。
// 辅助标识符统一加 reconnectReplay 前缀。

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/svc"
	pb "proto/player_locator"
)

// reconnectReplayOnline 造出一条经 Reconnect 提交的 ONLINE 记录 {会话 7, request "r1", 代次 committedGeneration},
// 返回那条重连请求(调用方改代次后原样重放)。
func reconnectReplayOnline(t *testing.T, sc *svc.ServiceContext, playerID uint64,
	previousGeneration, committedGeneration uint64) *pb.ReconnectRequest {
	t.Helper()
	ctx := context.Background()
	session := makeTestSession(playerID)
	session.LoginGeneration = previousGeneration
	_, err := NewSetSessionLogic(ctx, sc).SetSession(&pb.SetSessionRequest{Session: session})
	require.NoError(t, err)
	_, err = NewSetDisconnectingLogic(ctx, sc).SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId: playerID, SessionId: session.SessionId, LeaseTtlSeconds: 30,
	})
	require.NoError(t, err)

	req := &pb.ReconnectRequest{
		PlayerId: playerID, NewSessionId: 7, GateId: "gate-2", GateInstanceId: "inst-2",
		RequestId: "r1", LoginGeneration: committedGeneration,
	}
	resp, err := NewReconnectLogic(ctx, sc).Reconnect(req)
	require.NoError(t, err)
	require.True(t, resp.Success, resp.ErrorMessage)
	stored := readSession(t, sc, playerID)
	require.Equal(t, pb.PlayerSessionState_SESSION_STATE_ONLINE, stored.State)
	require.Equal(t, uint32(7), stored.SessionId)
	require.Equal(t, "r1", stored.RequestId)
	require.Equal(t, committedGeneration, stored.LoginGeneration)
	return req
}

func TestReconnect_ReplayWithDifferentGenerationRejected(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	const playerID uint64 = 18001
	req := reconnectReplayOnline(t, sc, playerID, 100, 101)
	committed := readSessionBytes(t, sc, playerID)

	replay := proto.Clone(req).(*pb.ReconnectRequest)
	replay.LoginGeneration = 102
	resp, err := NewReconnectLogic(context.Background(), sc).Reconnect(replay)
	require.NoError(t, err)
	assert.False(t, resp.Success, "同一幂等键配不同代次不是重放")
	assert.Contains(t, resp.ErrorMessage, "replay generation mismatch")
	assert.Equal(t, committed, readSessionBytes(t, sc, playerID), "被拒的重放不得改动记录")

	// 代次更小的同键请求同样拒绝。
	older := proto.Clone(req).(*pb.ReconnectRequest)
	older.LoginGeneration = 100
	resp, err = NewReconnectLogic(context.Background(), sc).Reconnect(older)
	require.NoError(t, err)
	assert.False(t, resp.Success)
	assert.Equal(t, committed, readSessionBytes(t, sc, playerID))
}

func TestReconnect_ReplayWithSameGenerationSucceeds(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	const playerID uint64 = 18002
	req := reconnectReplayOnline(t, sc, playerID, 100, 101)
	committed := readSessionBytes(t, sc, playerID)

	resp, err := NewReconnectLogic(context.Background(), sc).Reconnect(req)
	require.NoError(t, err)
	require.True(t, resp.Success, resp.ErrorMessage)
	assert.Equal(t, uint64(101), resp.Session.LoginGeneration)
	assert.Equal(t, committed, readSessionBytes(t, sc, playerID), "重放不得再次改写记录")
}

// 旧版 login:记录与请求代次都为 0,仍按重放幂等成功。
func TestReconnect_LegacyReplayWithZeroGenerationSucceeds(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	const playerID uint64 = 18003
	req := reconnectReplayOnline(t, sc, playerID, 0, 0)
	committed := readSessionBytes(t, sc, playerID)

	resp, err := NewReconnectLogic(context.Background(), sc).Reconnect(req)
	require.NoError(t, err)
	require.True(t, resp.Success, resp.ErrorMessage)
	assert.Zero(t, resp.Session.LoginGeneration)
	assert.Equal(t, committed, readSessionBytes(t, sc, playerID))
}
