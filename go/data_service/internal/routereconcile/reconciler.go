// Package routereconcile 是 db_first 下玩家归属 / 落点映射的对账 leader(D1 第 1b-2 批)。
//
// 依据:docs/handoff/launch-readiness-20261013/spec_D1_batch1b2_2_3.md §2(M1–M5)、§4.2–§4.5、§13.1(与正文冲突时以 §13.1 为准);
// 底稿 spec_D1_batch1b_scout.md「plan」1b-2 各项与「# VERDICT」;提案 docs/design/player-route-mapping-durability-proposal.md §5.4、§11.2。
//
// 职责(全服唯一一个 leader,由全局库 route_reconcile 的租约选出):
//   - 补发「已提交、未发布」的孤儿行(增量推送),Redis 重启 / 切主之后回看修补,Redis 整体丢失之后全量预热;
//   - 每拍做库回档检测(水位键 M3 + 世代 + 推送时复读确认的 cache_ahead),命中就冻结映射写者(route_meta.writable=0);
//   - 维护合服围栏镜像 merge:in_progress:{zone}(以库 zone_merge_fence 为准,双向)与合走标记 merge:merged_into:{src}(只增);
//   - ArmReadyMarker=true 时在完整预热 / 修补之后挂就绪标记 mapping:route:ready(预热令牌 M2 fencing + 同连接 WAITAOF),
//     false 时每拍撤掉现存标记(M4);每拍用读方同一段脚本自检并导出 verdict。
//
// redis 模式下不构造(svc.newRouteReconciler / New 拒绝):那时 Redis 才是提交点,库只是 P1 时刻的快照。
//
// 并发结构:一个决策循环(Run → tickOnce)独占全部可变状态;一个任务 goroutine,同一时刻最多一个任务(预热 / 修补 / 追平 / 增量);
// 任务只读库、推 Redis、复读库,结果经 channel 交回循环,由循环统一改状态、挂标记(挂标记的全部前置条件都在循环手里)。
// 「本地租约有效至」用 atomic 发布给任务 goroutine,任务每批之前检查,过点即中止。
//
// 每拍顺序(规格 §4.3,叠加 §13.1):
//  0. 收任务结果(完成 → 记 cvm 起点 S、追平置 VI、预热 / 修补产出挂标记候选;中止 → 修补可重排);
//  1. 租约:leader 每 RenewInterval 续约;非 leader 只 Peek,看起来过期才 Renew(副本多时不抢同一行锁);未持有 → 结束本拍;
//  2. 读身份(INFO server / replication / stats + CONFIG GET maxmemory-policy / enable-debug-command);出错或 role≠master 记
//     redis_error,连续超过 RedisFailureReleaseMs 让出租约;
//  3. 读三把控制键(ready / warm / watermark,一次 MGET + TYPE ready);WRONGTYPE 无条件 DEL、畸形 CAD、版本更新只记日志;
//     三把都不在而本任期见过 = 被 rewarm / FLUSH 清掉:解除 armingBlocked、重取 evicted_keys 基线;
//  4. AdvanceReconcile(水位键 + 世代 + 容差;cvm 按完成任务的起点写);冻结 → 停推送、身份不符 / 淘汰异常时撤标记;
//     未冻结 → 标记世代 ≠ 库世代即冻结(scout D4 保留的那条),否则单调推进水位键(GET → 判 → CAS / SET NX);
//  5. 淘汰策略 / evicted_keys / enable-debug-command 自检:异常时撤标记、禁止挂标记;
//  6. 围栏镜像双向对账(库生效的补 / 续,Redis 有而库已不生效的复读库后按完整值 CAD,解不出 fence_epoch 的不删、计 foreign_fence);
//  7. Arm=false:撤现存标记;VI 为空(或身份变了)排追平(catch_up,设新令牌);
//  8. Arm=true:标记在且身份相等 → VI = 当前身份,CAS 把标记水位推进到 cvm;标记在而身份不等 → CAD、排修补(起点
//     max(0, min(标记水位, cvm) − RepairLookbackMs));标记不在 → **一律**全量预热(armingBlocked 除外),预热 / 修补完成且无阻塞
//     → 挂标记;
//  9. 距上次增量满 IncrementalInterval 且没有任务 → 排增量(起点 max(0, cvm − IncrementalOverlapMs),并复读未决行);
//  10. 自检(RouteReadyReadScript 不带映射键)并写 leader 观测型 gauge。
//
// cache_verified_ms(cvm)的语义(规格 §4.2 must-2):「updated_at_ms < cvm − IncrementalOverlapMs 的行都已推过,且期间 Redis 身份未变」。
// 一个任务从它开始时的库时钟 S 完整跑完、首尾身份相同(带令牌的任务还要令牌仍是自己的)才把 cvm 写成 S;新 leader 的首次增量从
// max(0, cvm − Overlap) 开始;长任务结束后的首次增量从该任务的 S − Overlap 开始;被限速拖长的增量只推进到它自己的 S。
//
// 偏离规格(记在交付说明):
//   - 追平(catch_up)的起点在水位键缺席时取 0(全量):水位键由每拍的 leader 维护,它不在说明 Redis 是空实例 / 被 FLUSH / 被 rewarm,
//     从 cvm − Lookback 追平补不回整库;Arm=false 下身份变化后仍按 max(0, cvm − RepairLookbackMs)。
//   - 围栏镜像对账不成功(列库 / 写镜像出错、镜像的 fence_epoch 比库新)与 foreign_fence 一样阻止挂标记,事件记 arming_refused_unresolved
//     (规格的事件集合里没有单独的 fence 拒绝事件)。
//   - 合走标记镜像的冲突只记 ERROR 与 mirror_total{merged_into_conflict},不阻止挂标记(db_first 下 W9 / W10 读库里的 zone_merged_into)。
package routereconcile

import (
	"context"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"math"
	"sort"
	"strings"
	"sync/atomic"
	"time"

	"data_service/internal/config"
	"data_service/internal/metrics"
	"data_service/internal/store"

	"shared/placement"
	"shared/safego"

	"github.com/zeromicro/go-zero/core/logx"
)

// ── 依赖(消费方定义的接口)────────────────────────────────────────────────────────

// Store 是 leader 对全局库的依赖。*store.RouteStore 天然满足(编译期断言在 reconciler_test.go)。
// 库写全部带租约谓词(store/route_reconcile.go 文件头),这里只读、只按约定调用。
type Store interface {
	PeekReconcileLease(ctx context.Context) (store.RouteLease, error)
	RenewReconcileLease(ctx context.Context, leaderID string, leaseMs uint64) (store.RouteLease, error)
	ReleaseReconcileLease(ctx context.Context, leaderID string) (bool, error)
	AdvanceReconcile(ctx context.Context, req store.ReconcileTickRequest) (store.ReconcileTickResult, error)
	FreezeByLeader(ctx context.Context, leaderID string, reason store.RouteFreezeReason, freezeTimeout time.Duration) (store.ReconcileTickResult, error)
	ScanRoutes(ctx context.Context, after store.RouteScanCursor, limit int) ([]store.RouteRow, error)
	ScanRoutesUpdatedSince(ctx context.Context, shard uint32, after store.RouteUpdatedCursor, limit int) ([]store.RouteRow, error)
	GetRoutes(ctx context.Context, ids []uint64) (map[uint64]store.RouteRow, error)
	ListMergedInto(ctx context.Context) ([]store.MergedIntoRow, error)
	ListActiveMergeFences(ctx context.Context) (store.ActiveMergeFences, error)
	DBNowMs(ctx context.Context) (uint64, error)
}

// Probe 是一次实例自检的结果(Cache.Probe:一个 pipeline 里的 INFO server / replication / stats 与两条 CONFIG GET)。
type Probe struct {
	Identity placement.RedisInstanceIdentity
	// MaxmemoryPolicy:CONFIG GET maxmemory-policy;"" = CONFIG 不可用(被禁用 / 改名,回了错误回复)。
	MaxmemoryPolicy string
	// EvictedKeys:INFO stats 的 evicted_keys(自实例启动以来的累计值);-1 = 取不到。
	EvictedKeys int64
	// DebugCommand:CONFIG GET enable-debug-command(Redis 7:no / yes / local);"" = CONFIG 不可用。
	DebugCommand string
}

// Control 是三把控制键一次读到的原值(一次 MGET + TYPE mapping:route:ready)。nil = 键不在(或不是 string)。
type Control struct {
	Ready *string
	// ReadyWrongType:就绪标记键在,但不是 string 类型(MGET 回 nil 而 TYPE 不是 none)。
	ReadyWrongType bool
	Warm           *string
	Watermark      *string
}

// PushOutcome 是一行推送的结局(cache_ahead 不在这里:它是 superseded 之后复读库的结论,由 leader 判)。
type PushOutcome uint8

const (
	PushSkippedCurrent PushOutcome = iota + 1
	PushApplied
	PushConfirmed
	PushSuperseded
	PushConflict
	PushLegacyConflict
	PushFenced
	PushTombstoneSkipped
	PushDBInvalid
	PushError
)

// label 是 route_reconcile_push_total{result} 的取值。
func (o PushOutcome) label() string {
	switch o {
	case PushSkippedCurrent:
		return metrics.RouteReconcilePushSkippedCurrent
	case PushApplied:
		return metrics.RouteReconcilePushApplied
	case PushConfirmed:
		return metrics.RouteReconcilePushConfirmed
	case PushSuperseded:
		return metrics.RouteReconcilePushSuperseded
	case PushConflict:
		return metrics.RouteReconcilePushConflict
	case PushLegacyConflict:
		return metrics.RouteReconcilePushLegacyConflict
	case PushFenced:
		return metrics.RouteReconcilePushFenced
	case PushTombstoneSkipped:
		return metrics.RouteReconcilePushTombstoneSkipped
	case PushDBInvalid:
		return metrics.RouteReconcilePushDBInvalid
	default:
		return metrics.RouteReconcilePushError
	}
}

// unresolved 报告这个结局是不是「未决」(挂标记要求零未决,规格 scout D3):冲突、遗留值冲突、围栏、库行无效、逐行的错误回复。
func (o PushOutcome) unresolved() bool {
	switch o {
	case PushConflict, PushLegacyConflict, PushFenced, PushDBInvalid, PushError:
		return true
	}
	return false
}

// PushRowResult 是一行的推送结果。CacheRev 只在 PushSuperseded 时有值(缓存里更大的 rev);Detail 只进日志。
type PushRowResult struct {
	PlayerID uint64
	Outcome  PushOutcome
	CacheRev string
	Detail   string
}

// MergedIntoMirrorResult 是一次合走标记镜像(只增)的统计。
type MergedIntoMirrorResult struct {
	Added, Present, Conflicts, Errors int
	// ConflictDetails 只进日志(src、库值、缓存值)。
	ConflictDetails []string
}

// FenceMirror 是 Redis 里一把 merge:in_progress:* 键。Zone 为 0 = 键的后缀不是合法 zone(不是本方案写的,不删)。
type FenceMirror struct {
	Key       string
	Zone      uint32
	Value     string
	WrongType bool
}

