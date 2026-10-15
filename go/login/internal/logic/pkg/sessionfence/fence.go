// Package sessionfence 是 login 的 gate 连接会话栅栏(docs/design/client-request-ordering.md §6.2 / O12)。
//
// 它要关掉的是三种与「gate 连接已关闭 / 已离开游戏」交错的竞态,gate 侧的排队都管不住:
//
//   - Disconnect 抢跑仍在途的 Login:Login 晚落地,留下没人清理的 login_session 与设备集条目;
//   - EnterGame 已受理(回包只表示受理)、后台链还没跑完时断线:gate 合成的 Disconnect 带 player_id = 0,
//     原先在 playerID == 0 处早退;后台链随后写入 ONLINE 会话,却没有任何租约,玩家永久卡在线。
//     这一条**顺序正确也会发生**;
//   - EnterGame 之后的 LeaveGame:LeaveGame 不拿玩家锁,与后台链并发,后台链在 LeaveGame 之后又把会话与位置写回去;
//     同一条连接连发两次 EnterGame(换角 / 重试),两条后台链都跑完,一个会话绑了两个角色。
//
// 记录是一个 Redis hash,键按「gate 连接」建:gate_instance_id(每次 gate 进程启动生成的 UUID)+
// session_incarnation(gate 进程内单调、不回绕)。不按 session_id 建键:它在 gate 重启后从头发号、17 位序号也会
// 回绕,上一个进程留下的关闭标记会误拒新连接。字段:
//
//	closed  "1" = 这条连接已断开(Disconnect 写)
//	gen     LeaveGame 的次数(LeaveGame 递增);EnterGame 入口记下当时的值,后台链据此识别「期间已离开游戏」
//	player  这条连接上已绑定的玩家(后台链绑定时写,LeaveGame 删)
//
// 正确性靠两段原子脚本的「双边握手」:Close 先写 closed、再读 player;BindPlayer 先查 closed、再写 player。
// 两段都在同一个 Redis 上原子执行,天然可串行化:关闭在前,绑定一定看到 closed,由后台链补发断线租约;
// 绑定在前,关闭一定读到 player,由 Disconnect 补发。两种顺序都收敛到「带租约的 DISCONNECTING」。
//
// 约束:CommitLogin 是多键脚本(栅栏 + login_session + 设备集),要求 login 的 Redis 是单节点(现状是
// *redis.Client)。将来切 Redis Cluster 时必须给这三个键加同一个 hash tag。
package sessionfence

import (
	"context"
	"errors"
	"strconv"
	"time"

	login_proto_common "proto/common/base"

	"github.com/redis/go-redis/v9"
)

// Key 标识一条 gate 连接。
type Key struct {
	GateInstanceID string
	Incarnation    uint64
}

// KeyOf 从 gate 带来的 SessionDetails 取栅栏键。旧版 gate(session_incarnation = 0)或缺 gate_instance_id 时
// 返回 ok = false,调用方跳过栅栏、沿用旧行为(兼容窗口,client-request-ordering.md §6.2)。
func KeyOf(details *login_proto_common.SessionDetails) (Key, bool) {
	if details == nil || details.GetSessionIncarnation() == 0 || details.GetGateInstanceId() == "" {
		return Key{}, false
	}
	return Key{GateInstanceID: details.GetGateInstanceId(), Incarnation: details.GetSessionIncarnation()}, true
}

func (k Key) redisKey() string {
	return "login:sessfence:" + k.GateInstanceID + ":" + strconv.FormatUint(k.Incarnation, 10)
}

// TTLFor 由 EnterGame 后台链的总预算派生栅栏记录的寿命:记录必须比所有还可能读它的操作活得久,
// 最长的是后台链本身;多留 1 分钟吸收时钟与调度误差。不用 SessionExpireMin(那是登录态的寿命,不是在途操作的寿命)。
func TTLFor(enterGameChainBudget time.Duration) time.Duration {
	return enterGameChainBudget + time.Minute
}

// Outcome 是栅栏判定的结果。
type Outcome uint8

