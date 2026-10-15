package clientplayerloginlogic

// gate 连接会话栅栏接进 login 各 handler 之后的回归测试(docs/design/client-request-ordering.md §6.2)。
// 栅栏本身的双边握手在 pkg/sessionfence 里测;这里测的是 handler 有没有在正确的位置调用它、补偿有没有发出:
//   - 后台链绑定点看到「连接已关闭」→ 补挂断线租约(原先玩家永久卡在 ONLINE);
//   - 后台链绑定点看到「期间已离开游戏」→ 删除本次写入的会话;
//   - Disconnect / LeaveGame 带 player_id = 0(BindSession 还没到 gate)时,用栅栏里绑定的玩家收尾;
//   - 换角时会话头带的是旧角色(gate 在 LeaveGame 后不清 playerId),栅栏里绑定的新角色也要收尾;
//   - 绑定报错(结果未知)也补挂断线租约,且补偿不受已取消的链 ctx 影响;
//   - 旧版 gate(没有会话代次)完全不碰栅栏。

import (
	"context"
	"sync"
	"testing"

	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/sessionfence"
	"login/internal/svc"

	pbbase "proto/common/base"
	login_proto "proto/login"
	plpb "proto/player_locator"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
)

const (
	fenceWiringPlayerID  = uint64(880001)
	fenceWiringSessionID = uint32(6161)
	fenceWiringGate      = "gate-uuid-fence"
	fenceWiringIncarn    = uint64(42)
)

var fenceWiringKey = sessionfence.Key{GateInstanceID: fenceWiringGate, Incarnation: fenceWiringIncarn}

// fenceLocator 是 player_locator 的假客户端,只实现这几条收尾路径会碰的 RPC;别的走内嵌的 nil 接口,调到即 panic。
type fenceLocator struct {
	plpb.PlayerLocatorClient

	// current 非 nil 时 GetSession 对任何玩家都回这条会话记录(DeleteSession 据此决定是否 MarkOffline);
	// byPlayer 里有该玩家时优先用它,用来区分换角前后两个角色的记录。
	current  *plpb.PlayerSession
	byPlayer map[uint64]*plpb.PlayerSession

	mu            sync.Mutex
	disconnecting []*plpb.SetDisconnectingRequest
	markedOffline []*plpb.MarkOfflineRequest
}

func (f *fenceLocator) GetSession(_ context.Context, in *plpb.GetSessionRequest, _ ...grpc.CallOption) (*plpb.GetSessionResponse, error) {
	if s, ok := f.byPlayer[in.GetPlayerId()]; ok {
		if s == nil {
			return &plpb.GetSessionResponse{}, nil
		}
		return &plpb.GetSessionResponse{Found: true, Session: s}, nil
	}
	if f.current == nil {
		return &plpb.GetSessionResponse{}, nil
	}
	return &plpb.GetSessionResponse{Found: true, Session: f.current}, nil
}

func (f *fenceLocator) disconnectingPlayers() []uint64 {
	f.mu.Lock()
	defer f.mu.Unlock()
	out := make([]uint64, 0, len(f.disconnecting))
	for _, r := range f.disconnecting {
		out = append(out, r.GetPlayerId())
	}
	return out
}

func (f *fenceLocator) markedOfflinePlayers() []uint64 {
	f.mu.Lock()
	defer f.mu.Unlock()
	out := make([]uint64, 0, len(f.markedOffline))
	for _, r := range f.markedOffline {
		out = append(out, r.GetPlayerId())
	}
	return out
}

func (f *fenceLocator) SetDisconnecting(_ context.Context, in *plpb.SetDisconnectingRequest, _ ...grpc.CallOption) (*pbbase.Empty, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.disconnecting = append(f.disconnecting, in)
	return &pbbase.Empty{}, nil
}

func (f *fenceLocator) MarkOffline(_ context.Context, in *plpb.MarkOfflineRequest, _ ...grpc.CallOption) (*pbbase.Empty, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.markedOffline = append(f.markedOffline, in)
	return &pbbase.Empty{}, nil
}

