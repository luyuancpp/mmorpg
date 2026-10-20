package loginsession

// 登录态按 gate 连接建键的回归测试(docs/design/data-overwrite-rollback-audit-20261018.md §2.3、
// client-request-ordering.md §7.2 第 2 条)。核心场景:gate 被 kill -9 后以同一 node_id 重启、session 序号从 0 开始,
// 新连接与旧进程的连接同号;旧连接的登录态在 TTL 内仍在 Redis 里。新连接不先 Login 就不能读到它。

import (
	"context"
	"errors"
	"testing"
	"time"

	"login/internal/config"
	"login/internal/constants"
	pbbase "proto/common/base"

	"github.com/alicebob/miniredis/v2"
	"github.com/prometheus/client_golang/prometheus/testutil"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
)

const (
	collidingSessionID = uint32(3<<17 | 5) // node_id = 3、序号 5:重启前后两个 gate 进程都会发出这个号
	victimAccount      = "victim_account"
)

func newTestRedis(t *testing.T) (*redis.Client, *miniredis.Miniredis) {
	t.Helper()
	previous := config.AppConfig.Node.SessionExpireMin
	config.AppConfig.Node.SessionExpireMin = 30
	t.Cleanup(func() { config.AppConfig.Node.SessionExpireMin = previous })
	mr := miniredis.RunT(t)
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rdb.Close() })
	return rdb, mr
}

// 重启前的 gate 进程上的连接(已登录 victim_account)。
func oldGateDetails() *pbbase.SessionDetails {
	return &pbbase.SessionDetails{SessionId: collidingSessionID, GateInstanceId: "gate-before-restart", SessionIncarnation: 9}
}

// 重启后同一 node_id 的新 gate 进程上、与旧连接同号的新连接。
func restartedGateDetails() *pbbase.SessionDetails {
	return &pbbase.SessionDetails{SessionId: collidingSessionID, GateInstanceId: "gate-after-restart", SessionIncarnation: 1}
}

func TestRefOfFencedConnectionUsesGateInstanceAndIncarnation(t *testing.T) {
	ref := RefOf(oldGateDetails())
	require.False(t, ref.Legacy())
	require.Equal(t, "login_session:gate-before-restart:9", ref.Key())
	require.Equal(t, "gate-before-restart:9", ref.DeviceMember())
	require.Equal(t, []string{"login_session:gate-before-restart:9", "login_session:393221"}, ref.WriteKeys(),
		"带代次的连接双写按 session_id 的旧键,给滚动升级窗口里的旧 login 读")
	require.NotEqual(t, ref.Key(), RefOf(restartedGateDetails()).Key(), "撞号的两条连接必须落在不同的键上")
}

func TestRefOfLegacyGateFallsBackToSessionID(t *testing.T) {
	// 旧版 gate:没有会话代次,或缺实例号。与 sessionfence.KeyOf 的判定一致。
	for _, details := range []*pbbase.SessionDetails{
		{SessionId: collidingSessionID},
		{SessionId: collidingSessionID, GateInstanceId: "gate-x"},
		{SessionId: collidingSessionID, SessionIncarnation: 3},
	} {
		ref := RefOf(details)
		require.True(t, ref.Legacy(), "details=%v", details)
		require.Equal(t, "login_session:393221", ref.Key())
		require.Equal(t, "393221", ref.DeviceMember(), "旧版成员值与原先 SADD uint32 session_id 的写法一致")
		require.Equal(t, []string{"login_session:393221"}, ref.WriteKeys())
	}
	const maxUint32 uint32 = 0xFFFFFFFF
	require.Equal(t, "login_session:4294967295", RefOf(&pbbase.SessionDetails{SessionId: maxUint32}).Key())
}

func TestGetAccountAfterGateRestartDoesNotInheritCollidingSession(t *testing.T) {
	ctx := context.Background()
	rdb, _ := newTestRedis(t)
	// 重启前:旧连接以新、旧两种键形状都留下了 victim 的登录态(新 login 双写),TTL 30min 内仍有效。
	victim := RefOf(oldGateDetails())
	for _, key := range victim.WriteKeys() {
		require.NoError(t, rdb.Set(ctx, key, victimAccount, TTL()).Err())
	}

	// 重启后同号的新连接跳过 Login:读不到任何账号,也绝不回落到同号的旧键。
	_, err := GetAccount(ctx, rdb, RefOf(restartedGateDetails()))
	require.True(t, errors.Is(err, redis.Nil), "撞号的新连接没有登录态,err=%v", err)

	// 旧连接自己的登录态不受影响。
	account, err := GetAccount(ctx, rdb, victim)
	require.NoError(t, err)
	require.Equal(t, victimAccount, account)
}