// Cache 是 leader 对 mapping Redis 的依赖。生产实现是 RedisCache(cache.go,独立 go-redis 客户端,不经 go-zero 熔断器)。
// 全部方法并发安全(任务 goroutine 与决策循环会同时用它)。
type Cache interface {
	Probe(ctx context.Context) (Probe, error)
	ReadControl(ctx context.Context) (Control, error)
	// SelfCheck 跑一次读方同一段 RouteReadyReadScript(不带映射键)。
	SelfCheck(ctx context.Context) (placement.RouteReadyRead, error)
	SetWarmToken(ctx context.Context, token string) error
	// ArmReady 用 RouteReadyArmScript 挂标记,同一连接紧跟 WAITAOF;挂上了却确认不了落盘时按原值撤掉并返回 error。
	ArmReady(ctx context.Context, token string, m placement.RouteReadyMarker) (placement.RouteReadyArmOutcome, error)
	CompareAndSetReady(ctx context.Context, old, next string) (bool, error)
	CompareAndDeleteReady(ctx context.Context, old string) (bool, error)
	// DeleteReadyWrongType 无条件 DEL 就绪标记键,只给「键在但不是 string」用。
	DeleteReadyWrongType(ctx context.Context) error
	// AdvanceWatermark:prev 为 nil 时 SET NX,否则按原值 CAS 成 next。
	AdvanceWatermark(ctx context.Context, prev *string, next string) (bool, error)
	Push(ctx context.Context, rows []store.RouteRow) ([]PushRowResult, error)
	MirrorMergedInto(ctx context.Context, rows []store.MergedIntoRow) (MergedIntoMirrorResult, error)
	ListFenceMirrors(ctx context.Context) ([]FenceMirror, error)
	SetFenceMirror(ctx context.Context, zone uint32, holderJSON string, epoch, expiresAtMs uint64) (placement.MergeFenceMirrorOutcome, error)
	CompareAndDeleteFenceMirror(ctx context.Context, key, value string) (bool, error)
	Close() error
}

// Clock 是单调时钟、可取消的等待与定时通道。生产用 realClock;单测注入假钟,不依赖墙钟、不真睡。
type Clock interface {
	Now() time.Time
	Sleep(ctx context.Context, d time.Duration) error
	After(d time.Duration) <-chan time.Time
}

type realClock struct{}

func (realClock) Now() time.Time { return time.Now() }

func (realClock) Sleep(ctx context.Context, d time.Duration) error {
	if d <= 0 {
		return ctx.Err()
	}
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return nil
	}
}

func (realClock) After(d time.Duration) <-chan time.Time { return time.After(d) }

// Observer 是指标出口(生产是 metrics 包,单测注入记录器)。label 取 metrics.RouteReconcile* 常量。
type Observer interface {
	Tick(result string)
	Event(event string)
	Push(phase, result string, n int)
	Mirror(result string, n int)
	TaskSeconds(phase string, d time.Duration)
	TaskRows(phase string, n int)
	Leader(held bool)
	LeaderObservations(markerPresent, writable bool, cacheVerifiedAgeSeconds float64, selfCheck string)
	ClearLeaderObservations()
}

// metricsObserver 把 Observer 接到 metrics 包(进程内的默认注册表)。
type metricsObserver struct{}

func (metricsObserver) Tick(result string) { metrics.ObserveRouteReconcileTick(result) }

func (metricsObserver) Event(event string) { metrics.ObserveRouteReconcileEvent(event) }

func (metricsObserver) Push(phase, result string, n int) {
	metrics.ObserveRouteReconcilePush(phase, result, n)
}

func (metricsObserver) Mirror(result string, n int) { metrics.ObserveRouteReconcileMirror(result, n) }

func (metricsObserver) TaskSeconds(phase string, d time.Duration) {
	metrics.ObserveRouteReconcileTaskSeconds(phase, d)
}

func (metricsObserver) TaskRows(phase string, n int) { metrics.SetRouteReconcileTaskRows(phase, n) }

func (metricsObserver) Leader(held bool) { metrics.SetRouteReconcileLeader(held) }

func (metricsObserver) LeaderObservations(markerPresent, writable bool, age float64, selfCheck string) {
	metrics.SetRouteLeaderObservations(markerPresent, writable, age, selfCheck)
}

func (metricsObserver) ClearLeaderObservations() { metrics.ClearRouteLeaderObservations() }

// ── 构造 ──────────────────────────────────────────────────────────────────────

// ErrDisabled 表示配置不要求 leader(redis 模式,或 Reconcile.Enabled=false):调用方不构造。
var ErrDisabled = errors.New("route reconcile leader is disabled (needs RouteStore.Mode=db_first and Reconcile.Enabled)")

// Deps 是 New 的依赖。Store / Cache / LeaderID / Random 必填;Clock、Observer 为 nil 时用真实时钟与 metrics 包。
type Deps struct {
	Store    Store
	Cache    Cache
	LeaderID string
	// Random 是预热令牌的随机源(生产 crypto/rand.Reader)。
	Random   io.Reader
	Clock    Clock
	Observer Observer
}

// 各处的固定时限(不进配置:它们是实现细节,上限由租约与 go-zero 的 5.5s 关停预算决定)。
const (
	// tickOpTimeout 是拍内一次 Redis / 库调用的时限(冻结事务另用 FreezeTimeout)。
	tickOpTimeout = 3 * time.Second
	// taskOpTimeout 是任务里一次扫库 / 推送 / 复读的时限(一批至多 1000 行)。
	taskOpTimeout = 10 * time.Second
	// releaseTimeout 是关停 / 让出租约时释放租约的独立时限(尽力而为,失败不重试,租约到期自然失效)。
	releaseTimeout = time.Second
	// taskStopWait 是 Run 退出时等在途任务收尾的上限;data_service.go 等 Run 整体退出最多 500ms(规格 §4.2「关停顺序」)。
	taskStopWait = 200 * time.Millisecond
	// unresolvedLogSample 是一次日志里列出的未决 player_id 个数上限。
	unresolvedLogSample = 10
)

type options struct {
	arm                 bool
	leaseMs             uint64
	leaseSafety         time.Duration
	lease               time.Duration
	renewInterval       time.Duration
	tick                time.Duration
	incrementalInterval time.Duration
	freezeTimeout       time.Duration
	redisFailureRelease time.Duration
	overlapMs           uint64
	lookbackMs          uint64
	toleranceMs         uint64
	pushBatch           int
	pushRate            int
	pendingLimit        int
	assumeNoEviction    bool
}

func optionsFrom(rc config.RouteStoreConfig) options {
	r := rc.Reconcile
	ms := func(v int) time.Duration { return time.Duration(v) * time.Millisecond }
	return options{
		arm:                 r.ArmReadyMarker,
		leaseMs:             uint64(r.LeaseMs),
		leaseSafety:         ms(r.LeaseSafetyMs()),
		lease:               ms(r.LeaseMs),
		renewInterval:       ms(r.RenewIntervalMs),
		tick:                ms(r.TickIntervalMs),
		incrementalInterval: ms(r.IncrementalIntervalMs),
		freezeTimeout:       ms(r.FreezeTimeoutMs),
		redisFailureRelease: ms(r.RedisFailureReleaseMs),
		overlapMs:           uint64(r.IncrementalOverlapMs),
		lookbackMs:          uint64(r.RepairLookbackMs),
		toleranceMs:         r.ToleranceMs(),
		pushBatch:           r.PushBatch,
		pushRate:            r.PushMaxRowsPerSec,
		pendingLimit:        r.ArmingBlockedReportLimit,
		assumeNoEviction:    r.AssumeNoEviction,
	}
}

// Reconciler 是对账 leader。构造时不开连接、不起 goroutine(单测可以多次构造);Start / Run 才开始工作。
type Reconciler struct {
	opts     options
	st       Store
	cache    Cache
	clock    Clock
	rnd      io.Reader
	obs      Observer
	leaderID string

	// epoch 是本地单调时钟的基准;leaseDeadlineNs = 「本地租约有效至」相对 epoch 的纳秒数(0 = 不持有),发布给任务 goroutine。
	epoch           time.Time
	leaseDeadlineNs atomic.Int64

	// ── 以下只由决策循环读写 ──
	leader        bool
	localDeadline time.Time
	lastRenew     time.Time
	// acquireHoldoffUntil:因连不上 Redis 让出租约后,这段时间内不再抢(让连得上的副本接手)。
	acquireHoldoffUntil time.Time
	redisFailSince      time.Time

	evictedBaseline int64
	baselineSet     bool
	sawControlKeys  bool

	// vi 是「已验证身份」:这份副本被确认对应的 Redis 实例身份(追平完成、标记身份相符、挂标记成功时置)。
	vi        *placement.RedisInstanceIdentity
	candidate *armCandidate
	latch     *armLatch
	// queuedRepair:撤掉身份不符的标记时有任务在跑,修补排在它之后(标记已撤,下一拍看到的是「标记不在」,不能因此改成全量预热)。
	queuedRepair *task
	lastRefusal  string

	pending         map[uint64]struct{}
	pendingOverflow bool

	pendingCvm         *uint64
	pendingCvmIdentity placement.RedisInstanceIdentity
	cacheAheadPending  bool

	generation string
	cvm        uint64
	wm         uint64
	dbNow      uint64

	lastIncremental  time.Time
	lastForeignFence string
	running          *runningTask
	taskSeq          uint64
	taskDone         chan taskResult
	startedThisTick  bool

	// syncTasks:单测用,任务在启动它的那一拍里同步跑完,结果在下一拍开头收(等价于「两拍之间跑完」)。
	syncTasks  bool
	syncResult *taskResult
}

// New 构造 leader。rc 必须是 Normalize 过的 RouteStore 段;不要求 leader(redis 模式 / Enabled=false)时回 ErrDisabled。
func New(rc config.RouteStoreConfig, d Deps) (*Reconciler, error) {
	if !rc.ReconcileEnabled() {
		return nil, ErrDisabled
	}
	if err := rc.Validate(); err != nil {
		return nil, fmt.Errorf("route reconcile: %w", err)
	}
	switch {
	case d.Store == nil:
		return nil, errors.New("route reconcile: the route store is required")
	case d.Cache == nil:
		return nil, errors.New("route reconcile: the mapping redis cache is required")
	case d.Random == nil:
		return nil, errors.New("route reconcile: a random source is required for warm tokens")
	case d.LeaderID == "":
		return nil, errors.New("route reconcile: the leader id is required (store.NewRouteLeaderID)")
	}
	if d.Clock == nil {
		d.Clock = realClock{}
	}
	if d.Observer == nil {
		d.Observer = metricsObserver{}
	}
	r := &Reconciler{
		opts:     optionsFrom(rc),
		st:       d.Store,
		cache:    d.Cache,
		clock:    d.Clock,
		rnd:      d.Random,
		obs:      d.Observer,
		leaderID: d.LeaderID,
		pending:  map[uint64]struct{}{},
		taskDone: make(chan taskResult, 1),
	}
	r.epoch = r.clock.Now()
	return r, nil
}

// LeaderID 返回本副本的 leader 标识(日志 / 横幅用)。
func (r *Reconciler) LeaderID() string { return r.leaderID }

// ArmEnabled 返回 ArmReadyMarker 的取值(横幅、arm_enabled gauge 用)。
func (r *Reconciler) ArmEnabled() bool { return r.opts.arm }