func newFenceWiring(t *testing.T) (*svc.ServiceContext, *redis.Client, *fenceLocator) {
	t.Helper()
	mr := miniredis.RunT(t)
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rdb.Close() })
	locator := &fenceLocator{}
	return &svc.ServiceContext{RedisClient: rdb, PlayerLocatorClient: locator}, rdb, locator
}

func fenceWiringState(fenced bool, gen uint64) enterGameSessionState {
	return enterGameSessionState{
		playerID:       fenceWiringPlayerID,
		sessionID:      fenceWiringSessionID,
		gateInstanceID: fenceWiringGate,
		fenced:         fenced,
		fenceKey:       fenceWiringKey,
		fenceGen:       gen,
	}
}

func TestBindSessionFence_ClosedBeforeBindPlacesDisconnectLease(t *testing.T) {
	ctx := context.Background()
	svcCtx, rdb, locator := newFenceWiring(t)
	_, gen, err := sessionfence.CheckEnterGame(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, sessionFenceTTL)
	require.NoError(t, err)
	// EnterGame 已受理、后台链还没绑定时断线:Disconnect 先关了栅栏(它带的 player_id 为 0,没能挂租约)。
	_, err = sessionfence.Close(ctx, rdb, fenceWiringKey, sessionFenceTTL)
	require.NoError(t, err)

	l := NewEnterGameLogic(ctx, svcCtx)
	err = l.bindSessionFence(ctx, fenceWiringState(true, gen))
	require.Error(t, err, "连接已关闭时后台链必须按失败收尾,不发 BindSession、不进场")
	require.Len(t, locator.disconnecting, 1, "必须由后台链补挂断线租约,否则玩家永久停在 ONLINE")
	require.Equal(t, fenceWiringPlayerID, locator.disconnecting[0].GetPlayerId())
	require.Equal(t, fenceWiringSessionID, locator.disconnecting[0].GetSessionId())
	require.Empty(t, locator.markedOffline)
}

func TestBindSessionFence_LeftDuringChainRemovesSession(t *testing.T) {
	ctx := context.Background()
	svcCtx, rdb, locator := newFenceWiring(t)
	_, gen, err := sessionfence.CheckEnterGame(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, sessionFenceTTL)
	require.NoError(t, err)
	_, err = sessionfence.Leave(ctx, rdb, fenceWiringKey, sessionFenceTTL)
	require.NoError(t, err)
	// 后台链已经写了会话(persist 成功),LeaveGame 在那之前已经执行完、删不到它。
	locator.current = &plpb.PlayerSession{SessionId: fenceWiringSessionID, SessionVersion: 3}

	l := NewEnterGameLogic(ctx, svcCtx)
	err = l.bindSessionFence(ctx, fenceWiringState(true, gen))
	require.Error(t, err)
	require.Len(t, locator.markedOffline, 1, "期间离开过游戏,本次写入的会话必须删掉")
	require.Equal(t, fenceWiringPlayerID, locator.markedOffline[0].GetPlayerId())
	require.Equal(t, fenceWiringSessionID, locator.markedOffline[0].GetExpectedSessionId())
	require.Empty(t, locator.disconnecting)
}

func TestBindSessionFence_OpenConnectionBindsAndLegacyGateSkips(t *testing.T) {
	ctx := context.Background()
	svcCtx, rdb, locator := newFenceWiring(t)
	l := NewEnterGameLogic(ctx, svcCtx)

	require.NoError(t, l.bindSessionFence(ctx, fenceWiringState(false, 0)), "旧版 gate 不碰栅栏")
	exists, err := rdb.Exists(ctx, "login:sessfence:"+fenceWiringGate+":42").Result()
	require.NoError(t, err)
	require.Zero(t, exists)

	_, gen, err := sessionfence.CheckEnterGame(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, sessionFenceTTL)
	require.NoError(t, err)
	require.NoError(t, l.bindSessionFence(ctx, fenceWiringState(true, gen)))
	bound, err := sessionfence.Close(ctx, rdb, fenceWiringKey, sessionFenceTTL)
	require.NoError(t, err)
	require.Equal(t, fenceWiringPlayerID, bound, "绑定成功之后,Disconnect 能从栅栏读到玩家")
	require.Empty(t, locator.disconnecting)
	require.Empty(t, locator.markedOffline)
}

