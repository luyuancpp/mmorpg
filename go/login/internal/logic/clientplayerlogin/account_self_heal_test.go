package clientplayerloginlogic

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"

	"login/internal/constants"
	"login/internal/svc"
	pbbase "proto/common/base"
	comppb "proto/common/component"
	dbpb "proto/common/database"
	loginpb "proto/login"
)

// selfHealConcurrentRoleID 是「自愈读账号之后、写回之前」被并发建出的另一个角色。
const selfHealConcurrentRoleID = uint64(7002)

// afterNthGetHook 在第 n 次 GET key 返回之后执行一次 fn,用来把并发写者精确插进两次读写之间:
// EnterGame 里第 1 次是第 4 步锁外读账号目录,第 2 次是自愈锁内重读;直接调 healAccountRole 时第 1 次就是锁内重读。
type afterNthGetHook struct {
	key  string
	n    int
	mu   sync.Mutex
	seen int
	fn   func()
}

func (h *afterNthGetHook) DialHook(next redis.DialHook) redis.DialHook { return next }

func (h *afterNthGetHook) ProcessHook(next redis.ProcessHook) redis.ProcessHook {
	return func(ctx context.Context, cmd redis.Cmder) error {
		err := next(ctx, cmd)
		if args := cmd.Args(); cmd.Name() == "get" && len(args) > 1 && args[1] == h.key {
			h.mu.Lock()
			h.seen++
			fire := h.seen == h.n
			h.mu.Unlock()
			if fire {
				h.fn()
			}
		}
		return err
	}
}

func (h *afterNthGetHook) ProcessPipelineHook(next redis.ProcessPipelineHook) redis.ProcessPipelineHook {
	return next
}

func marshalAccount(t *testing.T, players ...*pbbase.AccountSimplePlayer) []byte {
	t.Helper()
	raw, err := proto.Marshal(&dbpb.UserAccounts{SimplePlayers: &pbbase.AccountSimplePlayerList{Players: players}})
	if err != nil {
		t.Fatal(err)
	}
	return raw
}

func accountPlayerIDs(t *testing.T, rdb *redis.Client, account string) []uint64 {
	t.Helper()
	raw, err := rdb.Get(context.Background(), constants.GetAccountDataKey(account)).Bytes()
	if err != nil {
		t.Fatalf("read account blob: %v", err)
	}
	var ids []uint64
	for _, p := range mustUnmarshalAccount(t, raw).GetSimplePlayers().GetPlayers() {
		ids = append(ids, p.GetPlayerId())
	}
	return ids
}

func enterSelfHeal(t *testing.T, h *enterWiringHarness) {
	t.Helper()
	if err := h.rdb.Set(context.Background(), constants.PlayerToAccountKey(identityPlayerID), enterWiringAccount, 0).Err(); err != nil {
		t.Fatalf("seed reverse index: %v", err)
	}
	resp, err := h.l.EnterGame(&loginpb.EnterGameRequest{PlayerId: identityPlayerID})
	if err != nil {
		t.Fatalf("EnterGame: %v", err)
	}
	if resp.GetErrorMessage() != nil {
		t.Fatalf("EnterGame rejected with tip %d, want accepted (ownership is proven by the reverse index)", resp.GetErrorMessage().GetId())
	}
	h.waitChain(t)
}

// 回归(2026-10-18):自愈原来拿第 4 步锁外读到的账号 blob 直接整块 SET。读之后、写回之前同账号建出的角色
// 会被抹掉。现在锁内重读后整字节 CAS,两个角色都在。修复前这里只剩 7001,7002 丢失。
func TestEnterGame_SelfHealKeepsRoleCreatedConcurrently(t *testing.T) {
	h := newEnterWiringHarness(t, nil, map[uint64]string{identityPlayerID: "云中君"})
	key := constants.GetAccountDataKey(enterWiringAccount)
	concurrent := marshalAccount(t, &pbbase.AccountSimplePlayer{PlayerId: selfHealConcurrentRoleID, Name: "新角色"})
	h.rdb.AddHook(&afterNthGetHook{key: key, n: 1, fn: func() {
		// 模拟 CreatePlayer 在自愈读账号之后完成写入(它在锁内整块写 blob,这里直接落最终结果)。
		if err := h.rdb.Set(context.Background(), key, concurrent, 0).Err(); err != nil {
			t.Errorf("simulate concurrent CreatePlayer: %v", err)
		}
	}})

	enterSelfHeal(t, h)

	ids := accountPlayerIDs(t, h.rdb, enterWiringAccount)
	if len(ids) != 2 || ids[0] != selfHealConcurrentRoleID || ids[1] != identityPlayerID {
		t.Fatalf("account players = %v, want [%d %d] (concurrently created role must survive self-heal)",
			ids, selfHealConcurrentRoleID, identityPlayerID)
	}
}