// Start 在 safego goroutine 里跑 Run,返回 stop:取消并最多等 wait,返回 Run 是否在时限内退出。
// 关停顺序(规格 §4.2):data_service.go 把 stop 注册在 Kafka 的 defer 之前(LIFO,在 Kafka flush 之后执行)、等待 ≤ 500ms;
// 释放租约是 Run 退出路径上的尽力而为(独立 1s 时限,失败不重试,租约 LeaseMs 后自然过期)。整个关停不超过 go-zero proc 的 5.5s。
func (r *Reconciler) Start(parent context.Context) (stop func(wait time.Duration) bool) {
	ctx, cancel := context.WithCancel(parent)
	done := make(chan struct{})
	safego.Go("data_service.route_reconcile", func() {
		defer close(done)
		r.Run(ctx)
	})
	return func(wait time.Duration) bool {
		cancel()
		select {
		case <-done:
			return true
		case <-r.clock.After(wait):
			return false
		}
	}
}

// Run 是决策循环:每 TickInterval 一拍,直到 ctx 结束。退出时取消在途任务(最多等 taskStopWait)、尽力释放租约、关 Redis 客户端。
func (r *Reconciler) Run(ctx context.Context) {
	defer r.shutdown()
	logx.Infof("[RouteReconcile] started: leader_id=%s arm_ready_marker=%t lease_ms=%d tick=%s incremental=%s overlap_ms=%d lookback_ms=%d push_batch=%d push_rate=%d/s",
		r.leaderID, r.opts.arm, r.opts.leaseMs, r.opts.tick, r.opts.incrementalInterval, r.opts.overlapMs, r.opts.lookbackMs, r.opts.pushBatch, r.opts.pushRate)
	for ctx.Err() == nil {
		r.tickOnce(ctx)
		if err := r.clock.Sleep(ctx, r.opts.tick); err != nil {
			return
		}
	}
}

func (r *Reconciler) shutdown() {
	if rt := r.running; rt != nil {
		rt.abort("shutdown")
		if r.syncResult == nil {
			select {
			case <-r.taskDone:
			case <-r.clock.After(taskStopWait):
				logx.Errorf("[RouteReconcile] shutdown: task %s seq=%d did not stop within %s; leaving it (its pushes are idempotent and lease-checked)",
					rt.task.phase, rt.task.seq, taskStopWait)
			}
		}
		r.running = nil
	}
	if r.leader {
		ctx, cancel := context.WithTimeout(context.Background(), releaseTimeout)
		released, err := r.st.ReleaseReconcileLease(ctx, r.leaderID)
		cancel()
		switch {
		case err != nil:
			logx.Errorf("[RouteReconcile] shutdown: releasing the lease failed (it expires on its own after %dms): %v", r.opts.leaseMs, err)
		case !released:
			logx.Infof("[RouteReconcile] shutdown: the lease was no longer ours")
		default:
			logx.Infof("[RouteReconcile] shutdown: lease released")
		}
	}
	r.leader = false
	r.leaseDeadlineNs.Store(0)
	r.obs.Leader(false)
	r.obs.ClearLeaderObservations()
	if err := r.cache.Close(); err != nil {
		logx.Errorf("[RouteReconcile] shutdown: close mapping redis client: %v", err)
	}
	logx.Infof("[RouteReconcile] stopped")
}

// ── 任务 ──────────────────────────────────────────────────────────────────────

type taskPhase uint8

const (
	phasePrewarm taskPhase = iota + 1
	phaseRepair
	phaseCatchUp
	phaseIncremental
)

func (p taskPhase) String() string {
	switch p {
	case phasePrewarm:
		return metrics.RouteReconcilePhasePrewarm
	case phaseRepair:
		return metrics.RouteReconcilePhaseRepair
	case phaseCatchUp:
		return metrics.RouteReconcilePhaseCatchUp
	case phaseIncremental:
		return metrics.RouteReconcilePhaseIncremental
	}
	return "unknown"
}

// tokened 报告这类任务是否带预热令牌(可能以挂标记收尾、或要靠令牌证明期间没有 FLUSH / rewarm)。
func (p taskPhase) tokened() bool { return p != phaseIncremental }

type task struct {
	seq   uint64
	phase taskPhase
	// fromMs:repair / catch_up / incremental 扫 updated_at_ms >= fromMs 的行(prewarm 全表,不用它)。
	fromMs     uint64
	token      string
	identity   placement.RedisInstanceIdentity
	generation string
	// pending:incremental 顺带复读的未决 player_id。
	pending []uint64
}

type taskResult struct {
	seq           uint64
	phase         taskPhase
	token         string
	generation    string
	startMs       uint64
	startIdentity placement.RedisInstanceIdentity
	endIdentity   placement.RedisInstanceIdentity
	// identityStable:任务开始与结束时 Redis 身份相同;tokenHeld:结束时 mapping:route:warm 仍是本任务的令牌。
	identityStable bool
	tokenHeld      bool
	complete       bool
	rows           int
	unresolved     []uint64
	overflow       bool
	resolved       []uint64
	cacheAhead     bool
	freezeOutcome  store.ReconcileTickOutcome
	freezeErr      error
	err            error
}

type runningTask struct {
	task      task
	cancel    context.CancelFunc
	startedAt time.Time
	// aborted 非空 = 循环主动取消(降级、冻结、身份变化、令牌被换);结果到达时整份丢弃。
	aborted string
}

func (rt *runningTask) abort(reason string) {
	if rt.aborted == "" {
		rt.aborted = reason
	}
	rt.cancel()
}

// armCandidate 是一次完整预热 / 修补的结果:令牌、它对应的 Redis 身份与库世代。挂标记在决策循环里做。
type armCandidate struct {
	token      string
	identity   placement.RedisInstanceIdentity
	generation string
	phase      taskPhase
}

// armLatch 是 armingBlocked 闩锁(规格 §4.2「挂标记被拒后无限预热」):被拒之后不再重跑全量预热,只做增量并复读未决行;
// 阻塞原因清空后用手里的候选挂标记。candidate 为 nil = 没有可挂的完整结果(例如淘汰导致撤标记),解除后要全量预热一次;
// evictionTaint = 等待期间发生过淘汰,候选作废,解除后同样要全量预热。身份变化、控制键被清(rewarm)时整个闩锁作废。
type armLatch struct {
	candidate     *armCandidate
	identity      placement.RedisInstanceIdentity
	evictionTaint bool
}

// startTask 起一个任务(调用方已确认 running == nil)。带令牌的任务先 SET 新令牌(M2):SET 失败就不起。
func (r *Reconciler) startTask(ctx context.Context, t task) bool {
	if t.phase.tokened() {
		token, err := newWarmToken(r.rnd)
		if err == nil {
			opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
			err = r.cache.SetWarmToken(opCtx, token)
			cancel()
		}
		if err != nil {
			logx.Errorf("[RouteReconcile] %s not started: setting a new warm token failed: %v", t.phase, err)
			return false
		}
		t.token = token
	}
	if t.phase == phasePrewarm {
		// 全量预热重新发现全部未决行。
		r.pending = map[uint64]struct{}{}
		r.pendingOverflow = false
	}
	if t.phase == phaseIncremental {
		r.lastIncremental = r.clock.Now()
		t.pending = r.pendingIDs()
	}
	r.taskSeq++
	t.seq = r.taskSeq
	t.generation = r.generation
	tctx, cancel := context.WithCancel(ctx)
	r.running = &runningTask{task: t, cancel: cancel, startedAt: r.clock.Now()}
	r.startedThisTick = true
	r.obs.Event(metrics.RouteReconcileEventTaskStarted)
	r.obs.TaskRows(t.phase.String(), 0)
	logx.Infof("[RouteReconcile] %s seq=%d started: from_ms=%d pending=%d run_id=%s", t.phase, t.seq, t.fromMs, len(t.pending), t.identity.RunID)
	if r.syncTasks {
		res := r.runTask(tctx, t)
		r.syncResult = &res
		return true
	}
	go func() {
		var res taskResult
		if !safego.Run("data_service.route_reconcile.task", func() { res = r.runTask(tctx, t) }) {
			res = taskResult{seq: t.seq, phase: t.phase, err: errors.New("task panicked (see the safego log)")}
		}
		r.taskDone <- res
	}()
	return true
}

// collectTaskResult 收已结束任务的结果(非阻塞),交给 applyTaskResult。
func (r *Reconciler) collectTaskResult(ctx context.Context) {
	var res taskResult
	switch {
	case r.syncResult != nil:
		res = *r.syncResult
		r.syncResult = nil
	case r.running != nil:
		select {
		case res = <-r.taskDone:
		default:
			return
		}
	default:
		return
	}
	rt := r.running
	r.running = nil
	if rt == nil || rt.task.seq != res.seq {
		logx.Errorf("[RouteReconcile] dropped the result of an unknown task seq=%d (internal bug)", res.seq)
		return
	}
	rt.cancel()
	r.obs.TaskSeconds(res.phase.String(), r.clock.Now().Sub(rt.startedAt))
	r.obs.TaskRows(res.phase.String(), res.rows)
	if rt.aborted != "" {
		r.obs.Event(metrics.RouteReconcileEventTaskAborted)
		logx.Infof("[RouteReconcile] %s seq=%d aborted by the leader loop (%s) after %d rows; result discarded", res.phase, res.seq, rt.aborted, res.rows)
		return
	}
	r.applyTaskResult(ctx, res, rt.task)
}

func (r *Reconciler) applyTaskResult(ctx context.Context, res taskResult, t task) {
	if res.cacheAhead {
		r.obs.Event(metrics.RouteReconcileEventTaskAborted)
		switch {
		case res.freezeErr == nil && res.freezeOutcome == store.ReconcileTickFrozen:
			r.obs.Event(metrics.RouteReconcileEventFreezeCacheAhead)
		case res.freezeErr == nil && res.freezeOutcome == store.ReconcileTickAlreadyFrozen:
		default:
			// 证据在内存里,冻结没做成:每拍重试,直到冻结或丢租约(fail-closed)。
			r.cacheAheadPending = true
		}
		return
	}
	if !res.complete {
		r.obs.Event(metrics.RouteReconcileEventTaskAborted)
		logx.Errorf("[RouteReconcile] %s seq=%d aborted after %d rows: %v", res.phase, res.seq, res.rows, res.err)
		if t.phase == phaseRepair && r.queuedRepair == nil {
			// 标记已经撤掉了;只要身份没再变,修补重排(不因一次网络错误升级成全量预热)。
			retry := t
			retry.token, retry.seq = "", 0
			r.queuedRepair = &retry
		}
		return
	}
	r.obs.Event(metrics.RouteReconcileEventTaskCompleted)
	for _, id := range res.resolved {
		delete(r.pending, id)
	}
	for _, id := range res.unresolved {
		r.addPending(id)
	}
	if res.overflow {
		r.pendingOverflow = true
	}
	if !res.identityStable || (res.phase.tokened() && !res.tokenHeld) {
		logx.Errorf("[RouteReconcile] %s seq=%d finished but cannot be trusted: identity_stable=%t token_held=%t (start run_id=%s end run_id=%s); "+
			"cache_verified_ms is not advanced and nothing is armed", res.phase, res.seq, res.identityStable, res.tokenHeld,
			res.startIdentity.RunID, res.endIdentity.RunID)
		return
	}
	s := res.startMs
	r.pendingCvm = &s
	r.pendingCvmIdentity = res.endIdentity
	logx.Infof("[RouteReconcile] %s seq=%d completed: rows=%d unresolved=%d overflow=%t start_ms=%d", res.phase, res.seq, res.rows,
		len(res.unresolved), res.overflow, res.startMs)
	switch res.phase {
	case phaseCatchUp:
		id := res.endIdentity
		r.vi = &id
	case phasePrewarm, phaseRepair:
		r.candidate = &armCandidate{token: res.token, identity: res.endIdentity, generation: res.generation, phase: res.phase}
	}
}

