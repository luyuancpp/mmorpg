package logic

import (
	"context"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/config"
	"player_locator/internal/svc"
	pb "proto/player_locator"
)

// ---------------------------------------------------------------------------
// Test helper
// ---------------------------------------------------------------------------

func newTestSvcCtx(t *testing.T) (*svc.ServiceContext, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { rdb.Close() })

	return &svc.ServiceContext{
		Config: config.Config{
			Lease: config.LeaseConf{DefaultTTLSeconds: 30},
		},
		RedisClient: rdb,
	}, mr
}

// ---------------------------------------------------------------------------
// Location tests
// ---------------------------------------------------------------------------

func TestSetAndGetLocation(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	setLogic := NewSetLocationLogic(ctx, sc)
	_, err := setLogic.SetLocation(&pb.PlayerLocation{
		Uid:      1001,
		ServerId: "scene-node-1",
		SceneId:  42,
	})
	require.NoError(t, err)

	getLogic := NewGetLocationLogic(ctx, sc)
	loc, err := getLogic.GetLocation(&pb.PlayerId{Uid: 1001})
	require.NoError(t, err)

	assert.Equal(t, int64(1001), loc.Uid)
	assert.True(t, loc.Online)
	assert.Equal(t, "scene-node-1", loc.ServerId)
	assert.Equal(t, int32(42), loc.SceneId)
	assert.True(t, loc.Ts > 0, "timestamp should be set")
}

func TestGetLocation_NotFound(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	logic := NewGetLocationLogic(ctx, sc)
	loc, err := logic.GetLocation(&pb.PlayerId{Uid: 9999})
	require.NoError(t, err)

	assert.Equal(t, int64(9999), loc.Uid)
	assert.False(t, loc.Online)
}

func TestMarkOffline(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	// Set location first
	setLogic := NewSetLocationLogic(ctx, sc)
	_, err := setLogic.SetLocation(&pb.PlayerLocation{Uid: 2001, ServerId: "node-1"})
	require.NoError(t, err)

	// Verify it exists
	getLogic := NewGetLocationLogic(ctx, sc)
	loc, _ := getLogic.GetLocation(&pb.PlayerId{Uid: 2001})
	assert.True(t, loc.Online)

	// Mark offline
	offlineLogic := NewMarkOfflineLogic(ctx, sc)
	_, err = offlineLogic.MarkOffline(&pb.PlayerId{Uid: 2001})
	require.NoError(t, err)

	// Verify it's gone
	loc, _ = getLogic.GetLocation(&pb.PlayerId{Uid: 2001})
	assert.False(t, loc.Online)
}

// ---------------------------------------------------------------------------
// Session tests
// ---------------------------------------------------------------------------

func makeTestSession(playerID uint64) *pb.PlayerSession {
	return &pb.PlayerSession{
		PlayerId:       playerID,
		SessionId:      100,
		GateId:         "gate-1",
		GateInstanceId: "inst-aaa",
		SceneNodeId:    "scene-1",
		SceneId:        50,
		TokenId:        "tok-abc",
		SessionVersion: 1,
		State:          pb.PlayerSessionState_SESSION_STATE_ONLINE,
		LastActiveTs:   time.Now().UnixMilli(),
		Account:        "test_account",
	}
}

func TestSetAndGetSession(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	session := makeTestSession(3001)

	setLogic := NewSetSessionLogic(ctx, sc)
	_, err := setLogic.SetSession(&pb.SetSessionRequest{Session: session})
	require.NoError(t, err)

	getLogic := NewGetSessionLogic(ctx, sc)
	resp, err := getLogic.GetSession(&pb.GetSessionRequest{PlayerId: 3001})
	require.NoError(t, err)

	assert.True(t, resp.Found)
	assert.Equal(t, uint64(3001), resp.Session.PlayerId)
	assert.Equal(t, uint64(100), resp.Session.SessionId)
	assert.Equal(t, "gate-1", resp.Session.GateId)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_ONLINE, resp.Session.State)
}

func TestGetSession_NotFound(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	logic := NewGetSessionLogic(ctx, sc)
	resp, err := logic.GetSession(&pb.GetSessionRequest{PlayerId: 9999})
	require.NoError(t, err)

	assert.False(t, resp.Found)
}