// 账号锁被建角 / 外观修复占着时,自愈不写回、不抢锁,入场照常受理;账号目录与锁原样不动。
func TestEnterGame_SelfHealSkipsWhileAccountLocked(t *testing.T) {
	h := newEnterWiringHarness(t, nil, map[uint64]string{identityPlayerID: "云中君"})
	ctx := context.Background()
	lockKey := "account_lock:create:" + enterWiringAccount
	if err := h.rdb.Set(ctx, lockKey, "create-in-progress", time.Minute).Err(); err != nil {
		t.Fatal(err)
	}

	enterSelfHeal(t, h)

	if ids := accountPlayerIDs(t, h.rdb, enterWiringAccount); len(ids) != 0 {
		t.Fatalf("account players = %v, want none written while the account lock is held by someone else", ids)
	}
	if got, err := h.rdb.Get(ctx, lockKey).Result(); err != nil || got != "create-in-progress" {
		t.Fatalf("account lock = (%q, %v), want the other holder's token untouched", got, err)
	}
}

func newSelfHealLogic(t *testing.T) (*EnterGameLogic, *redis.Client) {
	t.Helper()
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(mr.Close)
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rdb.Close() })
	return NewEnterGameLogic(context.Background(), &svc.ServiceContext{RedisClient: rdb}), rdb
}

func selfHealState(account, lockToken string) enterGameSessionState {
	return enterGameSessionState{
		playerID:        identityPlayerID,
		account:         account,
		playerLockKey:   identityLockKey,
		playerLockToken: lockToken,
	}
}

// 玩家登录锁已不归本链(过期后被新登录拿走):CAS 拒写,账号目录不变,账号锁照常释放。
func TestHealAccountRole_LostPlayerLockDoesNotWrite(t *testing.T) {
	l, rdb := newSelfHealLogic(t)
	ctx := context.Background()
	const account = "heal-lost-lock"
	before := marshalAccount(t)
	if err := rdb.Set(ctx, constants.GetAccountDataKey(account), before, 0).Err(); err != nil {
		t.Fatal(err)
	}
	if err := rdb.Set(ctx, identityLockKey, "someone-else", time.Minute).Err(); err != nil {
		t.Fatal(err)
	}

	if got := l.healAccountRole(ctx, selfHealState(account, identityLockToken), "云中君"); got != nil {
		t.Fatalf("healAccountRole = %v, want nil", got)
	}
	if ids := accountPlayerIDs(t, rdb, account); len(ids) != 0 {
		t.Fatalf("account players = %v, want none written after losing the player lock", ids)
	}
	if n, err := rdb.Exists(ctx, "account_lock:create:"+account).Result(); err != nil || n != 0 {
		t.Fatalf("account lock exists=%d err=%v, want released", n, err)
	}
}

// 锁内重读发现角色已被别的链补回:返回那条记录供调用方取职业 / 外观,不重复追加、不改写。
func TestHealAccountRole_AlreadyRestoredReturnsRecord(t *testing.T) {
	l, rdb := newSelfHealLogic(t)
	ctx := context.Background()
	const account = "heal-already-restored"
	before := marshalAccount(t, &pbbase.AccountSimplePlayer{
		PlayerId: identityPlayerID, Name: "云中君", ClassId: 3, AppearanceId: "04_mountain_guardian_boy", Gender: 1,
	})
	if err := rdb.Set(ctx, constants.GetAccountDataKey(account), before, 0).Err(); err != nil {
		t.Fatal(err)
	}
	if err := rdb.Set(ctx, identityLockKey, identityLockToken, time.Minute).Err(); err != nil {
		t.Fatal(err)
	}

	got := l.healAccountRole(ctx, selfHealState(account, identityLockToken), "注册表名")
	if got.GetPlayerId() != identityPlayerID || got.GetClassId() != 3 || got.GetName() != "云中君" {
		t.Fatalf("healAccountRole = %v, want the existing record (class 3, name 云中君)", got)
	}
	after, err := rdb.Get(ctx, constants.GetAccountDataKey(account)).Bytes()
	if err != nil || string(after) != string(before) {
		t.Fatalf("account blob changed (err=%v); an already restored role must not be rewritten", err)
	}
}