func (r *Reconciler) addPending(id uint64) {
	if _, ok := r.pending[id]; ok {
		return
	}
	if len(r.pending) >= r.opts.pendingLimit {
		r.pendingOverflow = true
		return
	}
	r.pending[id] = struct{}{}
}

func (r *Reconciler) pendingIDs() []uint64 {
	ids := make([]uint64, 0, len(r.pending))
	for id := range r.pending {
		ids = append(ids, id)
	}
	sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })
	return ids
}

// leaseValid 报告本地租约此刻是否有效(任务 goroutine 每批之前调)。
func (r *Reconciler) leaseValid() bool {
	d := r.leaseDeadlineNs.Load()
	return d != 0 && r.clock.Now().Sub(r.epoch).Nanoseconds() < d
}

var (
	errLeaseExpired = errors.New("local lease expired")
	errCacheAhead   = errors.New("cache ahead of the database")
)

// taskRun 是一个任务的执行状态(只在任务 goroutine 里用)。
type taskRun struct {
	r            *Reconciler
	t            task
	res          *taskResult
	throttleBase time.Time
	throttleRows int
	seen         map[uint64]bool
}

// runTask 跑一个任务。只读库、推 Redis、复读库;cache_ahead 时直接调 FreezeByLeader(带租约谓词)并中止。不改 Reconciler 的循环状态。
func (r *Reconciler) runTask(ctx context.Context, t task) taskResult {
	res := taskResult{seq: t.seq, phase: t.phase, token: t.token, generation: t.generation, startIdentity: t.identity}
	run := &taskRun{r: r, t: t, res: &res, throttleBase: r.clock.Now(), seen: map[uint64]bool{}}

	opCtx, cancel := context.WithTimeout(ctx, taskOpTimeout)
	s, err := r.st.DBNowMs(opCtx)
	cancel()
	if err != nil {
		res.err = fmt.Errorf("read the database clock (task start S): %w", err)
		return res
	}
	res.startMs = s

	switch t.phase {
	case phasePrewarm:
		err = run.prewarm(ctx)
	default:
		err = run.scanUpdatedSince(ctx)
	}
	if err == nil && t.phase == phaseIncremental {
		err = run.recheckPending(ctx)
	}
	if err == nil {
		err = run.mirrorMergedInto(ctx)
	}
	if err != nil {
		res.err = err
		return res
	}

	opCtx, cancel = context.WithTimeout(ctx, taskOpTimeout)
	defer cancel()
	p, err := r.cache.Probe(opCtx)
	if err != nil {
		res.err = fmt.Errorf("re-read the redis identity at the end: %w", err)
		return res
	}
	res.endIdentity = p.Identity
	res.identityStable = p.Identity == t.identity
	if t.phase.tokened() {
		ctl, err := r.cache.ReadControl(opCtx)
		if err != nil {
			res.err = fmt.Errorf("re-read the warm token at the end: %w", err)
			return res
		}
		res.tokenHeld = ctl.Warm != nil && *ctl.Warm == t.token
	}
	res.complete = true
	return res
}

// gate 在每批之前检查:ctx 没结束、本地租约仍有效(规格 scout「LeaseExpiryMidPrewarmAbortsBeforeNextBatch」)。
func (run *taskRun) gate(ctx context.Context) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if !run.r.leaseValid() {
		return errLeaseExpired
	}
	return nil
}

// throttle 按 PushMaxRowsPerSec 限速(注入的时钟,不依赖墙钟):累计行数对应的最早时刻还没到就等。
func (run *taskRun) throttle(ctx context.Context, n int) error {
	run.throttleRows += n
	due := run.throttleBase.Add(time.Duration(float64(run.throttleRows) / float64(run.r.opts.pushRate) * float64(time.Second)))
	if wait := due.Sub(run.r.clock.Now()); wait > 0 {
		return run.r.clock.Sleep(ctx, wait)
	}
	return nil
}

func (run *taskRun) prewarm(ctx context.Context) error {
	r := run.r
	cursor := store.RouteScanCursor{}
	for {
		if err := run.gate(ctx); err != nil {
			return err
		}
		opCtx, cancel := context.WithTimeout(ctx, taskOpTimeout)
		rows, err := r.st.ScanRoutes(opCtx, cursor, r.opts.pushBatch)
		cancel()
		if err != nil {
			return fmt.Errorf("scan player_route after (%d, %d): %w", cursor.Shard, cursor.PlayerID, err)
		}
		if len(rows) > 0 {
			if err := run.push(ctx, rows); err != nil {
				return err
			}
			cursor = store.RouteScanCursorAt(rows[len(rows)-1])
		}
		if len(rows) < r.opts.pushBatch {
			return nil
		}
	}
}

// scanUpdatedSince 对 64 个行片各自按 (updated_at_ms, player_id) 键集翻页,起点 updated_at_ms >= fromMs(提案 §11.2 G1a 模板)。
func (run *taskRun) scanUpdatedSince(ctx context.Context) error {
	r := run.r
	for shard := uint32(0); shard < placement.RouteRowShards; shard++ {
		cursor := store.RouteUpdatedSince(run.t.fromMs)
		for {
			if err := run.gate(ctx); err != nil {
				return err
			}
			opCtx, cancel := context.WithTimeout(ctx, taskOpTimeout)
			rows, err := r.st.ScanRoutesUpdatedSince(opCtx, shard, cursor, r.opts.pushBatch)
			cancel()
			if err != nil {
				return fmt.Errorf("scan player_route shard %d updated after (%d, %d): %w", shard, cursor.UpdatedAtMs, cursor.PlayerID, err)
			}
			if len(rows) > 0 {
				if err := run.push(ctx, rows); err != nil {
					return err
				}
				cursor = store.RouteUpdatedCursorAt(rows[len(rows)-1])
			}
			if len(rows) < r.opts.pushBatch {
				break
			}
		}
	}
	return nil
}

// recheckPending 复读上一轮留下的未决行并重推(armingBlocked 期间靠它解除「未决」)。库里读不到的行仍算未决(只记日志)。
func (run *taskRun) recheckPending(ctx context.Context) error {
	r := run.r
	ids := run.t.pending
	for len(ids) > 0 {
		n := min(len(ids), store.RouteBatchLimit, r.opts.pushBatch)
		chunk := ids[:n]
		ids = ids[n:]
		if err := run.gate(ctx); err != nil {
			return err
		}
		opCtx, cancel := context.WithTimeout(ctx, taskOpTimeout)
		got, err := r.st.GetRoutes(opCtx, chunk)
		cancel()
		if err != nil {
			return fmt.Errorf("re-read %d unresolved rows: %w", len(chunk), err)
		}
		rows := make([]store.RouteRow, 0, len(got))
		for _, id := range chunk {
			if row, ok := got[id]; ok {
				rows = append(rows, row)
			} else {
				logx.Errorf("[RouteReconcile] unresolved player %d has no player_route row any more; it stays unresolved (check route_tool audit)", id)
				run.unresolve(id)
			}
		}
		if len(rows) > 0 {
			if err := run.push(ctx, rows); err != nil {
				return err
			}
		}
	}
	return nil
}

func (run *taskRun) unresolve(id uint64) {
	if len(run.res.unresolved) >= run.r.opts.pendingLimit {
		run.res.overflow = true
		return
	}
	run.res.unresolved = append(run.res.unresolved, id)
}

// push 推一批行:限速 → Push → superseded 的复读库(库 rev < 缓存 rev 或读不到行 = cache_ahead,冻结并中止)→ 计数。
func (run *taskRun) push(ctx context.Context, rows []store.RouteRow) error {
	r := run.r
	phase := run.t.phase.String()
	if err := run.throttle(ctx, len(rows)); err != nil {
		return err
	}
	if err := run.gate(ctx); err != nil {
		return err
	}
	opCtx, cancel := context.WithTimeout(ctx, taskOpTimeout)
	results, err := r.cache.Push(opCtx, rows)
	cancel()
	if err != nil {
		return fmt.Errorf("push %d rows: %w", len(rows), err)
	}
	counts := map[string]int{}
	var superseded []PushRowResult
	for _, pr := range results {
		switch {
		case pr.Outcome == PushSuperseded:
			superseded = append(superseded, pr)
			continue
		case pr.Outcome.unresolved():
			run.unresolve(pr.PlayerID)
			if !run.seen[pr.PlayerID] && len(run.seen) < unresolvedLogSample*10 {
				logx.Errorf("[RouteReconcile] %s: player %d unresolved (%s): %s", phase, pr.PlayerID, pr.Outcome.label(), pr.Detail)
			}
			run.seen[pr.PlayerID] = true
		default:
			run.res.resolved = append(run.res.resolved, pr.PlayerID)
		}
		counts[pr.Outcome.label()]++
	}
	defer func() {
		for label, n := range counts {
			r.obs.Push(phase, label, n)
		}
		run.res.rows += len(rows)
		r.obs.TaskRows(phase, run.res.rows)
	}()
	if len(superseded) == 0 {
		return nil
	}
	ids := make([]uint64, len(superseded))
	for i, s := range superseded {
		ids[i] = s.PlayerID
	}
	opCtx, cancel = context.WithTimeout(ctx, taskOpTimeout)
	db, err := r.st.GetRoutes(opCtx, ids)
	cancel()
	if err != nil {
		return fmt.Errorf("re-read %d superseded rows: %w", len(ids), err)
	}
	var ahead []string
	for _, s := range superseded {
		row, ok := db[s.PlayerID]
		cmp := -1
		if ok {
			cmp, err = placement.CompareRev(placement.FormatRev(row.Rev), s.CacheRev)
			if err != nil {
				run.unresolve(s.PlayerID)
				counts[metrics.RouteReconcilePushError]++
				logx.Errorf("[RouteReconcile] %s: player %d superseded with an uncomparable rev (db %d, cache %q): %v", phase, s.PlayerID, row.Rev, s.CacheRev, err)
				continue
			}
		}
		if cmp < 0 {
			// 库 rev < 缓存 rev,或库里干脆没有这一行:缓存比库新 —— 库丢了已提交的写(回档),或有写者绕过了库。
			counts[metrics.RouteReconcilePushCacheAhead]++
			if ok {
				ahead = append(ahead, fmt.Sprintf("%d(db_rev=%d cache_rev=%s)", s.PlayerID, row.Rev, s.CacheRev))
			} else {
				ahead = append(ahead, fmt.Sprintf("%d(no db row, cache_rev=%s)", s.PlayerID, s.CacheRev))
			}
			continue
		}
		// 库 rev >= 缓存 rev:在线写者在我们读库之后提交并发布了更新的版本,正常竞态(更新的库值由写者自己发布,或由下一轮增量补)。
		counts[metrics.RouteReconcilePushSuperseded]++
		run.res.resolved = append(run.res.resolved, s.PlayerID)
	}
	if len(ahead) == 0 {
		return nil
	}
	if len(ahead) > unresolvedLogSample {
		ahead = append(ahead[:unresolvedLogSample], fmt.Sprintf("... %d more", len(ahead)-unresolvedLogSample))
	}
	logx.Errorf("[RouteReconcile] %s: CACHE AHEAD of player_route for %s; freezing the route writers (route_meta.writable=0). "+
		"Recover with route_tool freeze → reconcile-from-cache → audit -frozen-recovery → unfreeze (runbook §9.4)", phase, strings.Join(ahead, ", "))
	run.res.cacheAhead = true
	fr, ferr := r.st.FreezeByLeader(ctx, r.leaderID, store.RouteFreezeCacheAhead, r.opts.freezeTimeout)
	run.res.freezeOutcome, run.res.freezeErr = fr.Outcome, ferr
	if ferr != nil {
		logx.Errorf("[RouteReconcile] freezing the route writers after cache_ahead failed (the leader loop retries every tick): %v", ferr)
	}
	return errCacheAhead
}

