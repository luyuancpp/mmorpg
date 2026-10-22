// Package loginsession manages login session state in Redis.
//
// 登录态按「gate 连接」记,一条连接一个键:
//
//	login_session:{gate_instance_id}:{session_incarnation} → account   (带会话代次的 gate)
//	login_session:{session_id}                             → account   (旧版 gate,兼容路径)
//
// 为什么不再只用 session_id 建键(docs/design/data-overwrite-rollback-audit-20261018.md §2.3、
// client-request-ordering.md §7.2 第 2 条):session_id = node_id<<17 | 进程内序号,gate 被 kill -9 后
// 以同一个 node_id 重启、序号从 0 开始,新连接会撞上旧进程留下、TTL 内仍有效的 login_session。
// 跳过 Login 直接发 CreatePlayer / EnterGame 的客户端于是继承了别人的账号。gate_instance_id 是每次进程启动
// 生成的 UUID,session_incarnation 在进程内单调,两者合起来全局唯一 —— 与会话栅栏(sessionfence)同一个身份,
// 拼法也只有 sessionfence.Key.String 一处。
//
// 账号设备集(constants.GenerateSessionKey)的成员就是键后缀(Ref.DeviceMember),所以「成员 m 是否还在线」
// 一律看 login_session:{m} 是否存在,两种键形状共用同一套自愈逻辑(PruneStaleFromDeviceSet)。
//
// Operations:
//
//	RefOf:      从 gate 带来的 SessionDetails 取本连接的登录态身份。
//	Save:       旧版 gate 路径写登录态(带代次的连接由 sessionfence.CommitLogin 在同一段脚本里写)。
//	GetAccount: 读本连接的账号;带代次的连接**只认**新键,绝不回落到按 session_id 的旧键。
//	Cleanup:    删本连接的登录态并移出设备集。
package loginsession

