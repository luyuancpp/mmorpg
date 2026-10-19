package admin

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
	dbpb "proto/common/database"
)

func TestRemovePlayerPreservesRemainingAppearanceWithoutExpiry(t *testing.T) {
	mr := miniredis.RunT(t)
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rdb.Close() })
	ctx := context.Background()
	key := constants.GetAccountDataKey("admin-retention")
	kept := &pbbase.AccountSimplePlayer{PlayerId: 2, AppearanceId: "06_thunder_caster_boy"}
	raw, err := proto.Marshal(&dbpb.UserAccounts{SimplePlayers: &pbbase.AccountSimplePlayerList{Players: []*pbbase.AccountSimplePlayer{
		{PlayerId: 1, AppearanceId: "04_mountain_guardian_boy"}, kept,
	}}})
	if err != nil {
		t.Fatal(err)
	}
	if err := rdb.Set(ctx, key, raw, 12*time.Hour).Err(); err != nil {
		t.Fatal(err)
	}
	if removed, err := removePlayersFromAccount(ctx, &svc.ServiceContext{RedisClient: rdb}, "admin-retention", []uint64{1}); err != nil || removed != 1 {
		t.Fatalf("移除角色失败: removed=%d err=%v", removed, err)
	}
	mr.FastForward(48 * time.Hour)
	raw, err = rdb.Get(ctx, key).Bytes()
	if err != nil {
		t.Fatal(err)
	}
	account := &dbpb.UserAccounts{}
	if err := proto.Unmarshal(raw, account); err != nil {
		t.Fatal(err)
	}
	players := account.GetSimplePlayers().GetPlayers()
	if len(players) != 1 || !proto.Equal(players[0], kept) || mr.TTL(key) != 0 {
		t.Fatalf("剩余角色身份或保留策略错误: %v", account)
	}
}

const orphanAccount = "admin-orphans"

func newAdminRedis(t *testing.T) (*redis.Client, *svc.ServiceContext) {
	t.Helper()
	mr := miniredis.RunT(t)
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rdb.Close() })
	return rdb, &svc.ServiceContext{RedisClient: rdb}
}

func seedAccount(t *testing.T, rdb *redis.Client, account string, ids ...uint64) []byte {
	t.Helper()
	players := make([]*pbbase.AccountSimplePlayer, 0, len(ids))
	for _, id := range ids {
		players = append(players, &pbbase.AccountSimplePlayer{PlayerId: id})
	}
	raw, err := proto.Marshal(&dbpb.UserAccounts{SimplePlayers: &pbbase.AccountSimplePlayerList{Players: players}})
	if err != nil {
		t.Fatal(err)
	}
	if err := rdb.Set(context.Background(), constants.GetAccountDataKey(account), raw, 0).Err(); err != nil {
		t.Fatal(err)
	}
	return raw
}

func seedReverse(t *testing.T, rdb *redis.Client, account string, ids ...uint64) {
	t.Helper()
	for _, id := range ids {
		if err := rdb.Set(context.Background(), constants.PlayerToAccountKey(id), account, 0).Err(); err != nil {
			t.Fatal(err)
		}
	}
}

func reverseOf(t *testing.T, rdb *redis.Client, id uint64) string {
	t.Helper()
	v, err := rdb.Get(context.Background(), constants.PlayerToAccountKey(id)).Result()
	if err == redis.Nil {
		return ""
	}
	if err != nil {
		t.Fatal(err)
	}
	return v
}

func accountIDs(t *testing.T, rdb *redis.Client, account string) []uint64 {
	t.Helper()
	raw, err := rdb.Get(context.Background(), constants.GetAccountDataKey(account)).Bytes()
	if err != nil {
		t.Fatal(err)
	}
	acct := &dbpb.UserAccounts{}
	if err := proto.Unmarshal(raw, acct); err != nil {
		t.Fatal(err)
	}
	var ids []uint64
	for _, p := range acct.GetSimplePlayers().GetPlayers() {
		ids = append(ids, p.GetPlayerId())
	}
	return ids
}

// afterNthGetHook 在第 n 次 GET key 返回之后执行一次 fn,把并发写者插进「锁内重读」与「提交」之间。
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