func (run *taskRun) mirrorMergedInto(ctx context.Context) error {
	r := run.r
	if err := run.gate(ctx); err != nil {
		return err
	}
	opCtx, cancel := context.WithTimeout(ctx, taskOpTimeout)
	defer cancel()
	rows, err := r.st.ListMergedInto(opCtx)
	if err != nil {
		return fmt.Errorf("list zone_merged_into: %w", err)
	}
	if len(rows) == 0 {
		return nil
	}
	m, err := r.cache.MirrorMergedInto(opCtx, rows)
	r.obs.Mirror(metrics.RouteReconcileMirrorMergedIntoAdded, m.Added)
	r.obs.Mirror(metrics.RouteReconcileMirrorMergedIntoPresent, m.Present)
	r.obs.Mirror(metrics.RouteReconcileMirrorMergedIntoConflict, m.Conflicts)
	r.obs.Mirror(metrics.RouteReconcileMirrorError, m.Errors)
	for _, d := range m.ConflictDetails {
		logx.Errorf("[RouteReconcile] merge:merged_into mirror CONFLICT (add-only, not overwritten; zone_merged_into is the source of truth): %s", d)
	}
	if err != nil {
		return fmt.Errorf("mirror merge:merged_into: %w", err)
	}
	return nil
}

// ── 决策循环 ──────────────────────────────────────────────────────────────────

// tickOnce 跑一拍并记 tick_total;返回本拍的结果 label。单测直接调它(不起计时器)。
func (r *Reconciler) tickOnce(ctx context.Context) string {
	if ctx.Err() != nil {
		return ""
	}
	r.startedThisTick = false
	r.collectTaskResult(ctx)
	res := r.tick(ctx)
	r.obs.Tick(res)
	return res
}

// markerKind 是一次读到的就绪标记 / 水位键的形状。
type markerKind uint8

const (
	keyAbsent markerKind = iota
	keyValid
	keyMalformed
	keyNewer
	keyWrongType
)

type markerState struct {
	kind markerKind
	raw  string
	m    placement.RouteReadyMarker
}

func classifyMarker(ctl Control) markerState {
	switch {
	case ctl.ReadyWrongType:
		return markerState{kind: keyWrongType}
	case ctl.Ready == nil:
		return markerState{kind: keyAbsent}
	}
	raw := *ctl.Ready
	m, err := placement.DecodeRouteReadyMarker(raw)
	switch {
	case errors.Is(err, placement.ErrRouteReadyMarkerNewerVersion):
		return markerState{kind: keyNewer, raw: raw}
	case err != nil:
		return markerState{kind: keyMalformed, raw: raw}
	}
	return markerState{kind: keyValid, raw: raw, m: m}
}

type watermarkState struct {
	kind markerKind
	raw  *string
	w    placement.RouteWatermark
}

func classifyWatermark(raw *string) watermarkState {
	if raw == nil {
		return watermarkState{kind: keyAbsent}
	}
	w, err := placement.DecodeRouteWatermark(*raw)
	switch {
	case errors.Is(err, placement.ErrRouteWatermarkNewerVersion):
		return watermarkState{kind: keyNewer, raw: raw}
	case err != nil:
		return watermarkState{kind: keyMalformed, raw: raw}
	}
	return watermarkState{kind: keyValid, raw: raw, w: w}
}

// tick 是一拍的主体(规格 §4.3 叠加 §13.1,步骤编号见包注释)。
func (r *Reconciler) tick(ctx context.Context) string {
	now := r.clock.Now()
	if res, ok := r.ensureLease(ctx, now); !ok {
		return res
	}

	// 2. 身份与自检项。
	p, err := r.probe(ctx)
	if err != nil {
		return r.onRedisFailure(ctx, now, "probe", err)
	}
	// 3. 三把控制键。
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	ctl, err := r.cache.ReadControl(opCtx)
	cancel()
	if err != nil {
		return r.onRedisFailure(ctx, now, "read control keys", err)
	}
	r.redisFailSince = time.Time{}
	if !r.baselineSet && p.EvictedKeys >= 0 {
		r.evictedBaseline, r.baselineSet = p.EvictedKeys, true
	}
	if r.pendingCvm != nil && r.pendingCvmIdentity != p.Identity {
		logx.Errorf("[RouteReconcile] redis identity changed since the last task finished (run_id %s → %s); its cache_verified_ms is discarded",
			r.pendingCvmIdentity.RunID, p.Identity.RunID)
		r.pendingCvm = nil
	}
	ms := r.settleMarkerShape(ctx, ctl)
	wm := classifyWatermark(ctl.Watermark)
	r.checkControlKeysWiped(ctl, p)
	r.checkRunningToken(ctl, p)

	// 4. 每拍的库内部分。
	if res, ok := r.advance(ctx, now, wm, &ms, p); !ok {
		return res
	}

	// 5. 淘汰 / DEBUG 自检。
	evBad, evTaint := r.evictionState(p)
	dbgBad := p.DebugCommand != "no"
	if !r.opts.arm && (ms.kind == keyValid || ms.kind == keyNewer) {
		r.dropMarker(ctx, &ms, metrics.RouteReconcileEventMarkerDroppedDisarmed, "ArmReadyMarker=false (M4: disarming withdraws trust)")
	}
	if ms.kind == keyValid && evBad {
		r.dropMarker(ctx, &ms, metrics.RouteReconcileEventMarkerDroppedEviction,
			fmt.Sprintf("eviction is possible or happened (maxmemory-policy=%q evicted_keys=%d baseline=%d)", p.MaxmemoryPolicy, p.EvictedKeys, r.evictedBaseline))
		r.vi = nil
		if r.opts.arm && r.latch == nil {
			r.latch = &armLatch{identity: p.Identity, evictionTaint: evTaint}
		}
	}
	if ms.kind == keyValid && dbgBad {
		// 事件(arming_refused_debug_enabled)由第 8 步的 reportRefusal 计,这里不重复计。
		r.dropMarker(ctx, &ms, "",
			fmt.Sprintf("enable-debug-command=%q (DEBUG RELOAD replaces the data without changing run_id; must be \"no\")", p.DebugCommand))
		r.vi = nil
	}

	// 6. 围栏镜像双向对账。
	mirrorOK, foreign := r.reconcileFenceMirrors(ctx)

	// 7 / 8. 标记与任务。
	if r.opts.arm {
		r.decideArmed(ctx, &ms, p, blockers{eviction: evBad, debug: dbgBad, fenceMirror: !mirrorOK || foreign}, evTaint)
	} else {
		r.decideDisarmed(ctx, ctl, p)
	}

	// 9. 增量。
	if r.running == nil && !r.startedThisTick && r.queuedRepair == nil && r.clock.Now().Sub(r.lastIncremental) >= r.opts.incrementalInterval {
		r.startTask(ctx, task{phase: phaseIncremental, fromMs: satSub(r.cvm, r.opts.overlapMs), identity: p.Identity})
	}

	// 10. 自检与观测。
	r.observe(ctx, true, ms.kind != keyAbsent)
	return metrics.RouteReconcileTickOK
}

// ensureLease 是第 1 步。返回 (tick 结果, 是否继续本拍)。
func (r *Reconciler) ensureLease(ctx context.Context, now time.Time) (string, bool) {
	if r.leader {
		if !now.Before(r.localDeadline) {
			r.demote("the local lease expired before it could be renewed")
			return metrics.RouteReconcileTickLeaseLost, false
		}
		if now.Sub(r.lastRenew) < r.opts.renewInterval {
			return "", true
		}
		return r.renew(ctx, now)
	}
	if now.Before(r.acquireHoldoffUntil) {
		return metrics.RouteReconcileTickNotLeader, false
	}
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	l, err := r.st.PeekReconcileLease(opCtx)
	cancel()
	if err != nil {
		logx.Errorf("[RouteReconcile] peek lease: %v", err)
		return metrics.RouteReconcileTickDBError, false
	}
	if l.Held && l.Holder != r.leaderID {
		return metrics.RouteReconcileTickNotLeader, false
	}
	return r.renew(ctx, now)
}

// renew 续约 / 抢租约。本地截止 = 续约发出时刻 + LeaseMs − Safety(scout D6;库侧 fencing 靠 SQL 谓词)。
func (r *Reconciler) renew(ctx context.Context, sent time.Time) (string, bool) {
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	l, err := r.st.RenewReconcileLease(opCtx, r.leaderID, r.opts.leaseMs)
	cancel()
	if err != nil {
		logx.Errorf("[RouteReconcile] renew lease: %v", err)
		return metrics.RouteReconcileTickDBError, false
	}
	if !l.HeldBy(r.leaderID) {
		if r.leader {
			r.demote(fmt.Sprintf("the lease is held by %q now", l.Holder))
			return metrics.RouteReconcileTickLeaseLost, false
		}
		return metrics.RouteReconcileTickNotLeader, false
	}
	if !r.leader {
		r.promote()
	}
	r.lastRenew = sent
	r.localDeadline = sent.Add(r.opts.lease - r.opts.leaseSafety)
	r.leaseDeadlineNs.Store(max(1, r.localDeadline.Sub(r.epoch).Nanoseconds()))
	return "", true
}

func (r *Reconciler) promote() {
	r.leader = true
	r.resetTerm()
	r.obs.Leader(true)
	logx.Infof("[RouteReconcile] became the leader (%s)", r.leaderID)
}

// demote 降级:取消任务、清掉本任期的全部内存状态、删 leader 观测型 gauge。
func (r *Reconciler) demote(reason string) {
	if !r.leader {
		return
	}
	r.leader = false
	r.leaseDeadlineNs.Store(0)
	if r.running != nil {
		r.running.abort("demoted")
	}
	r.resetTerm()
	r.obs.Leader(false)
	r.obs.ClearLeaderObservations()
	logx.Errorf("[RouteReconcile] no longer the leader: %s", reason)
}

func (r *Reconciler) resetTerm() {
	r.vi, r.candidate, r.latch, r.queuedRepair = nil, nil, nil, nil
	r.lastRefusal, r.lastForeignFence = "", ""
	r.pending = map[uint64]struct{}{}
	r.pendingOverflow = false
	r.pendingCvm = nil
	r.cacheAheadPending = false
	r.baselineSet = false
	r.sawControlKeys = false
	r.redisFailSince = time.Time{}
	r.lastIncremental = time.Time{}
	r.generation, r.cvm, r.wm, r.dbNow = "", 0, 0, 0
}

