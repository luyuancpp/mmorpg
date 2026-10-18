package routetool

import (
	"context"
	"errors"
	"fmt"
	"io"
	"strings"
	"time"

	"data_service/internal/store"

	"shared/placement"

	goredis "github.com/redis/go-redis/v9"
)

// freeze / unfreeze(第 1b-1 批,2026-10-18):全局库回档恢复流程 freeze → reconcile-from-cache → audit -frozen-recovery → unfreeze
// 的首尾两步(docs/design/player-route-mapping-durability-proposal.md §11.2「第 1b 批」、runbook §9.4)。
//
// freeze 只碰全局库(route_meta.writable = 0),不连 mapping Redis。
//
// unfreeze 在翻回 writable = 1 之前要确认 mapping Redis 是**可信的超集**:冻结期间 Redis 若重启 / 切主 / 换实例(从旧备份恢复),
// 库与缓存可能同时缺同一批登记,而 audit 看不见两边都缺的行;解冻之后读方(第 3 批)会把它们当成权威缺席。确认的依据只有
// 就绪标记(placement.RouteReadyMarker):存在、可解码,且标记里的 run_id / master_replid 等于当前实例。确认不了一律退出码 2,
// 只有显式 -cache-was-not-verified-acknowledged 才放行 —— 这时要先按 runbook §9.2 的清单重放,补上两边都可能缺的合服与搬库。
// **第 1b-2 批之前没有任何写者挂标记**,所以本批的 unfreeze 一律要走这条显式确认。
//
// 标记的处置(fail-closed 的方向是「撤掉信任」):畸形 → 按原值 compare-and-delete;版本号比本二进制新 → 只记日志、不删也不信任;
// 不是 string 类型 → 读方 MGET 看到的是 nil(等同缺席),不动它;身份不符 → 不信任,显式确认放行时按原值撤掉;
// 可信但 generation 与库不等 → 先 compare-and-set 成库的 generation 再解冻(第 1b-2 批的 leader 据 generation 判库被重置,
// 不对齐的话解冻之后它会立刻再冻结),CAS 输了退出码 1、重跑即可。

// readyMarkerRedis 是 unfreeze 对 mapping Redis 的依赖(就绪标记的读与按原值改 / 删)。go-zero 的 *redis.Redis 天然满足。
// GetCtx 的约定(go-zero):键不在回 ("", nil)。
type readyMarkerRedis interface {
	GetCtx(ctx context.Context, key string) (string, error)
	EvalCtx(ctx context.Context, script string, keys []string, args ...any) (any, error)
}

// freezeDB / unfreezeDB 是两个子命令对全局库的依赖,*store.RouteStore 天然满足。
type freezeDB interface {
	FreezeRouteWriters(ctx context.Context) (bool, error)
}

type unfreezeDB interface {
	ReadRouteMeta(ctx context.Context) (store.RouteMeta, error)
	UnfreezeRouteWriters(ctx context.Context, minWatermarkMs uint64) (bool, error)
}

// RedisIdentity 是 mapping Redis 实例的身份:INFO server 的 run_id 与 INFO replication 的 master_replid。
// 两者都是 Redis 生成的 40 个十六进制字符;任一变化都说明实例重启、切主或换了一个,此前挂的标记不再可信。
type RedisIdentity struct {
	RunID        string
	MasterReplID string
}

// ParseRedisIdentity 从 INFO server 与 INFO replication 的原文里取身份(行尾 CRLF 或 LF 都认)。
// 任一字段缺席或不是 40 个小写十六进制字符都报错:身份读不准时宁可确认不了(fail-closed)。
func ParseRedisIdentity(infoServer, infoReplication string) (RedisIdentity, error) {
	runID, err := infoField(infoServer, "run_id")
	if err != nil {
		return RedisIdentity{}, err
	}
	replID, err := infoField(infoReplication, "master_replid")
	if err != nil {
		return RedisIdentity{}, err
	}
	return RedisIdentity{RunID: runID, MasterReplID: replID}, nil
}

