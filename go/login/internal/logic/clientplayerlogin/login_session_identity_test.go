package clientplayerloginlogic

// 跨账号登录态继承的回归测试(docs/design/data-overwrite-rollback-audit-20261018.md §2.3 login-session-route#1;
// client-request-ordering.md §7.2 第 2 条)。
//
// 场景:gate 被 kill -9 后以同一 node_id 重启,session 序号从 0 开始,新连接拿到与旧进程某条已登录连接相同的
// session_id;旧连接的 login_session 在 30min TTL 内仍在。修复前,新连接跳过 Login 直接发 CreatePlayer / EnterGame,
// login 按 session_id 读到旧连接的账号,于是能在别人的账号下建角、进别人的角色。
// 修复后登录态按 (gate_instance_id, session_incarnation) 建键,撞号的新连接读不到任何账号。

import (
	"context"
	"fmt"
	"testing"
	"time"

	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/auth"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/loginsession"
	"login/internal/logic/pkg/sessionfence"
	"login/internal/logic/pkg/token"
	"login/internal/svc"
	pbbase "proto/common/base"
	dbpb "proto/common/database"
	login_proto "proto/login"
	"shared/generated/pb/table"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

const (
	identitySessionID     = uint32(3<<17 | 5) // node_id = 3、序号 5:重启前后的两个 gate 进程都会发出这个号
	identityVictimAccount = "identity_victim"
	identityVictimPlayer  = uint64(990001)
	identityOwnAccount    = "identity_attacker"
)

// 重启前 gate 进程上、已登录 victim 的连接。
func identityOldGateDetails() *pbbase.SessionDetails {
	return &pbbase.SessionDetails{SessionId: identitySessionID, GateInstanceId: "gate-before-restart", SessionIncarnation: 9}
}

// 重启后同一 node_id 的新 gate 进程上、与旧连接同号的新连接。
func identityRestartedGateDetails() *pbbase.SessionDetails {
	return &pbbase.SessionDetails{SessionId: identitySessionID, GateInstanceId: "gate-after-restart", SessionIncarnation: 1}
}

type identityHarness struct {
	rdb    *redis.Client
	mr     *miniredis.Miniredis
	svcCtx *svc.ServiceContext
}

// newIdentityHarness 种下重启前那条连接的全部残留:victim 的登录态(新键 + 双写的旧键,与新 login 的 CommitLogin
// 一致;旧键也就是旧 login 会写的那个)、victim 的账号 blob 与角色反查索引。
func newIdentityHarness(t *testing.T) *identityHarness {
	t.Helper()
	previous := config.AppConfig
	t.Cleanup(func() { config.AppConfig = previous })
	config.AppConfig.LegacyGateLoginEnabled = true
	config.AppConfig.Node.SessionExpireMin = 30
	config.AppConfig.Locker.AccountLockTTL = 10
	config.AppConfig.Locker.PlayerLockTTL = 120
	config.AppConfig.Account.MaxDevicesPerAccount = 3
	config.AppConfig.Account.MaxPlayersPerAccount = 5
	config.AppConfig.Account.CacheExpire = time.Hour
	config.AppConfig.HomeZone.RefreshRoleListDisabled = true

	mr := miniredis.RunT(t)
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rdb.Close() })
	ctx := context.Background()

	victim := loginsession.RefOf(identityOldGateDetails())
	oldFence, ok := sessionfence.KeyOf(identityOldGateDetails())
	require.True(t, ok)
	require.NoError(t, sessionfence.CommitLogin(ctx, rdb, oldFence, victim.WriteKeys(), identityVictimAccount,
		loginsession.TTL(), constants.GenerateSessionKey(identityVictimAccount), victim.DeviceMember(), loginsession.TTL()))
	require.True(t, mr.Exists(fmt.Sprintf("login_session:%d", identitySessionID)), "前提:按 session_id 的旧键里确实留着 victim")

	blob, err := proto.Marshal(&dbpb.UserAccounts{SimplePlayers: &pbbase.AccountSimplePlayerList{
		Players: []*pbbase.AccountSimplePlayer{{PlayerId: identityVictimPlayer, Name: "受害者"}},
	}})
	require.NoError(t, err)
	require.NoError(t, rdb.Set(ctx, constants.GetAccountDataKey(identityVictimAccount), blob, 0).Err())
	require.NoError(t, rdb.Set(ctx, constants.PlayerToAccountKey(identityVictimPlayer), identityVictimAccount, 0).Err())

	return &identityHarness{rdb: rdb, mr: mr, svcCtx: &svc.ServiceContext{
		RedisClient:  rdb,
		TokenManager: token.NewManager(rdb, token.Config{AccessTokenTTL: time.Hour, RefreshTokenTTL: 30 * 24 * time.Hour}),
	}}
}

func (h *identityHarness) victimBlob(t *testing.T) []byte {
	t.Helper()
	raw, err := h.rdb.Get(context.Background(), constants.GetAccountDataKey(identityVictimAccount)).Bytes()
	require.NoError(t, err)
	return raw
}

func TestCreatePlayerAfterGateRestartWithoutLoginIsRejected(t *testing.T) {
	h := newIdentityHarness(t)
	before := h.victimBlob(t)
	ctx := ctxkeys.WithSessionDetails(context.Background(), identityRestartedGateDetails())

	resp, err := NewCreatePlayerLogic(ctx, h.svcCtx).CreatePlayer(&login_proto.CreatePlayerRequest{})

	require.NoError(t, err)
	require.Equal(t, uint32(table.LoginError_kLoginSessionNotFound), resp.GetErrorMessage().GetId(),
		"撞号的新连接没有 Login 过,不能在 victim 的账号下建角")
	require.Equal(t, before, h.victimBlob(t), "victim 的账号 blob 不得被改动")
	require.False(t, h.mr.Exists("account_lock:create:"+identityVictimAccount), "拒绝发生在拿建角锁之前")
}