func (r *Reconciler) probe(ctx context.Context) (Probe, error) {
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	defer cancel()
	p, err := r.cache.Probe(opCtx)
	if err != nil {
		return Probe{}, err
	}
	if p.Identity.Role != "master" {
		return Probe{}, fmt.Errorf("mapping redis role is %q, not master", p.Identity.Role)
	}
	return p, nil
}

// onRedisFailure 记一次读不到 mapping Redis。连续失败超过 RedisFailureReleaseMs 就让出租约(核实意见 should「连不上 Redis 占着租约」),
// 并在同样长的时间内不再抢,让连得上 Redis 的副本接手。撤不了标记(撤不了):读方自己按 M1 判身份。
func (r *Reconciler) onRedisFailure(ctx context.Context, now time.Time, what string, err error) string {
	if r.redisFailSince.IsZero() {
		r.redisFailSince = now
	}
	failing := now.Sub(r.redisFailSince)
	logx.Errorf("[RouteReconcile] mapping redis %s failed (failing for %s): %v", what, failing, err)
	if failing < r.opts.redisFailureRelease {
		return metrics.RouteReconcileTickRedisError
	}
	relCtx, cancel := context.WithTimeout(ctx, releaseTimeout)
	released, rerr := r.st.ReleaseReconcileLease(relCtx, r.leaderID)
	cancel()
	if rerr != nil {
		logx.Errorf("[RouteReconcile] releasing the lease after %s of redis failures failed (it expires on its own): %v", failing, rerr)
	}
	r.obs.Event(metrics.RouteReconcileEventLeaseReleasedRedisUnreachable)
	r.demote(fmt.Sprintf("mapping redis unreachable for %s (released=%t); holding off acquisition for %s", failing, released, r.opts.redisFailureRelease))
	r.acquireHoldoffUntil = now.Add(r.opts.redisFailureRelease)
	return metrics.RouteReconcileTickRedisError
}

// settleMarkerShape 处理就绪标记的形状:WRONGTYPE 无条件 DEL、畸形按原值 CAD(都计 marker_dropped_malformed),版本更新只记日志。
func (r *Reconciler) settleMarkerShape(ctx context.Context, ctl Control) markerState {
	ms := classifyMarker(ctl)
	switch ms.kind {
	case keyWrongType:
		opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
		err := r.cache.DeleteReadyWrongType(opCtx)
		cancel()
		if err != nil {
			logx.Errorf("[RouteReconcile] %s is not a string; DEL failed: %v", placement.RouteReadyKey, err)
		} else {
			r.obs.Event(metrics.RouteReconcileEventMarkerDroppedMalformed)
			logx.Errorf("[RouteReconcile] %s was not a string (readers treat it as absent); deleted", placement.RouteReadyKey)
		}
		return markerState{kind: keyAbsent}
	case keyMalformed:
		r.dropMarker(ctx, &ms, metrics.RouteReconcileEventMarkerDroppedMalformed, "malformed marker value")
	case keyNewer:
		logx.Errorf("[RouteReconcile] %s has a newer format version than this binary; left untouched and not trusted", placement.RouteReadyKey)
	}
	return ms
}

// dropMarker 按原值 CAD 就绪标记,成功时计 event(event 为空不计);ms 置为缺席(CAD 输了说明值已被换,下一拍重判)。
// 返回 false = CAD 本身出错(Redis 故障),标记可能还在,调用方本拍不要据「已撤」往下走。
func (r *Reconciler) dropMarker(ctx context.Context, ms *markerState, event, why string) bool {
	if ms.kind == keyAbsent {
		return true
	}
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	ok, err := r.cache.CompareAndDeleteReady(opCtx, ms.raw)
	cancel()
	switch {
	case err != nil:
		logx.Errorf("[RouteReconcile] compare-and-delete %s (%s) failed: %v", placement.RouteReadyKey, why, err)
		return false
	case ok:
		if event != "" {
			r.obs.Event(event)
		}
		logx.Errorf("[RouteReconcile] dropped %s: %s", placement.RouteReadyKey, why)
	default:
		logx.Infof("[RouteReconcile] %s changed before it could be dropped (%s); re-judged next tick", placement.RouteReadyKey, why)
	}
	*ms = markerState{kind: keyAbsent}
	return true
}

// checkControlKeysWiped:三把控制键都不在,而本任期见过它们 = 被 rewarm(一条 DEL 删三把)或 FLUSHALL / FLUSHDB 清掉。
// 闩锁、候选、VI 全部作废,重取 evicted_keys 基线(规格 §13.1 第 6 条);在途的带令牌任务也挂不上了,取消。
func (r *Reconciler) checkControlKeysWiped(ctl Control, p Probe) {
	present := ctl.Ready != nil || ctl.ReadyWrongType || ctl.Warm != nil || ctl.Watermark != nil
	if present {
		r.sawControlKeys = true
		return
	}
	if !r.sawControlKeys {
		return
	}
	r.sawControlKeys = false
	logx.Errorf("[RouteReconcile] all three control keys are gone (rewarm, FLUSHALL or FLUSHDB): resetting arming state; evicted_keys baseline %d → %d",
		r.evictedBaseline, p.EvictedKeys)
	r.vi, r.candidate, r.latch, r.queuedRepair = nil, nil, nil, nil
	r.lastRefusal = ""
	r.pending = map[uint64]struct{}{}
	r.pendingOverflow = false
	r.baselineSet = p.EvictedKeys >= 0
	r.evictedBaseline = max(p.EvictedKeys, 0)
	if r.running != nil && r.running.task.phase.tokened() {
		r.running.abort("control keys wiped")
	}
}

// checkRunningToken:在途的带令牌任务,令牌已不是它的(被 FLUSH、rewarm 或别的 leader 覆写)→ 它挂不上标记、也证明不了数据完整,取消。
func (r *Reconciler) checkRunningToken(ctl Control, p Probe) {
	rt := r.running
	if rt == nil || !rt.task.phase.tokened() || rt.aborted != "" {
		return
	}
	if ctl.Warm != nil && *ctl.Warm == rt.task.token && rt.task.identity == p.Identity {
		return
	}
	reason := "warm token replaced"
	if rt.task.identity != p.Identity {
		reason = "redis identity changed"
	}
	logx.Errorf("[RouteReconcile] cancelling %s seq=%d: %s", rt.task.phase, rt.task.seq, reason)
	rt.abort(reason)
	r.vi = nil
}

// advance 是第 4 步。返回 (tick 结果, 是否继续)。冻结时在这里收尾(撤该撤的标记、写观测)。
func (r *Reconciler) advance(ctx context.Context, now time.Time, wm watermarkState, ms *markerState, p Probe) (string, bool) {
	// 冻结前本地剩余租约不够一次冻结事务 + 安全余量,先续约(规格 §4.2「TiDB 缓存表冻结超时」)。
	if r.localDeadline.Sub(now) < r.opts.freezeTimeout+r.opts.leaseSafety {
		if res, ok := r.renew(ctx, now); !ok {
			return res, false
		}
	}
	if r.cacheAheadPending {
		fr, err := r.st.FreezeByLeader(ctx, r.leaderID, store.RouteFreezeCacheAhead, r.opts.freezeTimeout)
		if err != nil {
			logx.Errorf("[RouteReconcile] retrying the cache_ahead freeze failed: %v", err)
			return metrics.RouteReconcileTickDBError, false
		}
		r.cacheAheadPending = false
		switch fr.Outcome {
		case store.ReconcileTickLeaseLost:
			r.demote("lease lost while freezing for cache_ahead")
			return metrics.RouteReconcileTickLeaseLost, false
		case store.ReconcileTickFrozen:
			r.obs.Event(metrics.RouteReconcileEventFreezeCacheAhead)
		}
	}
	var key *placement.RouteWatermark
	if wm.kind == keyValid {
		w := wm.w
		key = &w
	}
	res, err := r.st.AdvanceReconcile(ctx, store.ReconcileTickRequest{
		LeaderID:           r.leaderID,
		WatermarkKey:       key,
		ToleranceMs:        r.opts.toleranceMs,
		SetCacheVerifiedMs: r.pendingCvm,
		FreezeTimeout:      r.opts.freezeTimeout,
	})
	if err != nil {
		logx.Errorf("[RouteReconcile] advance: %v", err)
		return metrics.RouteReconcileTickDBError, false
	}
	r.generation, r.cvm, r.wm, r.dbNow = res.Generation, res.CacheVerifiedMs, res.WatermarkMs, res.NowMs
	switch res.Outcome {
	case store.ReconcileTickLeaseLost:
		r.demote("AdvanceReconcile found the lease lost")
		return metrics.RouteReconcileTickLeaseLost, false
	case store.ReconcileTickFrozen:
		r.pendingCvm = nil
		r.obs.Event(freezeEvent(res.FreezeReason))
		logx.Errorf("[RouteReconcile] FROZE the route writers: %s (watermark key %+v, db watermark_ms=%d generation=%s)",
			res.FreezeReason, key, res.WatermarkMs, res.Generation)
		return r.onFrozen(ctx, ms, p), false
	case store.ReconcileTickAlreadyFrozen:
		r.pendingCvm = nil
		return r.onFrozen(ctx, ms, p), false
	case store.ReconcileTickAdvanced:
	default:
		logx.Errorf("[RouteReconcile] advance returned an unexpected outcome %s", res.Outcome)
		return metrics.RouteReconcileTickDBError, false
	}
	r.pendingCvm = nil

	// 标记世代 ≠ 库世代:标记属于另一份库(库被重建 / 换库),冻结(scout D4 保留的世代判据;水位键缺席时只有它能发现)。
	if ms.kind == keyValid && ms.m.Generation != res.Generation {
		fr, err := r.st.FreezeByLeader(ctx, r.leaderID, store.RouteFreezeGenerationMismatch, r.opts.freezeTimeout)
		if err != nil {
			logx.Errorf("[RouteReconcile] marker generation %s != db generation %s; freezing failed: %v", ms.m.Generation, res.Generation, err)
			return metrics.RouteReconcileTickDBError, false
		}
		switch fr.Outcome {
		case store.ReconcileTickLeaseLost:
			r.demote("lease lost while freezing for a marker generation mismatch")
			return metrics.RouteReconcileTickLeaseLost, false
		case store.ReconcileTickFrozen:
			r.obs.Event(metrics.RouteReconcileEventFreezeGeneration)
			logx.Errorf("[RouteReconcile] FROZE the route writers: marker generation %s != db generation %s", ms.m.Generation, res.Generation)
		}
		return r.onFrozen(ctx, ms, p), false
	}
	r.advanceWatermarkKey(ctx, wm, res)
	return "", true
}

func freezeEvent(reason store.RouteFreezeReason) string {
	switch reason {
	case store.RouteFreezeGenerationMismatch:
		return metrics.RouteReconcileEventFreezeGeneration
	case store.RouteFreezeCacheAhead:
		return metrics.RouteReconcileEventFreezeCacheAhead
	default:
		return metrics.RouteReconcileEventFreezeWatermark
	}
}