// infoField 取 INFO 文本里 name:value 那一行的值。
func infoField(info, name string) (string, error) {
	for _, line := range strings.Split(info, "\n") {
		v, ok := strings.CutPrefix(strings.TrimRight(line, "\r"), name+":")
		if !ok {
			continue
		}
		if !redisIDOK(v) {
			return "", fmt.Errorf("INFO %s=%q is not 40 lowercase hex characters", name, v)
		}
		return v, nil
	}
	return "", fmt.Errorf("INFO has no %s field", name)
}

func redisIDOK(s string) bool {
	if len(s) != 40 {
		return false
	}
	for i := 0; i < len(s); i++ {
		if c := s[i]; (c < '0' || c > '9') && (c < 'a' || c > 'f') {
			return false
		}
	}
	return true
}

// ── freeze ──────────────────────────────────────────────────────────────────

// FreezeOptions 是 freeze 的参数。
type FreezeOptions struct {
	// Reason 只打到 stderr(route_meta 没有记原因的列),给事后对日志。
	Reason string
	// Timeout 是冻结写本身的时限,至少 store.RouteFreezeTimeoutMin(见它的注释)。
	Timeout time.Duration
	// RedisMode:yaml 是 RouteStore.Mode=redis 时为 true —— 那时 W1 不读 writable,冻结只拦导入与反向修库,打 WARN。
	RedisMode bool
	// Target 非空时作为 stderr 第一行。
	Target string
	// Stdout 只写一行统计;Stderr 写说明与告警。
	Stdout, Stderr io.Writer
}

// FreezeReport 是一次 freeze 的结果。
type FreezeReport struct {
	// Changed false = 本来就是冻结的。
	Changed bool
}

// Freeze 冻结映射写者。成功时 stdout 一行 `route_freeze changed=… writable=0`;出错时退出码 2、不出统计行。
func Freeze(ctx context.Context, db freezeDB, opts FreezeOptions) (FreezeReport, error) {
	if opts.Stdout == nil || opts.Stderr == nil {
		return FreezeReport{}, errors.New("stdout / stderr writers are required")
	}
	if opts.Timeout < store.RouteFreezeTimeoutMin {
		return FreezeReport{}, fmt.Errorf("freeze timeout %s is below %s", opts.Timeout, store.RouteFreezeTimeoutMin)
	}
	if opts.Target != "" {
		fmt.Fprintln(opts.Stderr, opts.Target)
	}
	if opts.RedisMode {
		fmt.Fprintln(opts.Stderr, "WARN: RouteStore.Mode=redis: W1 (RegisterPlayerZone) does not read route_meta.writable in redis mode; "+
			"freezing only blocks route_tool import and the DB repair")
	}
	fmt.Fprintf(opts.Stderr, "FREEZE reason=%q\n", opts.Reason)
	wctx, cancel := context.WithTimeout(ctx, opts.Timeout)
	defer cancel()
	changed, err := db.FreezeRouteWriters(wctx)
	if err != nil {
		return FreezeReport{}, err
	}
	// W1 / 导入对开关是普通读:已经过了检查的事务仍会提交。反向修库开始前会再等 -freeze-grace。
	fmt.Fprintln(opts.Stderr, "NOTE: route writers that already passed the writable check may still commit for ~2s; "+
		"reconcile-from-cache waits -freeze-grace before scanning")
	fmt.Fprintf(opts.Stdout, "route_freeze changed=%t writable=0\n", changed)
	return FreezeReport{Changed: changed}, nil
}

// FreezeExitCode:出错 2,否则 0(含本来就冻结)。
func FreezeExitCode(_ FreezeReport, err error) int {
	if err != nil {
		return ExitFailed
	}
	return ExitClean
}

// ── unfreeze ────────────────────────────────────────────────────────────────

