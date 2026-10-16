package data

import (
	"context"
	"fmt"
	"strconv"

	"github.com/zeromicro/go-zero/core/stores/redis"
	"google.golang.org/protobuf/proto"

	plpb "proto/player_locator"
)

// session_reader.go —— 推送前"收件人在哪个 gate"的唯一读法(docs/design/mail-system.md §4.5)。
//
// MailAdmin 的调用方(运维 / robot)不是收件人,手里没有收件人的会话 metadata;要把 NotifyMailEvent 推到收件人
// 所在的 gate,只能读跨运行时契约 key `player:session:<id>`(契约 §4,与 friend 同一口径):
//
//   - 值是 proto 编码的 plpb.PlayerSession,**写者是 player_locator**,mail 只读,不写、不删、不设 TTL;
//   - 只能经 **SharedRedis** 读(SharedRedis 禁 Cluster:这个 key 没有 hash tag);
//   - 键名不许加 mail 自己的前缀或 hash tag —— 改一个字符就读不到,而且是静默读不到(看起来只是"收件人离线")。
//
// 本文件只负责"读出来、解得开、是这个人";在不在线(只推 SESSION_STATE_ONLINE)、推没推、计什么指标
// 由推送那一层判(internal/logic,M1c)。这里不打日志也不计指标:同一次失败只该被记一次,记在做决定的那一层。

// playerSessionKeyPrefix 见文件头的契约说明:写者是 player_locator,mail 只读。测试与写法共用这一个前缀。
const playerSessionKeyPrefix = "player:session:"

// PlayerSessionKey 返回某名玩家的会话键。
func PlayerSessionKey(playerID uint64) string {
	return playerSessionKeyPrefix + strconv.FormatUint(playerID, 10)
}

// SessionReader 读一名玩家的 player:session。无状态,可被多个请求并发使用。
type SessionReader struct {
	rdb *redis.Redis
}

// NewSessionReader 构造。sharedRdb **必须**是 svc 的 SharedRedis(见文件头契约)。
func NewSessionReader(sharedRdb *redis.Redis) *SessionReader {
	return &SessionReader{rdb: sharedRdb}
}

// Lookup 读一名玩家的会话。
//
//   - (nil, nil):键不存在 = 玩家没登录(player_locator 正常删除,或从未登录)。最常见的分支,不是错误。
//   - (session, nil):解得开且确实是这个人;state 可能是 ONLINE 也可能是 DISCONNECTING,由调用方判。
//   - (nil, err):Redis 故障、解码失败、或会话里的 player_id 与键不符。
//
// 解码失败与身份不符**不当"离线"**:PlayerSession 是别的运行时写的 proto,解不开只可能是键被占用或协议不兼容,
// 当成离线会让一次协议事故表现为"推送全都不推"且零异常(推送层把 offline 与 error 分开计,就是为了这个)。
func (s *SessionReader) Lookup(ctx context.Context, playerID uint64) (*plpb.PlayerSession, error) {
	if s == nil || s.rdb == nil {
		return nil, fmt.Errorf("player session lookup: SharedRedis handle not configured")
	}
	if playerID == 0 {
		return nil, fmt.Errorf("player session lookup: player_id is 0")
	}
	// go-zero 的 GetCtx 吞掉 redis.Nil、返回空串,所以"键不存在"只能靠空串判,真故障一定从 err 出来。
	raw, err := s.rdb.GetCtx(ctx, PlayerSessionKey(playerID))
	if err != nil {
		return nil, fmt.Errorf("get player session %d: %w", playerID, err)
	}
	if raw == "" {
		return nil, nil
	}
	session := &plpb.PlayerSession{}
	if err := proto.Unmarshal([]byte(raw), session); err != nil {
		return nil, fmt.Errorf("unmarshal PlayerSession %d: %w", playerID, err)
	}
	if session.GetPlayerId() != playerID {
		return nil, fmt.Errorf("PlayerSession under key of player %d belongs to player %d", playerID, session.GetPlayerId())
	}
	return session, nil
}