// onFrozen:冻结期停止一切推送(取消在途任务、不起新任务、不推进水位键、不对账围栏镜像);标记不动,除非它已不可信
// (身份不符、淘汰异常、DEBUG 开着)或 Arm=false(撤销信任永远安全)。
func (r *Reconciler) onFrozen(ctx context.Context, ms *markerState, p Probe) string {
	if r.running != nil {
		r.running.abort("route writers frozen")
	}
	// 冻结期间库可能正处在回档之后:之前完成的预热 / 修补结果都不再作数,解冻之后按标记现状重新判(多半是全量预热)。
	r.candidate, r.queuedRepair, r.latch, r.lastRefusal = nil, nil, nil, ""
	if ms.kind == keyValid || (ms.kind == keyNewer && !r.opts.arm) {
		evBad, _ := r.evictionState(p)
		switch {
		case !r.opts.arm:
			r.dropMarker(ctx, ms, metrics.RouteReconcileEventMarkerDroppedDisarmed, "ArmReadyMarker=false (frozen)")
		case ms.m.RedisRunID != p.Identity.RunID || ms.m.MasterReplID != p.Identity.MasterReplID:
			r.dropMarker(ctx, ms, metrics.RouteReconcileEventMarkerDroppedIdentity, "redis identity changed (frozen)")
		case evBad:
			r.dropMarker(ctx, ms, metrics.RouteReconcileEventMarkerDroppedEviction, "eviction possible (frozen)")
		case p.DebugCommand != "no":
			r.dropMarker(ctx, ms, metrics.RouteReconcileEventArmingRefusedDebugEnabled, "enable-debug-command is not no (frozen)")
		}
	}
	r.vi = nil
	r.observe(ctx, false, ms.kind != keyAbsent)
	return metrics.RouteReconcileTickFrozen
}

// advanceWatermarkKey 在本拍提交之后单调推进水位键(M3):GET(本拍 MGET 的原值)→ 判 → 按原值 CAS / 缺席时 SET NX。
// 只增不减、世代不同不写(那种情况 AdvanceReconcile 已冻结);版本更新的值不碰。CAS 输了下一拍再来。
func (r *Reconciler) advanceWatermarkKey(ctx context.Context, wm watermarkState, res store.ReconcileTickResult) {
	if res.WatermarkMs == 0 || res.Generation == "" {
		return
	}
	switch wm.kind {
	case keyNewer:
		return
	case keyValid:
		if wm.w.Generation != res.Generation || wm.w.WatermarkMs >= res.WatermarkMs {
			return
		}
	}
	next, err := placement.EncodeRouteWatermark(placement.RouteWatermark{Generation: res.Generation, WatermarkMs: res.WatermarkMs})
	if err != nil {
		logx.Errorf("[RouteReconcile] encode the watermark key: %v", err)
		return
	}
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	ok, err := r.cache.AdvanceWatermark(opCtx, wm.raw, next)
	cancel()
	switch {
	case err != nil:
		logx.Errorf("[RouteReconcile] advance %s: %v", placement.RouteWatermarkKey, err)
	case !ok:
		logx.Infof("[RouteReconcile] %s changed under us; retried next tick", placement.RouteWatermarkKey)
	}
}

// evictionState:bad = 淘汰可能发生(策略不是 noeviction)或已经发生(evicted_keys 比基线大、或取不到);
// taint = 确实发生过淘汰(比基线大)—— 等待中的挂标记候选作废。
func (r *Reconciler) evictionState(p Probe) (bad, taint bool) {
	policyOK := p.MaxmemoryPolicy == "noeviction" || (p.MaxmemoryPolicy == "" && r.opts.assumeNoEviction)
	switch {
	case p.EvictedKeys < 0 || !r.baselineSet:
		return true, false
	case p.EvictedKeys > r.evictedBaseline:
		return true, true
	}
	return !policyOK, false
}

// blockers 是挂标记的阻塞原因(规格 scout D3 + §13.1 第 3、5 条)。
type blockers struct {
	eviction, debug, fenceMirror, unresolved bool
}

func (b blockers) any() bool { return b.eviction || b.debug || b.fenceMirror || b.unresolved }

// state 报告是否有「状态类」原因(与任务结果无关,不跑预热也清不掉)。
func (b blockers) state() bool { return b.eviction || b.debug || b.fenceMirror }

func (b blockers) key() string {
	var parts []string
	if b.eviction {
		parts = append(parts, "eviction")
	}
	if b.debug {
		parts = append(parts, "debug_enabled")
	}
	if b.fenceMirror {
		parts = append(parts, "fence_mirror")
	}
	if b.unresolved {
		parts = append(parts, "unresolved")
	}
	return strings.Join(parts, ",")
}

// reportRefusal 在阻塞原因变化时计事件、打 ERROR(每拍都在拒绝,只在变化时计数,免得计数器按秒涨)。
func (r *Reconciler) reportRefusal(b blockers) {
	k := b.key()
	if k == r.lastRefusal {
		return
	}
	r.lastRefusal = k
	if b.eviction {
		r.obs.Event(metrics.RouteReconcileEventArmingRefusedEviction)
	}
	if b.debug {
		r.obs.Event(metrics.RouteReconcileEventArmingRefusedDebugEnabled)
	}
	if b.unresolved || b.fenceMirror {
		r.obs.Event(metrics.RouteReconcileEventArmingRefusedUnresolved)
	}
	sample := r.pendingIDs()
	if len(sample) > unresolvedLogSample {
		sample = sample[:unresolvedLogSample]
	}
	logx.Errorf("[RouteReconcile] arming %s REFUSED (armingBlocked: no new full prewarm; incrementals re-read the unresolved rows): "+
		"reasons=%s pending=%d overflow=%t sample=%v", placement.RouteReadyKey, k, len(r.pending), r.pendingOverflow, sample)
}

// decideDisarmed 是 Arm=false 的第 7 步:VI 为空(或身份变了)排追平。追平起点:水位键不在 = 0(全量),否则 max(0, cvm − RepairLookbackMs)。
func (r *Reconciler) decideDisarmed(ctx context.Context, ctl Control, p Probe) {
	if r.vi != nil && *r.vi != p.Identity {
		logx.Errorf("[RouteReconcile] redis identity changed (run_id %s → %s); catching up", r.vi.RunID, p.Identity.RunID)
		r.vi = nil
	}
	if r.vi != nil || r.running != nil || r.startedThisTick {
		return
	}
	from := satSub(r.cvm, r.opts.lookbackMs)
	if ctl.Watermark == nil {
		from = 0
	}
	r.startTask(ctx, task{phase: phaseCatchUp, fromMs: from, identity: p.Identity})
}

// decideArmed 是 Arm=true 的第 8 步(规格 §4.3 第 8 步按 §13.1 第 1 条改写)。
func (r *Reconciler) decideArmed(ctx context.Context, ms *markerState, p Probe, b blockers, evTaint bool) {
	switch ms.kind {
	case keyNewer:
		r.vi = nil
		return
	case keyValid:
		if ms.m.RedisRunID == p.Identity.RunID && ms.m.MasterReplID == p.Identity.MasterReplID {
			id := p.Identity
			r.vi = &id
			r.candidate, r.latch, r.queuedRepair, r.lastRefusal = nil, nil, nil, ""
			r.raiseMarkerWatermark(ctx, ms)
			return
		}
		// 标记在、身份不符:自身 AOF / RDB 重启、副本提升 —— 标记随数据一起延续,回看修补成立(M5)。先关门(CAD),再修补。
		repairFrom := satSub(min(ms.m.WatermarkMs, r.cvm), r.opts.lookbackMs)
		r.vi, r.candidate, r.latch, r.lastRefusal = nil, nil, nil, ""
		if !r.dropMarker(ctx, ms, metrics.RouteReconcileEventMarkerDroppedIdentity,
			fmt.Sprintf("redis identity changed (marker run_id=%s replid=%s, now run_id=%s replid=%s)",
				ms.m.RedisRunID, ms.m.MasterReplID, p.Identity.RunID, p.Identity.MasterReplID)) {
			return // 门没关上就不修补:下一拍重来
		}
		r.queuedRepair = &task{phase: phaseRepair, fromMs: repairFrom, identity: p.Identity}
		if r.running != nil {
			r.running.abort("identity changed; repair queued")
			return
		}
		r.startQueuedRepair(ctx, p)
		return
	}

	// 标记不在。
	r.vi = nil
	if r.running != nil || r.startedThisTick {
		return
	}
	if r.queuedRepair != nil {
		r.startQueuedRepair(ctx, p)
		return
	}
	b.unresolved = len(r.pending) > 0 || r.pendingOverflow
	if r.latch != nil {
		if r.latch.identity != p.Identity {
			logx.Errorf("[RouteReconcile] redis identity changed while arming was blocked; full prewarm")
			r.latch, r.candidate, r.lastRefusal = nil, nil, ""
		} else {
			if evTaint {
				r.latch.evictionTaint = true
			}
			if b.any() {
				r.reportRefusal(b)
				return
			}
			l := r.latch
			r.latch, r.lastRefusal = nil, ""
			if l.candidate != nil && !l.evictionTaint && !r.pendingOverflow {
				r.tryArm(ctx, l.candidate, p)
				return
			}
			logx.Infof("[RouteReconcile] arming unblocked but the blocked result is not usable (candidate=%t eviction_taint=%t overflow=%t); full prewarm",
				l.candidate != nil, l.evictionTaint, r.pendingOverflow)
			r.candidate = nil
		}
	}
	if c := r.candidate; c != nil {
		if c.identity != p.Identity || c.generation != r.generation {
			logx.Errorf("[RouteReconcile] the %s result no longer matches (identity %t generation %t); full prewarm",
				c.phase, c.identity == p.Identity, c.generation == r.generation)
			r.candidate = nil
		} else {
			if b.any() {
				r.latch = &armLatch{candidate: c, identity: p.Identity, evictionTaint: evTaint}
				r.candidate = nil
				r.reportRefusal(b)
				return
			}
			r.candidate = nil
			r.tryArm(ctx, c, p)
			return
		}
	}
	if b.state() {
		// 淘汰 / DEBUG / 围栏镜像这类状态原因下,全量预热挂不上标记:先闩住,原因清掉再预热(不白跑一遍全表)。
		r.latch = &armLatch{identity: p.Identity, evictionTaint: evTaint}
		r.reportRefusal(b)
		return
	}
	r.startTask(ctx, task{phase: phasePrewarm, identity: p.Identity})
}

func (r *Reconciler) startQueuedRepair(ctx context.Context, p Probe) {
	q := r.queuedRepair
	if q.identity != p.Identity {
		logx.Errorf("[RouteReconcile] redis identity changed again before the queued repair started; the marker is absent, so a full prewarm follows")
		r.queuedRepair = nil
		return
	}
	if r.startTask(ctx, *q) {
		r.queuedRepair = nil
	}
}

// raiseMarkerWatermark:标记在且可信时,把标记的 watermark_ms 推进到 cvm(规格 §13.1 第 2 条:标记水位 = 缓存已验证到的点)。只增不减。
func (r *Reconciler) raiseMarkerWatermark(ctx context.Context, ms *markerState) {
	if r.cvm == 0 || r.cvm <= ms.m.WatermarkMs {
		return
	}
	next := ms.m
	next.WatermarkMs = r.cvm
	raw, err := placement.EncodeRouteReadyMarker(next)
	if err != nil {
		logx.Errorf("[RouteReconcile] encode the marker with watermark_ms=%d: %v", r.cvm, err)
		return
	}
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	ok, err := r.cache.CompareAndSetReady(opCtx, ms.raw, raw)
	cancel()
	switch {
	case err != nil:
		logx.Errorf("[RouteReconcile] compare-and-set the marker watermark: %v", err)
	case ok:
		ms.raw, ms.m = raw, next
	}
}