// 正常写回:追加带名字的记录,保留账号里的其他角色,写成永久键。
func TestHealAccountRole_RestoresAndKeepsOtherRoles(t *testing.T) {
	l, rdb := newSelfHealLogic(t)
	ctx := context.Background()
	const account = "heal-restore"
	if err := rdb.Set(ctx, constants.GetAccountDataKey(account),
		marshalAccount(t, &pbbase.AccountSimplePlayer{PlayerId: selfHealConcurrentRoleID}), time.Hour).Err(); err != nil {
		t.Fatal(err)
	}
	if err := rdb.Set(ctx, identityLockKey, identityLockToken, time.Minute).Err(); err != nil {
		t.Fatal(err)
	}

	if got := l.healAccountRole(ctx, selfHealState(account, identityLockToken), "云中君"); got != nil {
		t.Fatalf("healAccountRole = %v, want nil after a fresh restore", got)
	}
	raw, err := rdb.Get(ctx, constants.GetAccountDataKey(account)).Bytes()
	if err != nil {
		t.Fatal(err)
	}
	players := mustUnmarshalAccount(t, raw).GetSimplePlayers().GetPlayers()
	if len(players) != 2 || players[0].GetPlayerId() != selfHealConcurrentRoleID ||
		players[1].GetPlayerId() != identityPlayerID || players[1].GetName() != "云中君" {
		t.Fatalf("account players = %v, want [%d, %d(云中君)]", players, selfHealConcurrentRoleID, identityPlayerID)
	}
	if ttl, err := rdb.TTL(ctx, constants.GetAccountDataKey(account)).Result(); err != nil || ttl != -1 {
		t.Fatalf("account record ttl=%v err=%v, want a persistent key", ttl, err)
	}
}

// 锁内重读发现角色已被别的链补回:职业、名字以那条记录为准进存档(与「账号记录命中」分支同源),
// 不用注册表名,也不重写账号目录(评审 C3①)。删掉 entergamelogic.go 里对 existing 的回填,这条会红。
func TestEnterGame_SelfHealUsesRecordRestoredConcurrently(t *testing.T) {
	h := newEnterWiringHarness(t, nil, map[uint64]string{identityPlayerID: "注册表名"})
	key := constants.GetAccountDataKey(enterWiringAccount)
	restored := marshalAccount(t, &pbbase.AccountSimplePlayer{PlayerId: identityPlayerID, ClassId: 3, Name: "账号名"})
	h.rdb.AddHook(&afterNthGetHook{key: key, n: 1, fn: func() {
		if err := h.rdb.Set(context.Background(), key, restored, 0).Err(); err != nil {
			t.Errorf("simulate concurrent restore: %v", err)
		}
	}})

	enterSelfHeal(t, h)

	if got := h.locator.setSessionCalls.Load(); got != 1 {
		t.Fatalf("SetSession calls = %d, want 1 (chain must get past identity backfill)", got)
	}
	_, player := readIdentityBlob(t, h.rdb)
	if player.GetUint32PbComponent().GetClass() != 3 || player.GetProfileComponent().GetName() != "账号名" {
		t.Fatalf("player identity = class %d name %q, want class 3 name 账号名 from the restored record",
			player.GetUint32PbComponent().GetClass(), player.GetProfileComponent().GetName())
	}
	after, err := h.rdb.Get(context.Background(), key).Bytes()
	if err != nil || string(after) != string(restored) {
		t.Fatalf("account blob rewritten (err=%v); an already restored role must be left as is", err)
	}
}