const (
	// OutcomeOK:连接仍在、没有离开过游戏、没有绑定别的角色。
	OutcomeOK Outcome = iota
	// OutcomeClosed:连接已断开。
	OutcomeClosed
	// OutcomeLeft:EnterGame 入口之后发生过 LeaveGame。
	OutcomeLeft
	// OutcomeOtherPlayer:这条连接上已经绑定了另一个角色。
	OutcomeOtherPlayer
)

func (o Outcome) String() string {
	switch o {
	case OutcomeOK:
		return "ok"
	case OutcomeClosed:
		return "closed"
	case OutcomeLeft:
		return "left"
	case OutcomeOtherPlayer:
		return "other_player"
	}
	return "unknown"
}

// ErrClosed:CommitLogin 时连接已断开,Login 不写任何状态。
var ErrClosed = errors.New("gate connection already closed")

var commitLoginScript = redis.NewScript(`
if redis.call('HGET', KEYS[1], 'closed') == '1' then
  return 0
end
redis.call('SET', KEYS[2], ARGV[1], 'PX', ARGV[2])
redis.call('SADD', KEYS[3], ARGV[3])
redis.call('PEXPIRE', KEYS[3], ARGV[4])
return 1
`)

// CommitLogin 原子地「检查连接未关闭 → 写 login_session → 登记设备集」,取代原先 Save 与 SAdd/Expire 两步。
// 检查与写入必须在同一段脚本里,否则仍有「检查通过、Disconnect 插进来、再写入」的窗口。
// 连接已关闭返回 ErrClosed,什么都不写。
//
// loginSessionKey / deviceSetKey 由调用方给(与 loginsession / constants.GenerateSessionKey 的键格式同源),
// 这里不复制键格式。
func CommitLogin(ctx context.Context, rdb *redis.Client, key Key,
	loginSessionKey, account string, sessionTTL time.Duration,
	deviceSetKey string, sessionID uint32, deviceSetTTL time.Duration,
) error {
	res, err := commitLoginScript.Run(ctx, rdb,
		[]string{key.redisKey(), loginSessionKey, deviceSetKey},
		account, sessionTTL.Milliseconds(), strconv.FormatUint(uint64(sessionID), 10), deviceSetTTL.Milliseconds(),
	).Int64()
	if err != nil {
		return err
	}
	if res == 0 {
		return ErrClosed
	}
	return nil
}

var closeScript = redis.NewScript(`
redis.call('HSET', KEYS[1], 'closed', '1')
redis.call('PEXPIRE', KEYS[1], ARGV[1])
local p = redis.call('HGET', KEYS[1], 'player')
if p then
  return p
end
return '0'
`)

// Close 标记连接已断开,并返回这条连接上已绑定的玩家(没有为 0)。Disconnect 在清理 login_session 之前调用;
// 会话里的 player_id 为 0(BindSession 还没到 gate)时,用这里返回的玩家补发断线租约。
func Close(ctx context.Context, rdb *redis.Client, key Key, ttl time.Duration) (uint64, error) {
	raw, err := closeScript.Run(ctx, rdb, []string{key.redisKey()}, ttl.Milliseconds()).Text()
	if err != nil {
		return 0, err
	}
	return parsePlayer(raw)
}

// CheckEnterGame 是 EnterGame 入口的判定:连接已关闭、或已经绑定了别的角色就拒绝;否则返回当前的 gen,
// 后台链绑定时用它识别「期间已离开游戏」。
func CheckEnterGame(ctx context.Context, rdb *redis.Client, key Key, playerID uint64) (Outcome, uint64, error) {
	vals, err := rdb.HMGet(ctx, key.redisKey(), "closed", "gen", "player").Result()
	if err != nil {
		return OutcomeOK, 0, err
	}
	if asString(vals[0]) == "1" {
		return OutcomeClosed, 0, nil
	}
	gen, err := parseCounter(asString(vals[1]))
	if err != nil {
		return OutcomeOK, 0, err
	}
	bound, err := parsePlayer(asString(vals[2]))
	if err != nil {
		return OutcomeOK, 0, err
	}
	if bound != 0 && bound != playerID {
		return OutcomeOtherPlayer, gen, nil
	}
	return OutcomeOK, gen, nil
}

