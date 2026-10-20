package logic

import (
	"context"
	"errors"
	"fmt"
	"strconv"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/metric"

	"shared/placement"
)

// ── 合服闸门(merge:in_progress:{zone})──────────────────────────
//
// 这一份是 data_service 那把闸门的**只读**消费者。契约(真源在
// go/data_service/internal/routing/router.go 顶部,改一处必须同步另一处):
//
//	Redis  : data_service 的 mapping Redis(player:zone:{id} 所在的实例/DB),
//	         guild 侧由 config.MergeMarkerRedis 指过去
//	Key    : merge:in_progress:{zone}   zone = 十进制 zone id
//	Value  : JSON,{"started_at": <RFC3339 UTC>, "started_at_unix_ms": <int>, ...}
//	         —— **从不解析**,只看键在不在
//	TTL    : 由 tools/merge_zone 设定,长于整次合服;进程被杀时的兜底
//	谁清除 : tools/merge_zone 跑完显式 DEL;TTL 到期是兜底
//	判据   : **键存在即封锁**
//
// guild 为什么要看它:合服会把源 zone 的公会行搬到目标 zone,并重建 per-zone 榜。
// 维护窗口里如果还能在源 zone 建帮,那个公会要么在搬迁扫描之后诞生(永远留在
// 一个已下线的 zone,会长登进去看不到自己的会),要么与搬迁并发写同一批行。
// 拦在建帮入口是最便宜的一刀:公会创建是低频操作,拒绝一次的代价只是玩家重试。
//
// 2026-10-18 起(D1 第 3 批读方门禁,docs/handoff/launch-readiness-20261013/spec_D1_batch1b2_2_3.md §5.1 / §5.4):
// db_first 下闸门键只是全局库 zone_merge_fence 的镜像,mapping Redis 重启、切主、从旧备份恢复之后,
// 「键不在」可能只是镜像丢了。MergeMarkerRedis.RequireRouteReady=true 时改用 RouteReadyMergeFence:
// 闸门键与就绪标记、本实例身份在同一段只读 Lua(placement.RouteReadyReadScript)里取回,
// 只有 placement.VerifyRouteReady 判 Trusted 时才信任「键在 / 键不在」,其余一律返回 error
// (调用方既有契约:error = 封锁,回 kGuildZoneMerging)。缺省 false = 下面 RedisMergeFence 的 EXISTS 老路径,行为不变。

// MergeFence 报告某个 zone 是否正处在合服维护窗口。
// 抽成接口是为了让 GuildLogic 在没配 MergeMarkerRedis 时装 nil,
// 以及让单测不必起 Redis。
type MergeFence interface {
	// MergeInProgress 返回 zone 是否被封锁。err 非 nil 时调用方必须按**封锁**处理:
	// 查不到闸门状态就不能证明现在可以写。
	MergeInProgress(ctx context.Context, zoneID uint32) (bool, error)
}

const mergeFenceKeyPrefix = "merge:in_progress:"

// MergeFenceKey 拼合服标记键。与 data_service 的 routing.MergeFenceKey 同一份拼法;
// guild 不 import data_service(两个独立 module),所以这里各留一份并互相指认。
// 与 shared/placement.MergeFenceKey 逐字相同(TestMergeFenceKeyMatchesPlacement 钉住)。
func MergeFenceKey(zoneID uint32) string {
	return mergeFenceKeyPrefix + strconv.FormatUint(uint64(zoneID), 10)
}

// RedisMergeFence 用一个 go-redis 客户端做存在性检查。
type RedisMergeFence struct {
	rdb *redis.Client
}

// NewRedisMergeFence 在 rdb 为 nil 时返回 nil(未配置 = 闸门不生效),
// 这样调用方可以直接把结果塞给 GuildLogic,不必自己判空。
func NewRedisMergeFence(rdb *redis.Client) *RedisMergeFence {
	if rdb == nil {
		return nil
	}
	return &RedisMergeFence{rdb: rdb}
}

// MergeInProgress 只做 EXISTS,不读值、不看 TTL —— 见文件顶部的判据说明。
func (f *RedisMergeFence) MergeInProgress(ctx context.Context, zoneID uint32) (bool, error) {
	if f == nil || f.rdb == nil {
		return false, nil
	}
	if zoneID == 0 {
		return false, nil
	}
	n, err := f.rdb.Exists(ctx, MergeFenceKey(zoneID)).Result()
	if err != nil {
		return false, fmt.Errorf("merge fence check for zone %d: %w", zoneID, err)
	}
	return n > 0, nil
}

// ── RequireRouteReady=true:先判副本可信,再看闸门键 ───────────────

// ErrMergeFenceRouteNotReady 表示 mapping Redis 的副本此刻不可信(就绪标记缺席 / 畸形 / 更新版本写的、
// 实例身份取不到、不是主、或标记记下的身份不是本实例)。此时「闸门键不在」不能证明没有合服,
// MergeInProgress 返回包了它的 error,调用方按封锁处理。
var ErrMergeFenceRouteNotReady = errors.New("merge fence: mapping redis replica is not trusted by the route ready marker")