func TestEnterGameAfterGateRestartWithoutLoginIsRejected(t *testing.T) {
	h := newIdentityHarness(t)
	ctx := ctxkeys.WithSessionDetails(context.Background(), identityRestartedGateDetails())

	resp, err := NewEnterGameLogic(ctx, h.svcCtx).EnterGame(&login_proto.EnterGameRequest{PlayerId: identityVictimPlayer})

	require.NoError(t, err)
	require.Equal(t, uint32(table.LoginError_kLoginSessionNotFound), resp.GetErrorMessage().GetId(),
		"撞号的新连接没有 Login 过,不能进 victim 的角色")
	require.False(t, h.mr.Exists(fmt.Sprintf("player_locker:%d", identityVictimPlayer)), "拒绝发生在拿玩家锁之前")
	newFence, _ := sessionfence.KeyOf(identityRestartedGateDetails())
	require.False(t, h.mr.Exists("login:sessfence:"+newFence.String()), "拒绝发生在建会话栅栏记录之前,不留残留")
}

func TestLoginOnRestartedGateBindsOwnAccountOnly(t *testing.T) {
	h := newIdentityHarness(t)
	ctx := ctxkeys.WithSessionDetails(context.Background(), identityRestartedGateDetails())
	authType := fmt.Sprintf("identity-login-%d", accountPersistenceAuthSequence.Add(1))
	auth.Register(authType, accountPersistenceAuth{account: identityOwnAccount})

	loginResp, err := NewLoginLogic(ctx, h.svcCtx).Login(&login_proto.LoginRequest{AuthType: authType})
	require.NoError(t, err)
	require.Zero(t, loginResp.GetErrorMessage().GetId(), "正常 Login 不受影响")

	account, err := loginsession.GetAccount(ctx, h.rdb, loginsession.RefOf(identityRestartedGateDetails()))
	require.NoError(t, err)
	require.Equal(t, identityOwnAccount, account, "本连接的登录态是自己的账号")
	legacy, err := h.mr.Get(fmt.Sprintf("login_session:%d", identitySessionID))
	require.NoError(t, err)
	require.Equal(t, identityOwnAccount, legacy, "双写把同号旧键覆盖成本连接的账号,旧 login 也读不到 victim")

	// 登录之后会话检查放行,但读到的是自己的账号:victim 的角色不在其中、反查索引也不指向自己 → 不能进。
	enterResp, err := NewEnterGameLogic(ctx, h.svcCtx).EnterGame(&login_proto.EnterGameRequest{PlayerId: identityVictimPlayer})
	require.NoError(t, err)
	require.Equal(t, uint32(table.LoginError_kLoginEnterGameGuid), enterResp.GetErrorMessage().GetId())
}

func TestLegacyGateWithoutIncarnationStillUsesSessionIDKey(t *testing.T) {
	// 旧版 gate(不带会话代次)的兼容路径:登录态照旧按 session_id 建键,Login 写下、CreatePlayer / EnterGame 读得到。
	// 这条路径保留撞号旧风险,只能靠升级 gate 消除;用量看 loginsession_legacy_key_total。
	h := newIdentityHarness(t)
	legacyDetails := &pbbase.SessionDetails{SessionId: identitySessionID + 1}
	ctx := ctxkeys.WithSessionDetails(context.Background(), legacyDetails)
	authType := fmt.Sprintf("identity-legacy-%d", accountPersistenceAuthSequence.Add(1))
	auth.Register(authType, accountPersistenceAuth{account: identityOwnAccount})

	loginResp, err := NewLoginLogic(ctx, h.svcCtx).Login(&login_proto.LoginRequest{AuthType: authType})
	require.NoError(t, err)
	require.Zero(t, loginResp.GetErrorMessage().GetId())
	stored, err := h.mr.Get(fmt.Sprintf("login_session:%d", identitySessionID+1))
	require.NoError(t, err)
	require.Equal(t, identityOwnAccount, stored)
	members, err := h.mr.Members(constants.GenerateSessionKey(identityOwnAccount))
	require.NoError(t, err)
	require.Equal(t, []string{fmt.Sprint(identitySessionID + 1)}, members, "旧版设备集成员仍是 session_id")

	enterResp, err := NewEnterGameLogic(ctx, h.svcCtx).EnterGame(&login_proto.EnterGameRequest{PlayerId: identityVictimPlayer})
	require.NoError(t, err)
	require.Equal(t, uint32(table.LoginError_kLoginEnterGameGuid), enterResp.GetErrorMessage().GetId(),
		"会话检查放行(读到自己的账号),归属检查拒绝别人的角色")
}

func TestDisconnectOnRestartedGateLeavesOtherConnectionsAlone(t *testing.T) {
	// 撞号的新连接断开:只清本连接的登录态与同号旧键,不动重启前那条连接按新键记的登录态。
	h := newIdentityHarness(t)
	ctx := ctxkeys.WithSessionDetails(context.Background(), identityRestartedGateDetails())

	_, err := NewDisconnectLogic(ctx, h.svcCtx).Disconnect(&login_proto.LoginNodeDisconnectRequest{SessionId: identitySessionID})
	require.NoError(t, err)

	require.False(t, h.mr.Exists(fmt.Sprintf("login_session:%d", identitySessionID)), "同号旧键已删,旧 login 也无从继承")
	require.True(t, h.mr.Exists(loginsession.RefOf(identityOldGateDetails()).Key()), "另一条连接的新键不受影响")
}
