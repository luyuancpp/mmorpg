package admin

import (
	"context"
	"errors"
	"fmt"
	"time"

	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/locker"
	"login/internal/svc"
	login_proto_common "proto/common/base"
	login_data_base "proto/common/database"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"

	"github.com/zeromicro/go-zero/core/logx"
)

// accountLockReleaseTimeout:释放账号锁的时长上界。释放用脱离请求取消的 ctx,请求超时后也要把锁还回去,
// 否则同账号的建角要等 AccountLockTTL 自然过期。
const accountLockReleaseTimeout = time.Second

// errAccountBusy:账号锁被建角 / 外观修复 / 入场自愈占着。本次不动这个账号,运维稍后重跑即可(按 player_id 幂等)。
var errAccountBusy = errors.New("account is being modified (account_lock:create held); nothing changed, retry later")

type RemovePlayersFromAccountsReq struct {
	PlayerIDs []uint64
}

type RemovePlayersFromAccountsResp struct {
	RemovedCount  uint32
	NotFoundCount uint32
	FailedCount   uint32
}

// RemovePlayersFromAccounts removes the given player IDs from their accounts'
// SimplePlayers lists. Uses the player_to_account:{player_id} reverse mapping
// written by CreatePlayer to find each player's owning account.
//
// Intended for post-rollback orphan cleanup.
//
// 2026-10-18 起每个账号在建角同一把账号锁 account_lock:create:{account} 内处理:锁内重读账号目录、摘掉孤儿,
// 再用一段 Lua 原子地「复核账号锁 → 整字节 CAS 写回账号目录 → 删这些孤儿仍指向本账号的反向索引」
// (removeOrphansScript)。此前是无锁 GET 后整块 SET、之后再逐个 DEL 反向索引:会抹掉同账号并发建出的角色;
// 两步之间 EnterGame 的自愈还能凭仍在的反向索引把刚摘掉的角色写回去。现在自愈在提交点复核反向索引
// (clientplayerlogin.healAccountRoleScript),两边都在账号锁内,不再交错。
// 账号锁被占时这个账号的孤儿计入 FailedCount、什么都不改,重跑即可。
func RemovePlayersFromAccounts(ctx context.Context, svcCtx *svc.ServiceContext, req *RemovePlayersFromAccountsReq) (*RemovePlayersFromAccountsResp, error) {
	logx.Infof("[RemovePlayersFromAccounts] starting: %d player IDs to process: %v", len(req.PlayerIDs), req.PlayerIDs)
	resp := &RemovePlayersFromAccountsResp{}

	// Group player IDs by account to batch account updates
	accountToOrphans := make(map[string][]uint64)

	for _, pid := range req.PlayerIDs {
		reverseKey := constants.PlayerToAccountKey(pid)
		account, err := svcCtx.RedisClient.Get(ctx, reverseKey).Result()
		if errors.Is(err, redis.Nil) {
			logx.Infof("[RemovePlayersFromAccounts] no reverse mapping for player %d (created before reverse index existed)", pid)
			resp.NotFoundCount++
			continue
		}
		if err != nil {
			logx.Errorf("[RemovePlayersFromAccounts] failed to get reverse mapping for player %d: %v", pid, err)
			resp.FailedCount++
			continue
		}
		accountToOrphans[account] = append(accountToOrphans[account], pid)
	}

	// Process each account; the reverse mappings are deleted inside the same commit (removeOrphansScript).
	for account, orphanIDs := range accountToOrphans {
		removed, err := removePlayersFromAccount(ctx, svcCtx, account, orphanIDs)
		if err != nil {
			logx.Errorf("[RemovePlayersFromAccounts] account %s: %v", account, err)
			resp.FailedCount += uint32(len(orphanIDs))
			continue
		}
		resp.RemovedCount += uint32(removed)
	}

	logx.Infof("[RemovePlayersFromAccounts] done: removed=%d not_found=%d failed=%d",
		resp.RemovedCount, resp.NotFoundCount, resp.FailedCount)
	return resp, nil
}