func TestSetSession_NilSessionReturnsError(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	logic := NewSetSessionLogic(ctx, sc)
	_, err := logic.SetSession(&pb.SetSessionRequest{Session: nil})
	assert.Error(t, err)
}

// ---------------------------------------------------------------------------
// Disconnect → Reconnect lifecycle
// ---------------------------------------------------------------------------

func TestSetDisconnecting_HappyPath(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	ctx := context.Background()

	// Create session
	session := makeTestSession(4001)
	setLogic := NewSetSessionLogic(ctx, sc)
	_, err := setLogic.SetSession(&pb.SetSessionRequest{Session: session})
	require.NoError(t, err)

	// Disconnect with matching sessionId
	dcLogic := NewSetDisconnectingLogic(ctx, sc)
	_, err = dcLogic.SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId:        4001,
		SessionId:       100,
		LeaseTtlSeconds: 10,
	})
	require.NoError(t, err)

	// Verify session state changed to DISCONNECTING
	getLogic := NewGetSessionLogic(ctx, sc)
	resp, err := getLogic.GetSession(&pb.GetSessionRequest{PlayerId: 4001})
	require.NoError(t, err)
	assert.True(t, resp.Found)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, resp.Session.State)

	// Verify lease ZSET entry exists
	members, _ := mr.ZMembers(LeaseZSetKey)
	assert.Contains(t, members, "4001")
}

func TestSetDisconnecting_SessionMismatch_Ignored(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	// Create session with sessionId=100
	session := makeTestSession(5001)
	setLogic := NewSetSessionLogic(ctx, sc)
	_, err := setLogic.SetSession(&pb.SetSessionRequest{Session: session})
	require.NoError(t, err)

	// Disconnect with wrong sessionId=999
	dcLogic := NewSetDisconnectingLogic(ctx, sc)
	_, err = dcLogic.SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId:  5001,
		SessionId: 999,
	})
	require.NoError(t, err)

	// Session should still be ONLINE (not changed)
	getLogic := NewGetSessionLogic(ctx, sc)
	resp, _ := getLogic.GetSession(&pb.GetSessionRequest{PlayerId: 5001})
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_ONLINE, resp.Session.State)
}

func TestSetDisconnecting_NoSession_Noop(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	dcLogic := NewSetDisconnectingLogic(ctx, sc)
	_, err := dcLogic.SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId:  8888,
		SessionId: 1,
	})
	assert.NoError(t, err) // should succeed (no-op)
}

func TestReconnect_HappyPath(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	ctx := context.Background()

	// Create session and disconnect
	session := makeTestSession(6001)
	setLogic := NewSetSessionLogic(ctx, sc)
	_, _ = setLogic.SetSession(&pb.SetSessionRequest{Session: session})

	dcLogic := NewSetDisconnectingLogic(ctx, sc)
	_, _ = dcLogic.SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId:        6001,
		SessionId:       100,
		LeaseTtlSeconds: 30,
	})

	// Reconnect
	rcLogic := NewReconnectLogic(ctx, sc)
	resp, err := rcLogic.Reconnect(&pb.ReconnectRequest{
		PlayerId:       6001,
		NewSessionId:   200,
		GateId:         "gate-2",
		GateInstanceId: "inst-bbb",
		TokenId:        "tok-new",
	})
	require.NoError(t, err)

	assert.True(t, resp.Success)
	assert.Equal(t, uint64(200), resp.Session.SessionId)
	assert.Equal(t, "gate-2", resp.Session.GateId)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_ONLINE, resp.Session.State)
	assert.Equal(t, uint64(2), resp.Session.SessionVersion) // incremented

	// Lease ZSET entry should be removed
	members, _ := mr.ZMembers(LeaseZSetKey)
	assert.NotContains(t, members, "6001")
}

func TestReconnect_NoSession(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	ctx := context.Background()

	logic := NewReconnectLogic(ctx, sc)
	resp, err := logic.Reconnect(&pb.ReconnectRequest{
		PlayerId:     7001,
		NewSessionId: 300,
	})
	require.NoError(t, err)
	assert.False(t, resp.Success)
	assert.Contains(t, resp.ErrorMessage, "no session")
}

// ---------------------------------------------------------------------------
// Full lifecycle: Online → Disconnect → Reconnect → MarkOffline
// ---------------------------------------------------------------------------