// 就绪标记的状态(UnfreezeReport.Marker 的取值,只进统计行与日志)。
const (
	MarkerVerified         = "verified"
	MarkerAbsent           = "absent"
	MarkerMalformedDropped = "malformed_dropped"
	MarkerNewerVersion     = "newer_version"
	MarkerWrongType        = "wrong_type"
	MarkerIdentityMismatch = "identity_mismatch"
)

// ErrCacheNotVerified 表示 unfreeze 确认不了 mapping Redis 是可信的超集(没有可信的就绪标记),拒绝解冻(退出码 2)。
var ErrCacheNotVerified = errors.New("route_tool unfreeze: the mapping cache is not verified by a ready marker matching the current Redis instance")

// ErrReadyMarkerChanged 表示 unfreeze 按原值改 / 删就绪标记时 CAS 输了:读到之后有人改过它。重跑即可(退出码 1)。
var ErrReadyMarkerChanged = errors.New("route_tool unfreeze: mapping:route:ready changed while unfreezing (compare-and-swap lost); rerun")

// UnfreezeOptions 是 unfreeze 的参数。
type UnfreezeOptions struct {
	// CacheUnverifiedAcknowledged:没有可信标记时仍解冻(-cache-was-not-verified-acknowledged)。见文件头。
	CacheUnverifiedAcknowledged bool
	// Timeout 是解冻事务本身的时限,至少 store.RouteFreezeTimeoutMin。
	Timeout time.Duration
	// Identity 读当前 mapping Redis 的身份(装配层用 INFO 实现;单测注入)。只在标记可解码时才调。
	Identity func(ctx context.Context) (RedisIdentity, error)
	// Target 非空时作为 stderr 第一行。
	Target string
	// Stdout 只写一行统计;Stderr 写判定过程与告警。
	Stdout, Stderr io.Writer
}

// UnfreezeReport 是一次 unfreeze 的结果。
type UnfreezeReport struct {
	// Changed true = 本次把 writable 翻回了 1。
	Changed bool
	// AlreadyWritable:开始时就已可写,什么都没做。
	AlreadyWritable bool
	// Marker 是就绪标记的状态(Marker* 常量之一;AlreadyWritable 时为空)。
	Marker string
	// GenerationRealigned:可信标记的 generation 与库不等,已 CAS 成库的值。
	GenerationRealigned bool
	// MinWatermarkMs 是交给 UnfreezeRouteWriters 的最低水位(可信标记的 watermark_ms;否则 0)。
	MinWatermarkMs uint64
}

// SummaryLine 是 stdout 唯一的一行。
func (r UnfreezeReport) SummaryLine() string {
	return fmt.Sprintf("route_unfreeze changed=%t already_writable=%t marker=%s generation_realigned=%t min_watermark_ms=%d",
		r.Changed, r.AlreadyWritable, r.Marker, r.GenerationRealigned, r.MinWatermarkMs)
}

// UnfreezeExitCode:CAS 输了 1(重跑);其它错误 2(含确认不了缓存);否则 0。
func UnfreezeExitCode(_ UnfreezeReport, err error) int {
	switch {
	case errors.Is(err, ErrReadyMarkerChanged):
		return ExitAttention
	case err != nil:
		return ExitFailed
	default:
		return ExitClean
	}
}