func fenceWiringCtx(playerID uint64) context.Context {
	return ctxkeys.WithSessionDetails(context.Background(), &pbbase.SessionDetails{
		SessionId:          fenceWiringSessionID,
		PlayerId:           playerID,
		GateInstanceId:     fenceWiringGate,
		SessionIncarnation: fenceWiringIncarn,
	})
}

func TestDisconnect_ZeroPlayerIdUsesFenceBoundPlayer(t *testing.T) {
	// 后台链已经绑定了玩家,但 gate 会话还没收到 BindSession,gate 合成的 Disconnect 带 player_id = 0。
	svcCtx, rdb, locator := newFenceWiring(t)
	ctx := fenceWiringCtx(0)
	_, gen, err := sessionfence.CheckEnterGame(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, sessionFenceTTL)
	require.NoError(t, err)
	outcome, err := sessionfence.BindPlayer(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, gen, sessionFenceTTL)
	require.NoError(t, err)
	require.Equal(t, sessionfence.OutcomeOK, outcome)

	_, err = NewDisconnectLogic(ctx, svcCtx).Disconnect(&login_proto.LoginNodeDisconnectRequest{SessionId: fenceWiringSessionID})
	require.NoError(t, err)
	require.Len(t, locator.disconnecting, 1, "原先在 playerID == 0 处早退,玩家永久停在 ONLINE")
	require.Equal(t, fenceWiringPlayerID, locator.disconnecting[0].GetPlayerId())

	outcome, err = sessionfence.Recheck(ctx, rdb, fenceWiringKey, gen)
	require.NoError(t, err)
	require.Equal(t, sessionfence.OutcomeClosed, outcome, "Disconnect 之后后台链的进场前复查必须看到已关闭")
}

func TestLeaveGame_ZeroPlayerIdUsesFenceBoundPlayer(t *testing.T) {
	svcCtx, rdb, locator := newFenceWiring(t)
	ctx := fenceWiringCtx(0)
	_, gen, err := sessionfence.CheckEnterGame(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, sessionFenceTTL)
	require.NoError(t, err)
	_, err = sessionfence.BindPlayer(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, gen, sessionFenceTTL)
	require.NoError(t, err)
	locator.current = &plpb.PlayerSession{SessionId: fenceWiringSessionID, SessionVersion: 5}

	_, err = NewLeaveGameLogic(ctx, svcCtx).LeaveGame(&login_proto.LeaveGameRequest{})
	require.NoError(t, err)
	require.Len(t, locator.markedOffline, 1)
	require.Equal(t, fenceWiringPlayerID, locator.markedOffline[0].GetPlayerId())

	outcome, err := sessionfence.Recheck(ctx, rdb, fenceWiringKey, gen)
	require.NoError(t, err)
	require.Equal(t, sessionfence.OutcomeLeft, outcome, "LeaveGame 之后后台链的复查必须看到已离开")
}

// 换角:同一连接 EnterGame(旧角色) → LeaveGame → EnterGame(新角色),后台链已为新角色写会话并绑定栅栏,
// gate 还没收到 BindSession(新角色)。gate 在 LeaveGame 之后不清 playerId,会话头带的是旧角色。
const fenceWiringStalePlayerID = uint64(880000)

