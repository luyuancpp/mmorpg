package data

// SessionReader 的行为回归。不需要 MySQL:只读共享 Redis 的 player:session:<id>,miniredis 足够,所以不挂门控、也不 Skip。
// 会话值由测试用 proto.Marshal(plpb.PlayerSession) 直接写进 miniredis —— 与真正的写者 player_locator 同一种编码。

import (
	"context"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"google.golang.org/protobuf/proto"

	plpb "proto/player_locator"
)

func newSessionReaderOnMiniredis(t *testing.T) (*SessionReader, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	rdb := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})
	return NewSessionReader(rdb), mr
}

func putPlayerSession(t *testing.T, mr *miniredis.Miniredis, key string, session *plpb.PlayerSession) {
	t.Helper()
	payload, err := proto.Marshal(session)
	require.NoError(t, err)
	require.NoError(t, mr.Set(key, string(payload)))
}

// 键名是跨运行时契约:钉住逐字形状,改一个字符就读不到别人写的值。
func TestPlayerSessionKeyIsTheContractShape(t *testing.T) {
	assert.Equal(t, "player:session:42", PlayerSessionKey(42))
	assert.Equal(t, "player:session:18446744073709551615", PlayerSessionKey(^uint64(0)))
}

func TestLookupMissingKeyIsOfflineNotError(t *testing.T) {
	reader, _ := newSessionReaderOnMiniredis(t)
	session, err := reader.Lookup(context.Background(), 42)
	require.NoError(t, err)
	assert.Nil(t, session, "键不存在 = 没登录,应返回 (nil, nil)")
}

func TestLookupReturnsSessionWithRoutingFields(t *testing.T) {
	reader, mr := newSessionReaderOnMiniredis(t)
	putPlayerSession(t, mr, PlayerSessionKey(42), &plpb.PlayerSession{
		PlayerId:       42,
		SessionId:      7,
		GateId:         "3",
		GateInstanceId: "gate-uuid",
		State:          plpb.PlayerSessionState_SESSION_STATE_ONLINE,
	})

	session, err := reader.Lookup(context.Background(), 42)
	require.NoError(t, err)
	require.NotNil(t, session)
	assert.Equal(t, plpb.PlayerSessionState_SESSION_STATE_ONLINE, session.GetState())
	assert.Equal(t, "3", session.GetGateId())
	assert.Equal(t, "gate-uuid", session.GetGateInstanceId())
	assert.Equal(t, uint32(7), session.GetSessionId())
}

// 非 ONLINE 的会话照样返回:在不在线由推送层判,这一层不替它做决定。
func TestLookupReturnsDisconnectingSessionAsIs(t *testing.T) {
	reader, mr := newSessionReaderOnMiniredis(t)
	putPlayerSession(t, mr, PlayerSessionKey(42), &plpb.PlayerSession{
		PlayerId: 42,
		State:    plpb.PlayerSessionState_SESSION_STATE_DISCONNECTING,
	})
	session, err := reader.Lookup(context.Background(), 42)
	require.NoError(t, err)
	require.NotNil(t, session)
	assert.Equal(t, plpb.PlayerSessionState_SESSION_STATE_DISCONNECTING, session.GetState())
}

// 解不开不当离线:否则一次协议事故会表现为"推送全都不推"且零异常。
func TestLookupUndecodableValueIsError(t *testing.T) {
	reader, mr := newSessionReaderOnMiniredis(t)
	require.NoError(t, mr.Set(PlayerSessionKey(42), string([]byte{0xff, 0xff, 0xff})))
	session, err := reader.Lookup(context.Background(), 42)
	assert.Error(t, err)
	assert.Nil(t, session)
}

// 会话里的人与键不符:推过去就是把 A 的信推到 B 的 gate 上,宁可当错误。
func TestLookupIdentityMismatchIsError(t *testing.T) {
	reader, mr := newSessionReaderOnMiniredis(t)
	putPlayerSession(t, mr, PlayerSessionKey(42), &plpb.PlayerSession{
		PlayerId: 43,
		State:    plpb.PlayerSessionState_SESSION_STATE_ONLINE,
	})
	session, err := reader.Lookup(context.Background(), 42)
	assert.Error(t, err)
	assert.Nil(t, session)
}

func TestLookupRedisFailureIsError(t *testing.T) {
	reader, mr := newSessionReaderOnMiniredis(t)
	mr.SetError("injected failure")
	session, err := reader.Lookup(context.Background(), 42)
	assert.Error(t, err)
	assert.Nil(t, session)
}

func TestLookupRejectsZeroAndNilHandle(t *testing.T) {
	reader, _ := newSessionReaderOnMiniredis(t)
	_, err := reader.Lookup(context.Background(), 0)
	assert.Error(t, err, "0 不是合法 player_id")

	var nilReader *SessionReader
	_, err = nilReader.Lookup(context.Background(), 42)
	assert.Error(t, err)
	_, err = NewSessionReader(nil).Lookup(context.Background(), 42)
	assert.Error(t, err)
}