// Unfreeze 解冻映射写者(流程见文件头)。调用方(routetoolcli)负责 -confirm-audited 与 db_first 的门禁。
// 只有翻成功或本来就可写才往 Stdout 写统计行。
func Unfreeze(ctx context.Context, rd readyMarkerRedis, db unfreezeDB, opts UnfreezeOptions) (UnfreezeReport, error) {
	if opts.Stdout == nil || opts.Stderr == nil || opts.Identity == nil {
		return UnfreezeReport{}, errors.New("stdout / stderr writers and the identity reader are required")
	}
	if opts.Timeout < store.RouteFreezeTimeoutMin {
		return UnfreezeReport{}, fmt.Errorf("unfreeze timeout %s is below %s", opts.Timeout, store.RouteFreezeTimeoutMin)
	}
	if opts.Target != "" {
		fmt.Fprintln(opts.Stderr, opts.Target)
	}
	var rep UnfreezeReport

	meta, err := db.ReadRouteMeta(ctx)
	if err != nil {
		return rep, fmt.Errorf("read route_meta: %w", err)
	}
	if !meta.Frozen() {
		rep.AlreadyWritable = true
		fmt.Fprintln(opts.Stderr, "route writers are already writable; nothing to do")
		fmt.Fprintln(opts.Stdout, rep.SummaryLine())
		return rep, nil
	}

	raw, marker, state, err := readReadyMarker(ctx, rd, opts.Identity)
	if err != nil {
		return rep, err
	}
	rep.Marker = state
	if state == MarkerMalformedDropped {
		fmt.Fprintf(opts.Stderr, "WARN: %s held a malformed value %q; dropped it by compare-and-delete\n", placement.RouteReadyKey, raw)
	}

	if state != MarkerVerified {
		reason := unverifiedReason(state)
		if !opts.CacheUnverifiedAcknowledged {
			fmt.Fprintf(opts.Stderr, "REFUSED: %s. Without a verified ready marker the cache may have lost the same entries the DB lost, "+
				"and audit cannot see entries missing on both sides. Replay the manifests per merge-zone-runbook §9.2 first, "+
				"then rerun with -cache-was-not-verified-acknowledged (runbook §9.4).\n", reason)
			return rep, fmt.Errorf("%w: %s", ErrCacheNotVerified, reason)
		}
		fmt.Fprintf(opts.Stderr, "WARN: %s; unfreezing anyway because -cache-was-not-verified-acknowledged was given "+
			"(the operator confirmed the merge-zone-runbook §9.2 replay)\n", reason)
		if state == MarkerIdentityMismatch {
			// 不可信的标记不能活过解冻:读方只看「存在」。按原值撤掉,读到之后有人改过就让运维重跑。
			if err := compareAndDeleteReady(ctx, rd, raw); err != nil {
				return rep, err
			}
			fmt.Fprintf(opts.Stderr, "dropped %s (identity mismatch) by compare-and-delete\n", placement.RouteReadyKey)
		}
	} else {
		rep.MinWatermarkMs = marker.WatermarkMs
		if marker.Generation != meta.Generation {
			aligned := marker
			aligned.Generation = meta.Generation
			next, err := placement.EncodeRouteReadyMarker(aligned)
			if err != nil {
				return rep, fmt.Errorf("re-encode %s with generation %s: %w", placement.RouteReadyKey, meta.Generation, err)
			}
			if err := compareAndSetReady(ctx, rd, raw, next); err != nil {
				return rep, err
			}
			rep.GenerationRealigned = true
			fmt.Fprintf(opts.Stderr, "%s generation %s -> %s (compare-and-set)\n", placement.RouteReadyKey, marker.Generation, meta.Generation)
		}
	}

	wctx, cancel := context.WithTimeout(ctx, opts.Timeout)
	defer cancel()
	changed, err := db.UnfreezeRouteWriters(wctx, rep.MinWatermarkMs)
	if err != nil {
		return rep, err
	}
	rep.Changed = changed
	if !changed {
		// 读开关与解冻之间被别人解冻了:结果一样,照实报。
		fmt.Fprintln(opts.Stderr, "route writers were unfrozen concurrently; nothing changed")
	}
	fmt.Fprintln(opts.Stdout, rep.SummaryLine())
	return rep, nil
}