// removePlayersFromAccount 在账号锁内摘掉一个账号里的孤儿角色,并在同一次提交里删掉它们的反向索引。
// 返回实际从账号目录里摘掉的角色数。账号目录不存在或里面没有这些角色时只删反向索引(与改动前一致)。
// 返回 error 时账号目录与反向索引都没动。
func removePlayersFromAccount(ctx context.Context, svcCtx *svc.ServiceContext, account string, playerIDs []uint64) (int, error) {
	rdb := svcCtx.RedisClient
	lockTTL := time.Duration(max(config.AppConfig.Locker.AccountLockTTL, 1)) * time.Second
	accountLock, err := locker.NewRedisLocker(rdb).TryLock(ctx, "account_lock:create:"+account, lockTTL)
	if err != nil {
		return 0, fmt.Errorf("acquire account lock: %w", err)
	}
	if !accountLock.IsLocked() {
		return 0, errAccountBusy
	}
	defer func() {
		releaseCtx, cancel := context.WithTimeout(context.WithoutCancel(ctx), accountLockReleaseTimeout)
		defer cancel()
		if _, err := accountLock.Release(releaseCtx); err != nil {
			logx.Errorf("[RemovePlayersFromAccounts] account %s: account lock release failed: %v", account, err)
		}
	}()

	accountKey := constants.GetAccountDataKey(account)
	var previous, updated []byte
	removed := 0
	data, err := rdb.Get(ctx, accountKey).Bytes()
	switch {
	case errors.Is(err, redis.Nil):
		logx.Infof("[RemovePlayersFromAccounts] account %s not found in Redis (expired?)", account)
	case err != nil:
		return 0, err
	default:
		var filteredCount int
		updated, removed, filteredCount, err = withoutPlayers(data, account, playerIDs)
		if err != nil {
			return 0, err
		}
		if removed > 0 {
			previous = data
			logx.Infof("[RemovePlayersFromAccounts] account %s: before=%d players, after=%d players, removing %d",
				account, filteredCount+removed, filteredCount, removed)
		}
	}

	keys := make([]string, 0, 2+len(playerIDs))
	keys = append(keys, accountKey, accountLock.Key)
	for _, pid := range playerIDs {
		keys = append(keys, constants.PlayerToAccountKey(pid))
	}
	write := "0"
	if updated != nil {
		write = "1"
	}
	result, err := removeOrphansScript.Run(ctx, rdb, keys, accountLock.Value, account, write, previous, updated).Int()
	if err != nil {
		return 0, err
	}
	switch result {
	case -1:
		return 0, errors.New("account lock lost before commit; nothing changed, retry")
	case -2:
		return 0, errors.New("account record changed concurrently; nothing changed, retry")
	}
	logx.Infof("[RemovePlayersFromAccounts] account %s: removed %d orphan characters, deleted %d reverse mappings",
		account, removed, result)
	return removed, nil
}

// withoutPlayers 解析账号目录、去掉 playerIDs 里的角色。removed=0 时 updated 为 nil(不需要写回)。
// filteredCount 是去掉之后剩下的角色数,只用于日志。
func withoutPlayers(data []byte, account string, playerIDs []uint64) (updated []byte, removed, filteredCount int, err error) {
	userAccount := &login_data_base.UserAccounts{}
	if err := proto.Unmarshal(data, userAccount); err != nil {
		return nil, 0, 0, err
	}
	if userAccount.SimplePlayers == nil {
		return nil, 0, 0, nil
	}

	removeSet := make(map[uint64]bool, len(playerIDs))
	for _, pid := range playerIDs {
		removeSet[pid] = true
	}
	original := userAccount.SimplePlayers.Players
	filtered := make([]*login_proto_common.AccountSimplePlayer, 0, len(original))
	for _, p := range original {
		if removeSet[p.PlayerId] {
			removed++
			logx.Infof("[RemovePlayersFromAccounts] account %s: removing orphan player %d", account, p.PlayerId)
			continue
		}
		filtered = append(filtered, p)
	}
	if removed == 0 {
		return nil, 0, len(filtered), nil
	}
	userAccount.SimplePlayers.Players = filtered
	updated, err = proto.Marshal(userAccount)
	if err != nil {
		return nil, 0, 0, err
	}
	return updated, removed, len(filtered), nil
}

// removeOrphansScript 是摘孤儿的提交点,原子地:复核账号锁仍归本次;需要写回时(ARGV[3]="1")按整字节 CAS 写账号目录
// (不带 TTL,账号目录是永久键);再删 KEYS[3..] 里仍指向本账号的反向索引(指向别的账号的不动)。
// KEYS = {账号目录键, 账号锁键, 反向索引键...};ARGV = {账号锁令牌, 账号名, 是否写回, 旧字节, 新字节}。
// 返回删掉的反向索引数(>=0);-1 = 账号锁已不归本次;-2 = 账号目录自重读以来变了。负数时什么也不动。
var removeOrphansScript = redis.NewScript(`
if redis.call("GET", KEYS[2]) ~= ARGV[1] then return -1 end
if ARGV[3] == "1" then
  if redis.call("GET", KEYS[1]) ~= ARGV[4] then return -2 end
  redis.call("SET", KEYS[1], ARGV[5])
end
local deleted = 0
for i = 3, #KEYS do
  if redis.call("GET", KEYS[i]) == ARGV[2] then
    redis.call("DEL", KEYS[i])
    deleted = deleted + 1
  end
end
return deleted
`)