// RouteReadyReader 是 RouteReadyMergeFence 对 mapping Redis 的全部读法。抽成接口是为了让单测逐个 verdict 注入回复:
// miniredis 不支持 INFO 的 server / replication 段,读脚本的执行语义只能在真 Redis 上测。
type RouteReadyReader interface {
	// ReadRouteReady 执行一次 placement.RouteReadyReadScript 并返回原始回复;keys 按 placement.RouteReadyReadKeys 组装。
	ReadRouteReady(ctx context.Context, keys []string) (any, error)
	// KeyExists 报告 key 是否存在(任何类型)。只在副本已判 Trusted、读脚本对闸门键回了 nil 时调用,
	// 补上 MGET 看不见的「非 string 类型的闸门键」—— 老路径的 EXISTS 对它是封锁,这里不能因为换了读法而放行。
	KeyExists(ctx context.Context, key string) (bool, error)
}

// routeReadyReadScript 用 go-redis 的 Script:Run 先 EVALSHA,脚本不在缓存时(NOSCRIPT)自动退回 EVAL。
var routeReadyReadScript = redis.NewScript(placement.RouteReadyReadScript)

// redisRouteReadyReader 是 RouteReadyReader 的 go-redis 实现。
type redisRouteReadyReader struct {
	rdb *redis.Client
}

func (r redisRouteReadyReader) ReadRouteReady(ctx context.Context, keys []string) (any, error) {
	return routeReadyReadScript.Run(ctx, r.rdb, keys).Result()
}

func (r redisRouteReadyReader) KeyExists(ctx context.Context, key string) (bool, error) {
	n, err := r.rdb.Exists(ctx, key).Result()
	if err != nil {
		return false, err
	}
	return n > 0, nil
}

// RouteReadyMergeFence 是 MergeMarkerRedis.RequireRouteReady=true 时的闸门:
// 一次读脚本取回就绪标记、本实例身份与 merge:in_progress:{zone},按 placement.VerifyRouteReady 判可信后才看键。
//
//   - Trusted:闸门键的值非 nil → 封锁;值为 nil 时再 EXISTS 一次(见 RouteReadyReader.KeyExists),存在即封锁;
//   - 其余 verdict:返回包了 ErrMergeFenceRouteNotReady 的 error(调用方当封锁),值与缺键都不用;
//   - 脚本执行出错 / 回复形状不对:返回 error(调用方当封锁)。
//
// 并发:无可变状态,可被多个请求并发调用。
type RouteReadyMergeFence struct {
	reader RouteReadyReader
	// observe 记一次判定的 verdict label(取值见 routeReadyVerdictLabels);单测替换它来断言 label。
	observe func(label string)
}

// NewRouteReadyMergeFence 在 rdb 为 nil 时返回 nil,与 NewRedisMergeFence 同一约定。
func NewRouteReadyMergeFence(rdb *redis.Client) *RouteReadyMergeFence {
	if rdb == nil {
		return nil
	}
	return newRouteReadyMergeFence(redisRouteReadyReader{rdb: rdb})
}

func newRouteReadyMergeFence(reader RouteReadyReader) *RouteReadyMergeFence {
	return &RouteReadyMergeFence{reader: reader, observe: countRouteReadyVerdict}
}

// MergeInProgress 见 RouteReadyMergeFence 的说明。zone 0 与 nil 接收者的口径与 RedisMergeFence 相同。
func (f *RouteReadyMergeFence) MergeInProgress(ctx context.Context, zoneID uint32) (bool, error) {
	if f == nil || f.reader == nil {
		return false, nil
	}
	if zoneID == 0 {
		return false, nil
	}
	key := placement.MergeFenceKey(zoneID)
	read, err := f.read(ctx, placement.RouteReadyReadKeys(key), 1)
	if err != nil {
		f.observeLabel(routeReadyVerdictReadError)
		return false, fmt.Errorf("merge fence check for zone %d: %w", zoneID, err)
	}
	verdict, marker := placement.VerifyRouteReady(read)
	f.observeLabel(verdict.String())
	if verdict != placement.RouteReadyTrusted {
		return false, fmt.Errorf("merge fence check for zone %d: %w: %s", zoneID, ErrMergeFenceRouteNotReady, describeRouteReady(verdict, marker, read))
	}
	if read.Values[0] != nil {
		return true, nil
	}
	exists, err := f.reader.KeyExists(ctx, key)
	if err != nil {
		return false, fmt.Errorf("merge fence check for zone %d: exists after trusted read: %w", zoneID, err)
	}
	return exists, nil
}