// 摘孤儿与删反向索引在同一次提交里:摘掉的角色反向索引随之删掉;不在名单里的角色、指向别的账号的反向索引都不动。
func TestRemovePlayersFromAccount_RemovesRolesAndTheirReverseIndexTogether(t *testing.T) {
	rdb, svcCtx := newAdminRedis(t)
	seedAccount(t, rdb, orphanAccount, 1, 2)
	seedReverse(t, rdb, orphanAccount, 1, 2)
	seedReverse(t, rdb, "someone-else", 3)

	removed, err := removePlayersFromAccount(context.Background(), svcCtx, orphanAccount, []uint64{1, 3})
	if err != nil || removed != 1 {
		t.Fatalf("removePlayersFromAccount = (%d, %v), want (1, nil)", removed, err)
	}
	if ids := accountIDs(t, rdb, orphanAccount); len(ids) != 1 || ids[0] != 2 {
		t.Fatalf("account players = %v, want [2]", ids)
	}
	if reverseOf(t, rdb, 1) != "" || reverseOf(t, rdb, 2) != orphanAccount || reverseOf(t, rdb, 3) != "someone-else" {
		t.Fatalf("reverse index = (1:%q 2:%q 3:%q), want only player 1's removed",
			reverseOf(t, rdb, 1), reverseOf(t, rdb, 2), reverseOf(t, rdb, 3))
	}
	if n, err := rdb.Exists(context.Background(), "account_lock:create:"+orphanAccount).Result(); err != nil || n != 0 {
		t.Fatalf("account lock exists=%d err=%v, want released", n, err)
	}
}

// 账号锁被建角 / 自愈占着:这个账号什么都不改(账号目录、反向索引原样),报错让运维重跑。
// 修复前这里无锁整块 SET,会抹掉同账号并发建出的角色。
func TestRemovePlayersFromAccount_AccountBusyChangesNothing(t *testing.T) {
	rdb, svcCtx := newAdminRedis(t)
	before := seedAccount(t, rdb, orphanAccount, 1, 2)
	seedReverse(t, rdb, orphanAccount, 1)
	if err := rdb.Set(context.Background(), "account_lock:create:"+orphanAccount, "create-in-progress", time.Minute).Err(); err != nil {
		t.Fatal(err)
	}

	if _, err := removePlayersFromAccount(context.Background(), svcCtx, orphanAccount, []uint64{1}); err == nil {
		t.Fatal("want an error while the account lock is held by someone else")
	}
	after, err := rdb.Get(context.Background(), constants.GetAccountDataKey(orphanAccount)).Bytes()
	if err != nil || string(after) != string(before) {
		t.Fatalf("account blob changed (err=%v) while the account lock was held", err)
	}
	if reverseOf(t, rdb, 1) != orphanAccount {
		t.Fatal("reverse index must be kept when nothing was committed")
	}
}

// 锁内重读之后账号目录被别的写者改了(锁过期后的迟到写等):整字节 CAS 拒写,反向索引也不删。
func TestRemovePlayersFromAccount_ConcurrentChangeAfterReReadCommitsNothing(t *testing.T) {
	rdb, svcCtx := newAdminRedis(t)
	seedAccount(t, rdb, orphanAccount, 1, 2)
	seedReverse(t, rdb, orphanAccount, 1)
	key := constants.GetAccountDataKey(orphanAccount)
	var other []byte
	rdb.AddHook(&afterNthGetHook{key: key, n: 1, fn: func() {
		other = seedAccount(t, rdb, orphanAccount, 1, 2, 9)
	}})

	if _, err := removePlayersFromAccount(context.Background(), svcCtx, orphanAccount, []uint64{1}); err == nil {
		t.Fatal("want an error when the account record changed after the re-read")
	}
	after, err := rdb.Get(context.Background(), key).Bytes()
	if err != nil || string(after) != string(other) {
		t.Fatalf("account blob = %v (err=%v), want the concurrent writer's content kept", accountIDs(t, rdb, orphanAccount), err)
	}
	if reverseOf(t, rdb, 1) != orphanAccount {
		t.Fatal("reverse index must be kept when nothing was committed")
	}
}

// 账号目录已不存在(过期 / 清过):与改动前一致,只删仍指向本账号的反向索引。
func TestRemovePlayersFromAccount_MissingAccountStillDropsReverseIndex(t *testing.T) {
	rdb, svcCtx := newAdminRedis(t)
	seedReverse(t, rdb, orphanAccount, 1)

	removed, err := removePlayersFromAccount(context.Background(), svcCtx, orphanAccount, []uint64{1})
	if err != nil || removed != 0 {
		t.Fatalf("removePlayersFromAccount = (%d, %v), want (0, nil)", removed, err)
	}
	if reverseOf(t, rdb, 1) != "" {
		t.Fatal("reverse index of a missing account must be dropped")
	}
}