// readReadyMarker 读并判定就绪标记。返回原值(CAS / CAD 用)、解码后的标记(只在 verified 时有效)与状态。
// 畸形的值在这里就按原值撤掉(状态 malformed_dropped);撤的时候 CAS 输了按 ErrReadyMarkerChanged 返回。
func readReadyMarker(ctx context.Context, rd readyMarkerRedis, identity func(context.Context) (RedisIdentity, error)) (string, placement.RouteReadyMarker, string, error) {
	raw, err := rd.GetCtx(ctx, placement.RouteReadyKey)
	if err != nil {
		if isWrongType(err) {
			return "", placement.RouteReadyMarker{}, MarkerWrongType, nil
		}
		return "", placement.RouteReadyMarker{}, "", fmt.Errorf("GET %s: %w", placement.RouteReadyKey, err)
	}
	if raw == "" {
		return "", placement.RouteReadyMarker{}, MarkerAbsent, nil
	}
	m, err := placement.DecodeRouteReadyMarker(raw)
	switch {
	case errors.Is(err, placement.ErrRouteReadyMarkerNewerVersion):
		return raw, placement.RouteReadyMarker{}, MarkerNewerVersion, nil
	case err != nil:
		if cadErr := compareAndDeleteReady(ctx, rd, raw); cadErr != nil {
			return raw, placement.RouteReadyMarker{}, "", cadErr
		}
		return raw, placement.RouteReadyMarker{}, MarkerMalformedDropped, nil
	}
	id, err := identity(ctx)
	if err != nil {
		return raw, placement.RouteReadyMarker{}, "", fmt.Errorf("read the mapping Redis identity (INFO server / replication): %w", err)
	}
	if id.RunID != m.RedisRunID || id.MasterReplID != m.MasterReplID {
		return raw, placement.RouteReadyMarker{}, MarkerIdentityMismatch, nil
	}
	return raw, m, MarkerVerified, nil
}

// unverifiedReason 是确认不了缓存时给运维的一句原因。
func unverifiedReason(state string) string {
	switch state {
	case MarkerAbsent:
		return placement.RouteReadyKey + " is absent (no reconcile leader has verified the cache; there is none before batch 1b-2)"
	case MarkerMalformedDropped:
		return placement.RouteReadyKey + " was malformed"
	case MarkerNewerVersion:
		return placement.RouteReadyKey + " has a newer format version than this binary understands (left untouched; use the newer binary)"
	case MarkerWrongType:
		return placement.RouteReadyKey + " is not a string key (readers see it as absent; left untouched)"
	case MarkerIdentityMismatch:
		return placement.RouteReadyKey + " was set against another Redis instance (run_id / master_replid differ: restarted, failed over or replaced)"
	default:
		return placement.RouteReadyKey + " state " + state
	}
}

// compareAndDeleteReady 按原值撤标记;回 0(读到之后被改过)→ ErrReadyMarkerChanged。
func compareAndDeleteReady(ctx context.Context, rd readyMarkerRedis, raw string) error {
	return evalReadyScript(ctx, rd, placement.RouteReadyCompareAndDeleteScript, "compare-and-delete", raw)
}

// compareAndSetReady 按原值换标记;回 0 → ErrReadyMarkerChanged。
func compareAndSetReady(ctx context.Context, rd readyMarkerRedis, raw, next string) error {
	return evalReadyScript(ctx, rd, placement.RouteReadyCompareAndSetScript, "compare-and-set", raw, next)
}

func evalReadyScript(ctx context.Context, rd readyMarkerRedis, script, what string, args ...any) error {
	res, err := rd.EvalCtx(ctx, script, []string{placement.RouteReadyKey}, args...)
	if err != nil {
		return fmt.Errorf("%s %s: %w", what, placement.RouteReadyKey, err)
	}
	n, ok := res.(int64)
	switch {
	case !ok:
		return fmt.Errorf("%s %s: unexpected reply %T %v", what, placement.RouteReadyKey, res, res)
	case n == 1:
		return nil
	case n == 0:
		return fmt.Errorf("%w (%s)", ErrReadyMarkerChanged, what)
	default:
		return fmt.Errorf("%s %s: unexpected reply %d", what, placement.RouteReadyKey, n)
	}
}

// isWrongType 判断 err 是不是 Redis 的 WRONGTYPE 错误回复(键不是 string 类型)。
func isWrongType(err error) bool {
	var re goredis.Error
	return errors.As(err, &re) && strings.HasPrefix(re.Error(), "WRONGTYPE")
}
