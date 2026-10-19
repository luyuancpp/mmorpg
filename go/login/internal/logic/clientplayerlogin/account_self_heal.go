package clientplayerloginlogic

import (
	"context"
	"errors"
	"time"

	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/locker"
	login_proto_common "proto/common/base"
	login_proto_database "proto/common/database"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// accountHealLockReleaseTimeout:释放账号锁的时长上界。释放用脱离请求取消的 ctx,请求超时后也要把锁还回去,
// 否则同账号的建角要等 AccountLockTTL 自然过期。
const accountHealLockReleaseTimeout = time.Second

// healAccountRole 把反向索引 player_to_account 已证明归属、账号目录里却没有的角色补回 account:{account}。
// 调用方必须已持有该玩家的登录锁(state.playerLockKey / playerLockToken)。
//
// 写法与外观修复 repairAccountAppearance 相同:先拿建角同一把账号锁 account_lock:create:{account},
// 锁内重读账号目录,再按整字节 CAS 写回(healAccountRoleScript 同时校验两把锁的令牌与反向索引)。
//   - 账号锁挡住同账号并发的 CreatePlayer 与运维 RemovePlayersFromAccounts:它们都在锁内读-改-写整个 blob。
//     2026-10-18 之前这里是无锁 GET 之后整块 SET,拿锁外读到的旧 blob 写回,会把刚建出的角色抹掉。
//   - CAS 挡住锁过期后的迟到写;提交点复核反向索引,挡住「运维刚把它当孤儿摘掉、自愈又写回去」。
//
// 尽力而为:拿不到锁、CAS 冲突、Redis 出错都只打日志、不写,本函数不让 EnterGame 同步返回失败(归属已由反向索引证明),
// 下次入场会再试。但这不等于本次入场一定成功:存档里已有外观的角色,异步链上的外观修复 repairAccountAppearance
// 要求账号里有这条记录,写回被跳过时它会报错,本次入场按 kEnterSceneFailed 收口(与 2026-10-18 之前写回失败时相同,
// 有意保持 fail-closed,见 TestEnterGame_SelfHealSkippedFailsAppearanceRepair)。
// 锁内重读发现角色已在(别的链刚补回)时返回那条记录,调用方用它取职业与外观;其余情况返回 nil。
func (l *EnterGameLogic) healAccountRole(ctx context.Context, state enterGameSessionState, name string) *login_proto_common.AccountSimplePlayer {
	account, playerID := state.account, state.playerID
	rdb := l.svcCtx.RedisClient
	lockTTL := time.Duration(max(config.AppConfig.Locker.AccountLockTTL, 1)) * time.Second
	accountLock, err := locker.NewRedisLocker(rdb).TryLock(ctx, "account_lock:create:"+account, lockTTL)
	if err != nil {
		logx.Errorf("EnterGame self-heal skipped: account lock error account=%s playerId=%d err=%v", account, playerID, err)
		return nil
	}
	if !accountLock.IsLocked() {
		// 建角或外观修复正在改这个账号。现在写回只会与它竞争,留给下次入场。
		logx.Infof("EnterGame self-heal skipped: account busy account=%s playerId=%d", account, playerID)
		return nil
	}
	defer func() {
		releaseCtx, cancel := context.WithTimeout(context.WithoutCancel(ctx), accountHealLockReleaseTimeout)
		defer cancel()
		if _, err := accountLock.Release(releaseCtx); err != nil {
			logx.Errorf("EnterGame self-heal account lock release failed: account=%s err=%v", account, err)
		}
	}()

	key := constants.GetAccountDataKey(account)
	previous, err := rdb.Get(ctx, key).Bytes()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			logx.Infof("EnterGame self-heal skipped: account record gone account=%s playerId=%d", account, playerID)
		} else {
			logx.Errorf("EnterGame self-heal re-read failed: account=%s playerId=%d err=%v", account, playerID, err)
		}
		return nil
	}
	userAccount := &login_proto_database.UserAccounts{}
	if err := proto.Unmarshal(previous, userAccount); err != nil {
		logx.Errorf("EnterGame self-heal re-read unmarshal failed: account=%s playerId=%d err=%v", account, playerID, err)
		return nil
	}
	for _, p := range userAccount.GetSimplePlayers().GetPlayers() {
		if p.GetPlayerId() == playerID {
			logx.Infof("EnterGame self-heal: playerId=%d already restored in account=%s", playerID, account)
			return p
		}
	}

	if userAccount.SimplePlayers == nil {
		userAccount.SimplePlayers = &login_proto_common.AccountSimplePlayerList{
			Players: make([]*login_proto_common.AccountSimplePlayer, 0, 1),
		}
	}
	userAccount.SimplePlayers.Players = append(userAccount.SimplePlayers.Players,
		&login_proto_common.AccountSimplePlayer{PlayerId: playerID, Name: name})
	updated, err := proto.Marshal(userAccount)
	if err != nil {
		logx.Errorf("EnterGame self-heal marshal failed: account=%s playerId=%d err=%v", account, playerID, err)
		return nil
	}

	result, err := healAccountRoleScript.Run(ctx, rdb,
		[]string{key, state.playerLockKey, accountLock.Key, constants.PlayerToAccountKey(playerID)},
		state.playerLockToken, previous, updated, accountLock.Value, account).Int()
	switch {
	case err != nil:
		logx.Errorf("EnterGame self-heal write-back failed: account=%s playerId=%d err=%v", account, playerID, err)
	case result == 1:
		logx.Infof("EnterGame self-heal: restored playerId=%d into account=%s", playerID, account)
	case result == -1:
		logx.Errorf("EnterGame self-heal not written: player or account lock lost account=%s playerId=%d", account, playerID)
	case result == -2:
		// 运维 RemovePlayersFromAccounts 刚把它当孤儿摘掉(同一段脚本里删了反向索引),不能再写回去。
		logx.Infof("EnterGame self-heal not written: ownership index no longer points to account=%s playerId=%d", account, playerID)
	default:
		logx.Infof("EnterGame self-heal not written: account record changed concurrently account=%s playerId=%d", account, playerID)
	}
	return nil
}

// healAccountRoleScript 是自愈写回的提交点:两把锁仍归本链、反向索引仍指向本账号、账号目录自重读以来没变,才写。
// 比外观修复的 accountRecordCASScript 多复核一次反向索引:运维 RemovePlayersFromAccounts 在账号锁内、同一段脚本里
// 改写账号目录并删反向索引;自愈在 EnterGame 早先读到的反向索引可能已过时,不在提交点复核,会把刚摘掉的孤儿角色写回去。
// KEYS = {账号目录键, 玩家登录锁键, 账号锁键, 反向索引键};ARGV = {玩家锁令牌, 旧字节, 新字节, 账号锁令牌, 账号名}。
// 返回 1 = 已写;0 = 账号目录变了;-1 = 任一把锁已不归本链;-2 = 反向索引不再指向本账号。都不写时什么也不动。
var healAccountRoleScript = redis.NewScript(`
if redis.call("GET", KEYS[2]) ~= ARGV[1] then return -1 end
if redis.call("GET", KEYS[3]) ~= ARGV[4] then return -1 end
if redis.call("GET", KEYS[4]) ~= ARGV[5] then return -2 end
if redis.call("GET", KEYS[1]) ~= ARGV[2] then return 0 end
redis.call("SET", KEYS[1], ARGV[3])
return 1
`)