func TestFullSessionLifecycle(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	ctx := context.Background()

	playerID := uint64(10001)

	// 1. Set location (player enters game)
	setLocLogic := NewSetLocationLogic(ctx, sc)
	_, err := setLocLogic.SetLocation(&pb.PlayerLocation{
		Uid:      int64(playerID),
		ServerId: "scene-1",
		SceneId:  100,
	})
	require.NoError(t, err)

	// 2. Set session (gate binds session)
	setSessionLogic := NewSetSessionLogic(ctx, sc)
	_, err = setSessionLogic.SetSession(&pb.SetSessionRequest{
		Session: &pb.PlayerSession{
			PlayerId:       playerID,
			SessionId:      500,
			GateId:         "gate-1",
			GateInstanceId: "inst-111",
			SessionVersion: 1,
			State:          pb.PlayerSessionState_SESSION_STATE_ONLINE,
		},
	})
	require.NoError(t, err)

	// 3. Player disconnects
	dcLogic := NewSetDisconnectingLogic(ctx, sc)
	_, err = dcLogic.SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId:        playerID,
		SessionId:       500,
		LeaseTtlSeconds: 30,
	})
	require.NoError(t, err)

	// Verify disconnecting state
	getSession := NewGetSessionLogic(ctx, sc)
	resp, _ := getSession.GetSession(&pb.GetSessionRequest{PlayerId: playerID})
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, resp.Session.State)

	members, _ := mr.ZMembers(LeaseZSetKey)
	assert.Contains(t, members, "10001")

	// 4. Player reconnects
	rcLogic := NewReconnectLogic(ctx, sc)
	rcResp, err := rcLogic.Reconnect(&pb.ReconnectRequest{
		PlayerId:       playerID,
		NewSessionId:   600,
		GateId:         "gate-2",
		GateInstanceId: "inst-222",
	})
	require.NoError(t, err)
	assert.True(t, rcResp.Success)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_ONLINE, rcResp.Session.State)

	// Lease entry should be gone
	members, _ = mr.ZMembers(LeaseZSetKey)
	assert.NotContains(t, members, "10001")

	// 5. Location still exists
	getLoc := NewGetLocationLogic(ctx, sc)
	loc, _ := getLoc.GetLocation(&pb.PlayerId{Uid: int64(playerID)})
	assert.True(t, loc.Online)

	// 6. Player goes offline for real
	offLogic := NewMarkOfflineLogic(ctx, sc)
	_, _ = offLogic.MarkOffline(&pb.PlayerId{Uid: int64(playerID)})

	loc, _ = getLoc.GetLocation(&pb.PlayerId{Uid: int64(playerID)})
	assert.False(t, loc.Online)
}

// ---------------------------------------------------------------------------
// Key format tests
// ---------------------------------------------------------------------------

func TestKeyFormats(t *testing.T) {
	assert.Equal(t, "player:location:42", locationKey(42))
	assert.Equal(t, "player:session:99", sessionKey(99))
}

// ---------------------------------------------------------------------------
// SetDisconnecting default TTL fallback
// ---------------------------------------------------------------------------

func TestSetDisconnecting_DefaultTTL(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	ctx := context.Background()

	session := makeTestSession(11001)
	setLogic := NewSetSessionLogic(ctx, sc)
	_, _ = setLogic.SetSession(&pb.SetSessionRequest{Session: session})

	// Disconnect with LeaseTtlSeconds=0, should use config default (30s)
	dcLogic := NewSetDisconnectingLogic(ctx, sc)
	_, err := dcLogic.SetDisconnecting(&pb.SetDisconnectingRequest{
		PlayerId:        11001,
		SessionId:       100,
		LeaseTtlSeconds: 0,
	})
	require.NoError(t, err)

	// Session key should have a TTL
	ttl := mr.TTL(sessionKey(11001))
	assert.True(t, ttl > 0 && ttl <= 30*time.Second, "TTL should be ~30s, got %v", ttl)

	// Verify session is stored correctly despite TTL
	data, _ := mr.Get(sessionKey(11001))
	s := &pb.PlayerSession{}
	require.NoError(t, proto.Unmarshal([]byte(data), s))
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, s.State)
}