import (
	"context"
	"errors"
	"strconv"
	"time"

	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/sessionfence"
	login_proto_common "proto/common/base"
	sharedsession "shared/loginsession"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

const keyPrefix = "login_session:"

// cleanupMissTotal counts Cleanup() calls where the session key was already
// absent in Redis (legitimate race: Disconnect arrived before BindSession
// committed, or a duplicate Disconnect). Surfaced as a metric so the previous
// Error-level spam can stay at Debug without losing observability.
var cleanupMissTotal = promauto.NewCounterVec(prometheus.CounterOpts{
	Name: "loginsession_cleanup_miss_total",
	Help: "Cleanup() called but session key absent (Disconnect/BindSession race).",
}, []string{"caller"})

// legacyKeyTotal 计数仍按 session_id 建键的旧版 gate 请求(没有会话代次)。这条路径保留了撞号继承登录态的旧风险,
// 计数长期为 0 之后才能删掉兼容路径与新路径上的旧键双写(见 Ref.WriteKeys)。op = save / get / cleanup。
var legacyKeyTotal = promauto.NewCounterVec(prometheus.CounterOpts{
	Name: "loginsession_legacy_key_total",
	Help: "login_session operations keyed by legacy session_id (gate without session_incarnation).",
}, []string{"op"})

// Ref 是一条 gate 连接在 login_session 里的身份,由 RefOf 构造;零值不可用。
type Ref struct {
	// id 是键后缀,同时是账号设备集里的成员:带代次时为 "{gate_instance_id}:{incarnation}",旧版 gate 为 session_id。
	id string
	// sessionID 只用于新路径上的旧键双写与清理。
	sessionID uint32
	legacy    bool
}

// RefOf 从 gate 带来的 SessionDetails 取本连接的登录态身份。带 (gate_instance_id, session_incarnation) 时用新键;
// 旧版 gate(代次为 0 或缺实例号)回落到按 session_id 的旧键并计数。判定与会话栅栏共用 sessionfence.KeyOf,
// 两者对「这条连接是否带代次」永远一致。
func RefOf(details *login_proto_common.SessionDetails) Ref {
	sessionID := details.GetSessionId()
	if fenceKey, ok := sessionfence.KeyOf(details); ok {
		return Ref{id: fenceKey.String(), sessionID: sessionID}
	}
	return Ref{id: strconv.FormatUint(uint64(sessionID), 10), sessionID: sessionID, legacy: true}
}

// Legacy 表示这条连接来自没有会话代次的旧版 gate,登录态按 session_id 建键。
func (r Ref) Legacy() bool {
	return r.legacy
}

// Key 返回本连接 login_session 的 Redis 键;读登录态只认这一个键。
func (r Ref) Key() string {
	return sharedsession.KeyFromMember(r.id)
}

// DeviceMember 返回本连接在账号设备集里的成员值(= 键后缀)。
func (r Ref) DeviceMember() string {
	return r.id
}

// WriteKeys 返回登录时要写、清理时要删的全部键。
//
// 带代次的连接除新键外**双写**一份按 session_id 的旧键,只为滚动升级与回滚:新旧 login 并存时,旧 login 仍按
// session_id 读登录态,不双写的话「新 login 受理 Login、旧 login 受理 EnterGame」的连接会被误判未登录;
// login 回滚到旧版时,已经在选角界面的玩家也不会被踢回登录。新 login 读带代次的连接时从不看旧键,所以双写
// 不会把撞号风险带回新路径;旧键只对旧 login 与旧版 gate 可见,那两处本来就有这个风险。
// 全部 login 升级且 loginsession_legacy_key_total 长期为 0 之后,删除双写与旧版 gate 兼容路径。
func (r Ref) WriteKeys() []string {
	if r.legacy || r.sessionID == 0 {
		return []string{r.Key()}
	}
	return []string{r.Key(), keyPrefix + strconv.FormatUint(uint64(r.sessionID), 10)}
}

func sessionTTL() time.Duration {
	return time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute
}

// TTL 返回 login_session 的寿命(Node.SessionExpireMin)。导出给 sessionfence.CommitLogin 在同一段脚本里写入。
func TTL() time.Duration {
	return sessionTTL()
}

// Save 写本连接的登录态(覆盖写,同账号由账号锁保护)。生产上只有旧版 gate 路径调用;
// 带代次的连接由 sessionfence.CommitLogin 原子地写 WriteKeys 与设备集。
func Save(ctx context.Context, rdb *redis.Client, ref Ref, account string) error {
	if ref.legacy {
		legacyKeyTotal.WithLabelValues("save").Inc()
	}
	return rdb.Set(ctx, ref.Key(), account, sessionTTL()).Err()
}

// PruneStaleFromDeviceSet scans the per-account device set and removes any
// members whose backing login_session:{member} key no longer exists. Returns
// the number of entries removed. 成员可能是两种形状(新键后缀或旧版 session_id),
// 都按 keyPrefix + 成员 判断,见包注释。
//
// This is a best-effort self-heal called from Login to recover from the
// race where Gate's socket-closed → login.Disconnect RPC is still in
// flight when the same account reconnects — without it, a heavy-reconnect
// workload (stress robot) can trip MaxDevicesPerAccount on stale entries
// that Cleanup never got around to removing.
func PruneStaleFromDeviceSet(ctx context.Context, rdb *redis.Client, deviceSetKey string) int {
	members, err := rdb.SMembers(ctx, deviceSetKey).Result()
	if err != nil || len(members) == 0 {
		return 0
	}
	stale := make([]interface{}, 0, len(members))
	for _, member := range members {
		exists, err := rdb.Exists(ctx, keyPrefix+member).Result()
		if err != nil {
			continue
		}
		if exists == 0 {
			stale = append(stale, member)
		}
	}
	if len(stale) == 0 {
		return 0
	}
	if err := rdb.SRem(ctx, deviceSetKey, stale...).Err(); err != nil {
		logx.Errorf("PruneStaleFromDeviceSet SRem failed: key=%s err=%v", deviceSetKey, err)
		return 0
	}
	logx.Infof("Pruned %d stale sessions from device set %s", len(stale), deviceSetKey)
	return len(stale)
}

// GetAccount 返回本连接登录的账号;本连接没有成功 Login 过(或登录态已过期 / 已清理)时返回 redis.Nil。
//
// 带代次的连接只读 Ref.Key(),**不回落**到按 session_id 的旧键:旧键可能是同一 node_id 上一个 gate 进程留下的,
// 回落就是撞号继承登录态本身。滚动升级窗口里「旧 login 受理 Login、新 login 受理 EnterGame」的连接因此会被判
// 未登录、需要重登 —— 这是有意的 fail-closed,见 Ref.WriteKeys 与 client-request-ordering.md §7.2 第 2 条。
func GetAccount(ctx context.Context, rdb *redis.Client, ref Ref) (string, error) {
	if ref.legacy {
		legacyKeyTotal.WithLabelValues("get").Inc()
	}
	return rdb.Get(ctx, ref.Key()).Result()
}

// Cleanup removes the connection's login state (all of Ref.WriteKeys) and its
// device-set entry.
//
// `redis: nil` here is a legitimate race (Disconnect arriving before BindSession
// committed, or a second Cleanup for the same connection). Round 14 demotes that
// case to Debug and surfaces it via a Prometheus counter so the login ERROR log
// is no longer flooded under high reconnect churn (45k-bot stress saw tens of
// thousands of these per minute, drowning out real errors). Real Redis errors
// (connection drop, command timeout) stay at Error level.
//
// 新路径同时删双写的旧键:它与本连接同 session_id,删掉只会收掉一个撞号攻击面。滚动升级窗口里由旧 login 写下、
// 新 login 清理的连接,设备集里那条旧成员留给下次登录的 PruneStaleFromDeviceSet 自愈(旧键已删,成员即被判失效)。
func Cleanup(ctx context.Context, rdb *redis.Client, ref Ref, logicTag string) {
	if ref.legacy {
		legacyKeyTotal.WithLabelValues("cleanup").Inc()
	}
	key := ref.Key()
	account, err := rdb.Get(ctx, key).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			cleanupMissTotal.WithLabelValues(logicTag).Inc()
			logx.Debugf("[%s] cleanup miss (session not in redis, expected race): session=%s", logicTag, ref.id)
		} else {
			logx.Errorf("[%s] get account failed for cleanup: session=%s err=%v", logicTag, ref.id, err)
		}
	} else {
		rdb.SRem(ctx, constants.GenerateSessionKey(account), ref.DeviceMember())
	}
	rdb.Del(ctx, ref.WriteKeys()...)
	logx.Infof("[%s] session cleanup: session=%s", logicTag, ref.id)
}