// Probe 是 RequireRouteReady=true 时的启动探针(规格 §13.2 第 2 条):不带映射键试跑一次读脚本。
// error 非 nil = 脚本执行不了或回复形状不对(例如 INFO 在脚本里不可用),guild.go 据此拒启;此时返回的 verdict 无意义
// (固定给 RouteReadyIdentityUnavailable,不给零值 Trusted)。verdict 不是 Trusted 不算错误:标记可能还没挂上,
// 运行期在它变成 Trusted 之前,经闸门的公会写(建帮、入帮、经济、活动等)一律按封锁拒绝。Probe 不计指标(启动期指标开关还没打开)。
func (f *RouteReadyMergeFence) Probe(ctx context.Context) (placement.RouteReadyVerdict, error) {
	if f == nil || f.reader == nil {
		return placement.RouteReadyIdentityUnavailable, errors.New("merge fence route ready probe: fence not configured")
	}
	read, err := f.read(ctx, placement.RouteReadyReadKeys(), 0)
	if err != nil {
		return placement.RouteReadyIdentityUnavailable, fmt.Errorf("merge fence route ready probe: %w", err)
	}
	verdict, _ := placement.VerifyRouteReady(read)
	return verdict, nil
}

// read 执行读脚本并解析回复;nMappingKeys 是 keys 里映射键的个数(keys[0] 是就绪标记)。
func (f *RouteReadyMergeFence) read(ctx context.Context, keys []string, nMappingKeys int) (placement.RouteReadyRead, error) {
	raw, err := f.reader.ReadRouteReady(ctx, keys)
	if err != nil {
		return placement.RouteReadyRead{}, fmt.Errorf("run route ready read script: %w", err)
	}
	read, err := placement.ParseRouteReadyRead(raw, nMappingKeys)
	if err != nil {
		return placement.RouteReadyRead{}, err
	}
	return read, nil
}

func (f *RouteReadyMergeFence) observeLabel(label string) {
	if f.observe != nil {
		f.observe(label)
	}
}

// describeRouteReady 给不可信的判定拼一段诊断(进 error,由调用方的拒绝日志带出)。
// run_id / replid 只取前 8 位:足够分辨「重启 / 切主 / 换实例」,不把整串身份铺进每条日志。
func describeRouteReady(verdict placement.RouteReadyVerdict, marker placement.RouteReadyMarker, read placement.RouteReadyRead) string {
	id := read.Identity
	s := fmt.Sprintf("verdict=%s role=%q instance_run_id=%s instance_replid=%s",
		verdict, id.Role, shortRedisID(id.RunID), shortRedisID(id.MasterReplID))
	if marker.RedisRunID != "" {
		s += fmt.Sprintf(" marker_run_id=%s marker_replid=%s marker_generation=%s",
			shortRedisID(marker.RedisRunID), shortRedisID(marker.MasterReplID), marker.Generation)
	}
	return s
}

func shortRedisID(s string) string {
	const n = 8
	if len(s) > n {
		return s[:n]
	}
	return s
}

// ── 指标 guild_route_ready_verdict_total{verdict} ──────────────────

// routeReadyVerdictReadError 是读脚本执行出错或回复形状不对时的 label(没有得到 verdict)。
const routeReadyVerdictReadError = "read_error"

// guildRouteReadyVerdictTotal:RequireRouteReady=true 时每次闸门判定计一次。label 是封闭集合
// (placement.RouteReadyVerdictNames() + read_error),不放 zone / player_id。
// 写法与 activity_metrics.go 相同(go-zero core/metric,与 guild 其余指标同一套注册)。
var guildRouteReadyVerdictTotal = metric.NewCounterVec(&metric.CounterVecOpts{
	Namespace: "guild",
	Subsystem: "route_ready",
	Name:      "verdict_total",
	Help: "MergeMarkerRedis.RequireRouteReady=true 时合服闸门每次读到的 mapping Redis 副本判定。" +
		"verdict: trusted(按闸门键判)/ absent / malformed / newer_version / identity_unavailable / not_master / identity_mismatch" +
		"(副本不可信,经闸门的公会写按封锁拒绝)/ read_error(读脚本执行出错或回复形状不对,同样拒绝)。非 trusted 持续增长 = 合服闸门在 fail-closed。",
	Labels: []string{"verdict"},
})

// routeReadyVerdictLabels 是 verdict label 的全集,PrimeRouteReadyMetrics 逐个预置。
func routeReadyVerdictLabels() []string {
	return append(placement.RouteReadyVerdictNames(), routeReadyVerdictReadError)
}

func countRouteReadyVerdict(label string) {
	guildRouteReadyVerdictTotal.Inc(label)
}

// PrimeRouteReadyMetrics 把 verdict 的全部取值预置为 0,让「出现即告警」的 increase(...) 不漏第一次。
// 只在 RequireRouteReady=true 时调;与 server.PrimeGuildInternalMetrics 同理,**必须在 zrpc.MustNewServer 之后调**。
func PrimeRouteReadyMetrics() {
	for _, label := range routeReadyVerdictLabels() {
		guildRouteReadyVerdictTotal.Add(0, label)
	}
}