var bindPlayerScript = redis.NewScript(`
local v = redis.call('HMGET', KEYS[1], 'closed', 'gen', 'player')
if v[1] == '1' then
  return 'closed'
end
local gen = v[2] or '0'
if gen ~= ARGV[1] then
  return 'left'
end
if v[3] and v[3] ~= ARGV[2] then
  return 'other_player'
end
redis.call('HSET', KEYS[1], 'player', ARGV[2])
redis.call('PEXPIRE', KEYS[1], ARGV[3])
return 'ok'
`)

// BindPlayer 是后台链会话持久化成功之后、向 gate 发 BindSession 之前的绑定点:先查 closed 再写 player
// (与 Close 构成双边握手)。返回 OutcomeOK 才算绑定成功;其余结果由调用方补偿:
//
//	OutcomeClosed:连接已断开 → 补发断线租约(markPlayerSessionDisconnecting)后中止;
//	OutcomeLeft / OutcomeOtherPlayer → 删除本次写入的会话(MarkOffline CAS)后中止。
func BindPlayer(ctx context.Context, rdb *redis.Client, key Key, playerID, expectGen uint64, ttl time.Duration) (Outcome, error) {
	raw, err := bindPlayerScript.Run(ctx, rdb, []string{key.redisKey()},
		strconv.FormatUint(expectGen, 10), strconv.FormatUint(playerID, 10), ttl.Milliseconds(),
	).Text()
	if err != nil {
		return OutcomeOK, err
	}
	return parseOutcome(raw), nil
}

// Recheck 是后台链发 EnterScene 之前的复查:连接已关闭或期间离开过游戏就不再进场,
// 防止在 DISCONNECTING / 已离线之后再写入位置。
func Recheck(ctx context.Context, rdb *redis.Client, key Key, expectGen uint64) (Outcome, error) {
	vals, err := rdb.HMGet(ctx, key.redisKey(), "closed", "gen").Result()
	if err != nil {
		return OutcomeOK, err
	}
	if asString(vals[0]) == "1" {
		return OutcomeClosed, nil
	}
	gen, err := parseCounter(asString(vals[1]))
	if err != nil {
		return OutcomeOK, err
	}
	if gen != expectGen {
		return OutcomeLeft, nil
	}
	return OutcomeOK, nil
}

var leaveScript = redis.NewScript(`
redis.call('HINCRBY', KEYS[1], 'gen', 1)
local p = redis.call('HGET', KEYS[1], 'player')
redis.call('HDEL', KEYS[1], 'player')
redis.call('PEXPIRE', KEYS[1], ARGV[1])
if p then
  return p
end
return '0'
`)

// Leave 记一次离开游戏(gen + 1)并解除绑定,返回原先绑定的玩家(没有为 0)。LeaveGame 在清理会话之前调用;
// 会话里的 player_id 为 0 时,用这里返回的玩家删除会话。
func Leave(ctx context.Context, rdb *redis.Client, key Key, ttl time.Duration) (uint64, error) {
	raw, err := leaveScript.Run(ctx, rdb, []string{key.redisKey()}, ttl.Milliseconds()).Text()
	if err != nil {
		return 0, err
	}
	return parsePlayer(raw)
}

func asString(v any) string {
	s, _ := v.(string)
	return s
}

func parseCounter(raw string) (uint64, error) {
	if raw == "" {
		return 0, nil
	}
	return strconv.ParseUint(raw, 10, 64)
}

func parsePlayer(raw string) (uint64, error) {
	if raw == "" || raw == "0" {
		return 0, nil
	}
	return strconv.ParseUint(raw, 10, 64)
}

func parseOutcome(raw string) Outcome {
	switch raw {
	case "ok":
		return OutcomeOK
	case "closed":
		return OutcomeClosed
	case "left":
		return OutcomeLeft
	case "other_player":
		return OutcomeOtherPlayer
	}
	// 脚本只会回上面四种;回别的说明脚本被改坏了,按最保守的「已关闭」处理(fail-closed)。
	return OutcomeClosed
}