func TestGetAccountAfterLoginOnRestartedGateReadsOwnAccount(t *testing.T) {
	ctx := context.Background()
	rdb, _ := newTestRedis(t)
	require.NoError(t, Save(ctx, rdb, RefOf(oldGateDetails()), victimAccount))
	own := RefOf(restartedGateDetails())
	require.NoError(t, Save(ctx, rdb, own, "own_account"))

	account, err := GetAccount(ctx, rdb, own)
	require.NoError(t, err)
	require.Equal(t, "own_account", account, "正常 Login 之后读到的是本连接自己的账号")
}

func TestLegacyGatePathStillWorksAndIsCounted(t *testing.T) {
	ctx := context.Background()
	rdb, mr := newTestRedis(t)
	ref := RefOf(&pbbase.SessionDetails{SessionId: collidingSessionID})
	saves := testutil.ToFloat64(legacyKeyTotal.WithLabelValues("save"))
	gets := testutil.ToFloat64(legacyKeyTotal.WithLabelValues("get"))
	cleanups := testutil.ToFloat64(legacyKeyTotal.WithLabelValues("cleanup"))

	require.NoError(t, Save(ctx, rdb, ref, victimAccount))
	require.Equal(t, 30*time.Minute, mr.TTL("login_session:393221"))
	account, err := GetAccount(ctx, rdb, ref)
	require.NoError(t, err)
	require.Equal(t, victimAccount, account, "旧版 gate 兼容路径照旧按 session_id 读写")
	Cleanup(ctx, rdb, ref, "test")
	require.False(t, mr.Exists("login_session:393221"))

	require.Equal(t, saves+1, testutil.ToFloat64(legacyKeyTotal.WithLabelValues("save")))
	require.Equal(t, gets+1, testutil.ToFloat64(legacyKeyTotal.WithLabelValues("get")))
	require.Equal(t, cleanups+1, testutil.ToFloat64(legacyKeyTotal.WithLabelValues("cleanup")))
}

func TestFencedPathIsNotCountedAsLegacy(t *testing.T) {
	ctx := context.Background()
	rdb, _ := newTestRedis(t)
	ref := RefOf(restartedGateDetails())
	gets := testutil.ToFloat64(legacyKeyTotal.WithLabelValues("get"))
	_, _ = GetAccount(ctx, rdb, ref)
	require.Equal(t, gets, testutil.ToFloat64(legacyKeyTotal.WithLabelValues("get")))
}

func TestCleanupFencedRemovesBothKeysAndDeviceMember(t *testing.T) {
	ctx := context.Background()
	rdb, mr := newTestRedis(t)
	ref := RefOf(oldGateDetails())
	for _, key := range ref.WriteKeys() {
		require.NoError(t, rdb.Set(ctx, key, victimAccount, TTL()).Err())
	}
	deviceSet := constants.GenerateSessionKey(victimAccount)
	require.NoError(t, rdb.SAdd(ctx, deviceSet, ref.DeviceMember(), "another-device:1").Err())

	Cleanup(ctx, rdb, ref, "test")

	require.False(t, mr.Exists("login_session:gate-before-restart:9"))
	require.False(t, mr.Exists("login_session:393221"), "双写的旧键一并删除,不给旧 login / 旧版 gate 留撞号目标")
	members, err := mr.Members(deviceSet)
	require.NoError(t, err)
	require.Equal(t, []string{"another-device:1"}, members)
}

func TestPruneStaleFromDeviceSetHandlesBothMemberShapes(t *testing.T) {
	ctx := context.Background()
	rdb, mr := newTestRedis(t)
	deviceSet := constants.GenerateSessionKey(victimAccount)
	live := RefOf(oldGateDetails())
	require.NoError(t, Save(ctx, rdb, live, victimAccount))
	liveLegacy := RefOf(&pbbase.SessionDetails{SessionId: 77})
	require.NoError(t, Save(ctx, rdb, liveLegacy, victimAccount))
	require.NoError(t, rdb.SAdd(ctx, deviceSet,
		live.DeviceMember(), liveLegacy.DeviceMember(), "gate-gone:4", "78").Err())

	removed := PruneStaleFromDeviceSet(ctx, rdb, deviceSet)

	require.Equal(t, 2, removed)
	members, err := mr.Members(deviceSet)
	require.NoError(t, err)
	require.ElementsMatch(t, []string{live.DeviceMember(), liveLegacy.DeviceMember()}, members)
}