// 有意保持的 fail-closed(评审 C1):账号锁被占、自愈没写回,而存档已有外观时,异步链的外观修复要求账号里有这条记录,
// 本次入场在身份补齐处停下(不落会话),账号目录不动;锁空出来后下次入场先自愈、再修外观。
func TestEnterGame_SelfHealSkippedFailsAppearanceRepair(t *testing.T) {
	h := newEnterWiringHarness(t, nil, map[uint64]string{identityPlayerID: "云中君"})
	ctx := context.Background()
	playerBlob, err := proto.Marshal(&dbpb.PlayerAllData{
		PlayerDatabaseData: &dbpb.PlayerDatabase{
			PlayerId:         identityPlayerID,
			ProfileComponent: &comppb.PlayerProfileComp{AppearanceId: "04_mountain_guardian_boy", Gender: 1},
		},
		PlayerDatabase_1Data: &dbpb.PlayerDatabase_1{PlayerId: identityPlayerID},
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := h.rdb.Set(ctx, identityBlobKey(), playerBlob, time.Hour).Err(); err != nil {
		t.Fatal(err)
	}
	lockKey := "account_lock:create:" + enterWiringAccount
	if err := h.rdb.Set(ctx, lockKey, "create-in-progress", time.Minute).Err(); err != nil {
		t.Fatal(err)
	}

	enterSelfHeal(t, h)

	if got := h.locator.setSessionCalls.Load(); got != 0 {
		t.Fatalf("SetSession calls = %d, want 0: the chain stops at the appearance repair while the account lacks the role", got)
	}
	if ids := accountPlayerIDs(t, h.rdb, enterWiringAccount); len(ids) != 0 {
		t.Fatalf("account players = %v, want none written while the account lock is held by someone else", ids)
	}
}

// CAS 的整字节比较分支(评审 C3②):锁内重读之后、写回之前账号目录被别的写者改了(不拿锁的运维工具、锁过期后的迟到写),
// 自愈不写,账号目录保持那个写者的结果。删掉 accountRecordCASScript 的字节比较,这条会红。
func TestHealAccountRole_ConcurrentWriteAfterReReadIsNotOverwritten(t *testing.T) {
	l, rdb := newSelfHealLogic(t)
	ctx := context.Background()
	const account = "heal-cas-conflict"
	key := constants.GetAccountDataKey(account)
	if err := rdb.Set(ctx, key, marshalAccount(t), 0).Err(); err != nil {
		t.Fatal(err)
	}
	if err := rdb.Set(ctx, identityLockKey, identityLockToken, time.Minute).Err(); err != nil {
		t.Fatal(err)
	}
	other := marshalAccount(t, &pbbase.AccountSimplePlayer{PlayerId: selfHealConcurrentRoleID})
	rdb.AddHook(&afterNthGetHook{key: key, n: 1, fn: func() {
		if err := rdb.Set(context.Background(), key, other, 0).Err(); err != nil {
			t.Errorf("simulate concurrent writer: %v", err)
		}
	}})

	if got := l.healAccountRole(ctx, selfHealState(account, identityLockToken), "云中君"); got != nil {
		t.Fatalf("healAccountRole = %v, want nil", got)
	}
	after, err := rdb.Get(ctx, key).Bytes()
	if err != nil || string(after) != string(other) {
		t.Fatalf("account blob = %v (err=%v), want the concurrent writer's content kept", accountPlayerIDs(t, rdb, account), err)
	}
}

// 账号锁令牌校验分支(评审 C3③):锁内重读之后账号锁已不归本次(过期后被建角拿走),自愈不写;
// 按令牌释放也删不掉别人的锁。删掉 accountRecordCASScript 对 KEYS[3] 的校验,这条会红。
func TestHealAccountRole_LostAccountLockDoesNotWrite(t *testing.T) {
	l, rdb := newSelfHealLogic(t)
	ctx := context.Background()
	const account = "heal-lost-account-lock"
	key := constants.GetAccountDataKey(account)
	before := marshalAccount(t)
	if err := rdb.Set(ctx, key, before, 0).Err(); err != nil {
		t.Fatal(err)
	}
	if err := rdb.Set(ctx, identityLockKey, identityLockToken, time.Minute).Err(); err != nil {
		t.Fatal(err)
	}
	lockKey := "account_lock:create:" + account
	rdb.AddHook(&afterNthGetHook{key: key, n: 1, fn: func() {
		if err := rdb.Set(context.Background(), lockKey, "taken-by-create", time.Minute).Err(); err != nil {
			t.Errorf("simulate account lock takeover: %v", err)
		}
	}})

	if got := l.healAccountRole(ctx, selfHealState(account, identityLockToken), "云中君"); got != nil {
		t.Fatalf("healAccountRole = %v, want nil", got)
	}
	after, err := rdb.Get(ctx, key).Bytes()
	if err != nil || string(after) != string(before) {
		t.Fatalf("account blob changed (err=%v) after losing the account lock", err)
	}
	if got, err := rdb.Get(ctx, lockKey).Result(); err != nil || got != "taken-by-create" {
		t.Fatalf("account lock = (%q, %v), want the new holder's token untouched", got, err)
	}
}