// tryArm 挂标记(规格 §4.3 末段、§13.1 第 7 条):再 Peek 一次库租约(必须仍是自己)→ RouteReadyArmScript(令牌、标记值、身份)
// + 同连接 WAITAOF → 复读身份,变了就按原值撤掉自己的值。标记水位写 cvm,set_at_ms 取库时钟。
func (r *Reconciler) tryArm(ctx context.Context, c *armCandidate, p Probe) {
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	lease, err := r.st.PeekReconcileLease(opCtx)
	cancel()
	if err != nil {
		logx.Errorf("[RouteReconcile] arming postponed: peek lease: %v", err)
		r.candidate = c
		return
	}
	if !lease.HeldBy(r.leaderID) {
		r.demote(fmt.Sprintf("the lease is held by %q at arming time", lease.Holder))
		return
	}
	if r.cvm == 0 {
		logx.Errorf("[RouteReconcile] arming postponed: cache_verified_ms is still 0")
		r.candidate = c
		return
	}
	marker := placement.RouteReadyMarker{
		V:            placement.RouteReadyFormatVersion,
		Generation:   r.generation,
		RedisRunID:   c.identity.RunID,
		MasterReplID: c.identity.MasterReplID,
		SetAtMs:      lease.NowMs,
		WatermarkMs:  r.cvm,
		Leader:       r.leaderID,
	}
	opCtx, cancel = context.WithTimeout(ctx, tickOpTimeout)
	out, err := r.cache.ArmReady(opCtx, c.token, marker)
	cancel()
	if err != nil {
		logx.Errorf("[RouteReconcile] arming failed (retried next tick): %v", err)
		r.candidate = c
		return
	}
	switch out {
	case placement.RouteReadyArmArmed:
	case placement.RouteReadyArmTokenMismatch:
		logx.Errorf("[RouteReconcile] arming refused by the warm token (FLUSH / rewarm / another leader since the %s started): "+
			"the data may be incomplete; full prewarm next tick", c.phase)
		r.vi = nil
		return
	case placement.RouteReadyArmAlreadyPresent:
		logx.Infof("[RouteReconcile] arming found %s already present; re-judged next tick", placement.RouteReadyKey)
		return
	case placement.RouteReadyArmIdentityChanged:
		logx.Errorf("[RouteReconcile] arming refused: the redis identity is no longer the one the %s verified", c.phase)
		r.vi = nil
		return
	default:
		logx.Errorf("[RouteReconcile] arming returned %s (internal bug)", out)
		return
	}
	// 挂上了:复读身份,变了就撤掉自己写的值(scout D3)。
	raw, encErr := placement.EncodeRouteReadyMarker(marker)
	after, err := r.probe(ctx)
	if err != nil || after.Identity != c.identity {
		if encErr == nil {
			opCtx, cancel = context.WithTimeout(ctx, tickOpTimeout)
			_, derr := r.cache.CompareAndDeleteReady(opCtx, raw)
			cancel()
			if derr != nil {
				logx.Errorf("[RouteReconcile] dropping our own marker after an identity re-check failure also failed: %v", derr)
			}
		}
		r.obs.Event(metrics.RouteReconcileEventMarkerDroppedIdentity)
		logx.Errorf("[RouteReconcile] armed, but the identity re-check failed (err=%v); our marker was dropped", err)
		r.vi = nil
		return
	}
	id := c.identity
	r.vi = &id
	r.lastRefusal = ""
	r.obs.Event(metrics.RouteReconcileEventMarkerArmed)
	logx.Infof("[RouteReconcile] ARMED %s after %s: generation=%s run_id=%s watermark_ms(cvm)=%d set_at_ms=%d",
		placement.RouteReadyKey, c.phase, r.generation, c.identity.RunID, r.cvm, lease.NowMs)
}

// reconcileFenceMirrors 是围栏镜像双向对账(规格 §13.1 第 3 条)。返回 ok(本轮对账没有任何失败)与 foreign(有解不出 fence_epoch 的镜像)。
//   - 库里生效的围栏(63 号片,库时钟判):用 set-if-newer 脚本补 / 续镜像(PEXPIREAT = 库 expires_at_ms + 10 分钟);
//   - Redis 里有、库里对应 zone 不生效的:**复读库**确认仍不生效后按完整值 CAD(挡住「库刚获取、镜像刚写」的竞态);
//   - 值解不出 fence_epoch 的(旧版 merge_zone 写的、手工写的)、键后缀不是 zone 的:不删,计 foreign_fence。
func (r *Reconciler) reconcileFenceMirrors(ctx context.Context) (ok bool, foreign bool) {
	ok = true
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	defer cancel()
	fences, err := r.st.ListActiveMergeFences(opCtx)
	if err != nil {
		logx.Errorf("[RouteReconcile] fence mirror: list active zone_merge_fence: %v", err)
		r.obs.Mirror(metrics.RouteReconcileMirrorError, 1)
		return false, false
	}
	active := make(map[uint32]bool, len(fences.Fences))
	var foreignKeys []string
	for _, f := range fences.Fences {
		active[f.ZoneID] = true
		if f.ExpiresAtMs == 0 {
			logx.Errorf("[RouteReconcile] fence mirror: zone %d fence run_id=%s has no expires_at_ms (CHECK not enforced?); mirror not written", f.ZoneID, f.RunID)
			r.obs.Mirror(metrics.RouteReconcileMirrorError, 1)
			ok = false
			continue
		}
		out, err := r.cache.SetFenceMirror(opCtx, f.ZoneID, f.HolderJSON, f.FenceEpoch, f.ExpiresAtMs)
		switch {
		case err != nil:
			logx.Errorf("[RouteReconcile] fence mirror: zone %d run_id=%s epoch=%d: %v", f.ZoneID, f.RunID, f.FenceEpoch, err)
			r.obs.Mirror(metrics.RouteReconcileMirrorError, 1)
			ok = false
		case out == placement.MergeFenceMirrorWritten:
			r.obs.Mirror(metrics.RouteReconcileMirrorFenceWritten, 1)
		case out == placement.MergeFenceMirrorStale:
			// 镜像的 fence_epoch 比库里的大:库丢了一次围栏获取(回档?)。镜像只会多封锁,不动它;但这一轮不算对账成功。
			logx.Errorf("[RouteReconcile] fence mirror: zone %d mirror carries a fence_epoch newer than the database's %d (database rollback?); left as is",
				f.ZoneID, f.FenceEpoch)
			r.obs.Mirror(metrics.RouteReconcileMirrorFenceStale, 1)
			ok = false
		case out == placement.MergeFenceMirrorForeign:
			r.obs.Mirror(metrics.RouteReconcileMirrorFenceForeign, 1)
			r.obs.Event(metrics.RouteReconcileEventForeignFence)
			foreignKeys = append(foreignKeys, placement.MergeFenceKey(f.ZoneID))
			foreign = true
		}
	}
	mirrors, err := r.cache.ListFenceMirrors(opCtx)
	if err != nil {
		logx.Errorf("[RouteReconcile] fence mirror: SCAN merge:in_progress:*: %v", err)
		r.obs.Mirror(metrics.RouteReconcileMirrorError, 1)
		return false, foreign
	}
	var stale []FenceMirror
	for _, m := range mirrors {
		if m.Zone != 0 && active[m.Zone] {
			continue
		}
		if _, hasEpoch := placement.MergeFenceHolderEpoch(m.Value); m.Zone == 0 || m.WrongType || !hasEpoch {
			r.obs.Mirror(metrics.RouteReconcileMirrorFenceForeign, 1)
			r.obs.Event(metrics.RouteReconcileEventForeignFence)
			foreignKeys = append(foreignKeys, m.Key)
			foreign = true
			continue
		}
		stale = append(stale, m)
	}
	r.logForeignFences(foreignKeys)
	if len(stale) == 0 {
		return ok, foreign
	}
	recheck, err := r.st.ListActiveMergeFences(opCtx)
	if err != nil {
		logx.Errorf("[RouteReconcile] fence mirror: re-read active fences before deleting %d stale mirror(s): %v", len(stale), err)
		r.obs.Mirror(metrics.RouteReconcileMirrorError, 1)
		return false, foreign
	}
	stillActive := make(map[uint32]bool, len(recheck.Fences))
	for _, f := range recheck.Fences {
		stillActive[f.ZoneID] = true
	}
	for _, m := range stale {
		if stillActive[m.Zone] {
			continue // 两次读之间刚获取的围栏:下一拍按库值写镜像
		}
		deleted, err := r.cache.CompareAndDeleteFenceMirror(opCtx, m.Key, m.Value)
		switch {
		case err != nil:
			logx.Errorf("[RouteReconcile] fence mirror: compare-and-delete %s: %v", m.Key, err)
			r.obs.Mirror(metrics.RouteReconcileMirrorError, 1)
			ok = false
		case deleted:
			r.obs.Mirror(metrics.RouteReconcileMirrorFenceDeleted, 1)
			logx.Infof("[RouteReconcile] fence mirror: deleted %s (its fence is no longer active in zone_merge_fence)", m.Key)
		default:
			r.obs.Mirror(metrics.RouteReconcileMirrorFenceDeleteLost, 1)
		}
	}
	return ok, foreign
}

func (r *Reconciler) logForeignFences(keys []string) {
	sort.Strings(keys)
	k := strings.Join(keys, ",")
	if k == r.lastForeignFence {
		return
	}
	r.lastForeignFence = k
	if k == "" {
		logx.Infof("[RouteReconcile] fence mirror: no foreign merge fence mirror left")
		return
	}
	logx.Errorf("[RouteReconcile] FOREIGN merge fence mirror(s) without a fence_epoch (old merge_zone binary or a manual write; never deleted, "+
		"arming is refused until they are gone): %s", k)
}

// observe 是第 10 步:自检(RouteReadyReadScript 不带映射键,读方同一判据)并写 leader 观测型 gauge(规格 §13.1 第 8 条)。
func (r *Reconciler) observe(ctx context.Context, writable, markerPresent bool) {
	verdict := metrics.RouteReadySelfCheckScriptError
	opCtx, cancel := context.WithTimeout(ctx, tickOpTimeout)
	rr, err := r.cache.SelfCheck(opCtx)
	cancel()
	if err != nil {
		logx.Errorf("[RouteReconcile] self-check (route ready read script) failed: %v", err)
	} else {
		v, _ := placement.VerifyRouteReady(rr)
		verdict = v.String()
		markerPresent = rr.MarkerPresent
	}
	age := math.Inf(1)
	if r.cvm > 0 {
		age = float64(satSub(r.dbNow, r.cvm)) / 1000
	}
	r.obs.LeaderObservations(markerPresent, writable, age, verdict)
}

// satSub 是饱和减法(规格 §4.2 must-1:所有 X − Lookback / Overlap 饱和到 0,不回绕)。
func satSub(a, b uint64) uint64 {
	if a <= b {
		return 0
	}
	return a - b
}

// newWarmToken 生成 32 个小写十六进制字符的预热令牌(placement.ValidRouteWarmToken)。
func newWarmToken(rnd io.Reader) (string, error) {
	var b [placement.RouteWarmTokenLen / 2]byte
	if _, err := io.ReadFull(rnd, b[:]); err != nil {
		return "", fmt.Errorf("read random warm token: %w", err)
	}
	return hex.EncodeToString(b[:]), nil
}