func bindNewRoleAfterLeave(t *testing.T, ctx context.Context, rdb *redis.Client) uint64 {
	t.Helper()
	_, gen, err := sessionfence.CheckEnterGame(ctx, rdb, fenceWiringKey, fenceWiringStalePlayerID, sessionFenceTTL)
	require.NoError(t, err)
	_, err = sessionfence.BindPlayer(ctx, rdb, fenceWiringKey, fenceWiringStalePlayerID, gen, sessionFenceTTL)
	require.NoError(t, err)
	_, err = sessionfence.Leave(ctx, rdb, fenceWiringKey, sessionFenceTTL)
	require.NoError(t, err)
	_, gen, err = sessionfence.CheckEnterGame(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, sessionFenceTTL)
	require.NoError(t, err)
	outcome, err := sessionfence.BindPlayer(ctx, rdb, fenceWiringKey, fenceWiringPlayerID, gen, sessionFenceTTL)
	require.NoError(t, err)
	require.Equal(t, sessionfence.OutcomeOK, outcome)
	return gen
}

func TestDisconnect_StalePlayerIdAlsoSettlesFenceBoundPlayer(t *testing.T) {
	svcCtx, rdb, locator := newFenceWiring(t)
	ctx := fenceWiringCtx(fenceWiringStalePlayerID)
	bindNewRoleAfterLeave(t, ctx, rdb)

	_, err := NewDisconnectLogic(ctx, svcCtx).Disconnect(&login_proto.LoginNodeDisconnectRequest{SessionId: fenceWiringSessionID})
	require.NoError(t, err)
	require.ElementsMatch(t, []uint64{fenceWiringStalePlayerID, fenceWiringPlayerID}, locator.disconnectingPlayers(),
		"新角色必须挂上断线租约(只认会话头的旧角色时它永久停在 ONLINE);旧角色照旧收尾,对它是按 session_id 的 no-op")
}

func TestLeaveGame_StalePlayerIdAlsoSettlesFenceBoundPlayer(t *testing.T) {
	svcCtx, rdb, locator := newFenceWiring(t)
	ctx := fenceWiringCtx(fenceWiringStalePlayerID)
	bindNewRoleAfterLeave(t, ctx, rdb)
	// 旧角色的会话已被上一次 LeaveGame 删掉,新角色的会话是后台链刚写入的。
	locator.byPlayer = map[uint64]*plpb.PlayerSession{
		fenceWiringStalePlayerID: nil,
		fenceWiringPlayerID:      {SessionId: fenceWiringSessionID, SessionVersion: 7},
	}

	_, err := NewLeaveGameLogic(ctx, svcCtx).LeaveGame(&login_proto.LeaveGameRequest{})
	require.NoError(t, err)
	require.Equal(t, []uint64{fenceWiringPlayerID}, locator.markedOfflinePlayers(), "新角色刚写入的会话必须删掉")
}

func TestSessionPlayersToSettle(t *testing.T) {
	require.Empty(t, sessionPlayersToSettle(0, 0))
	require.Equal(t, []uint64{5}, sessionPlayersToSettle(5, 0))
	require.Equal(t, []uint64{5}, sessionPlayersToSettle(0, 5))
	require.Equal(t, []uint64{5}, sessionPlayersToSettle(5, 5), "同一玩家只收尾一次")
	require.Equal(t, []uint64{4, 5}, sessionPlayersToSettle(4, 5))
}

func TestBindSessionFence_BindErrorPlacesDisconnectLeaseEvenIfChainCanceled(t *testing.T) {
	// 绑定结果未知(这里用已取消的链 ctx 让 BindPlayer 报错,同时模拟心跳丢锁后的 chainCancel):
	// 链不会再发 BindSession,必须补挂断线租约;补偿用独立 ctx,不因链 ctx 已取消而放弃。
	svcCtx, rdb, locator := newFenceWiring(t)
	_, gen, err := sessionfence.CheckEnterGame(context.Background(), rdb, fenceWiringKey, fenceWiringPlayerID, sessionFenceTTL)
	require.NoError(t, err)

	chainCtx, chainCancel := context.WithCancel(context.Background())
	chainCancel()
	l := NewEnterGameLogic(chainCtx, svcCtx)
	err = l.bindSessionFence(chainCtx, fenceWiringState(true, gen))
	require.Error(t, err)
	require.Equal(t, []uint64{fenceWiringPlayerID}, locator.disconnectingPlayers())
	require.Empty(t, locator.markedOffline)
}
