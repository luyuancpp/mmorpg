package logic

import (
	"context"
	"errors"
	"fmt"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/proc"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/config"
	"player_locator/internal/svc"
	pb "proto/player_locator"
	"shared/leader"
	"shared/safego"
)

// 会话对账:兜住「gate 整机崩溃 → TCP 断开回调不执行 → SetDisconnecting
// 永远不来 → 会话永久 ONLINE」的结构性缺口。
//
// 会话进入清理状态机的唯一入口是 SetDisconnecting(它的 Lua 才会 ZADD 租约
// ZSET,LeaseMonitor 只消费该 ZSET),而会话键是无 TTL 写入的。断线通知链
// 三处都可能整段蒸发:gate 崩溃(回调不执行)、断线瞬间无 login 节点
// (gate 静默跳过)、login→locator RPC 穷尽重试仍失败。落到这里的会话没有
// 别的自愈路径;gate 活着而通知丢失的那一类对账同样看不到,只能按
// release-checklist F.B-1 的定向补救人工处理。
//
// 对账原则(fail-closed,宁可漏扫不可误杀):
//  1. 只动 State==ONLINE 且 GateInstanceId 指向死 gate 的会话;
//  2. gate 存活性以 etcd 注册表为准(C++ gate 以 lease 注册 NodeInfo,
//     node_uuid 即会话里的 GateInstanceId);etcd 列举失败 → 本拍不推进去抖、不转换;
//  3. 同一 gate 必须**连续两拍**都不在存活集内才动手 —— 单拍缺席可能只是 etcd 视图
//     抖动或 gate 正在重启重注册;
//  4. 动手 = 走与 SetDisconnecting 完全相同的提交点 transitionToDisconnecting(整字节 CAS +
//     版本号递进 + ZADD 租约),期间玩家经新 gate 重连/重登会改写会话字节,
//     CAS 自动失败成 no-op —— 绝不会踢掉活人。
//
// 怎么做(2026-10-18 起,docs/design/player-locator-gate-session-index.md):
//   - 会话写入在同一段 Lua 里维护「gate 实例 → 会话」索引(session_cas.go)。对账只读死 gate 的索引集合,
//     不再 SCAN 全键空间:旧实现每个副本每 60s 扫一遍,2000 万在线时比核心 Redis 其余流量高两个数量级。
//   - 全服一个 leader(shared/leader,Redis 锁;作用域 = 一个会话 Redis 域)。选主只为削减负载,不是正确性 fence:
//     两个 leader 短暂重叠时,所有动作都走整字节 CAS 或字节守卫 Lua,重叠无害。
//   - 选主锁只看 Redis,leader 自己看不清 etcd / Redis 时必须主动让位(连续 3 拍失败,或 3 拍内没有一次成功),
//     本进程多等一个 TTL 再竞选,让健康副本先接手;否则 etcd 单边不通的 leader 会让全服对账无声停摆。
//   - 漏进索引的会话(旧版写者、外部篡改)靠补建 / 审计扫描补:就绪标记缺失时连续跑限速的全键扫描,
//     就绪后按 AuditIntervalSeconds 跑。扫描只补索引、清脏成员,不转换会话 —— 转换只有索引循环一条路径。
//   - 就绪标记是租约(只由「本拍 etcd 读成功且零旧写者」的 leader 续期)+ Redis 侧代次(看到旧写者就 INCR,
//     补建结束时「代次未变才设标记」)。一次性回滚、换主、两个 leader 重叠时标记都不会错误地留着。

const (
	defaultReconcileInterval = 60 * time.Second
	// reconcilerLeaderTTL 是选主锁的 TTL,也是让位后本进程再竞选前多等的时长。
	reconcilerLeaderTTL = 30 * time.Second
	// maxConsecutiveRoundFailures / staleSuccessIntervals 是 leader 自我让位的两个门槛:
	// 连续这么多拍失败,或距上次成功超过这么多拍。
	maxConsecutiveRoundFailures = 3
	staleSuccessIntervals       = 3

	// maxKeyTypeErrorsPerRound:一拍里最多吸收这么多单键错误(WRONGTYPE,逐个跳过),再多就按实例级故障整拍失败、
	// 计入让位门槛。几个被篡改的键不该让全服对账停摆;成百上千个说明不是「个别键坏了」,要喊出来而不是一拍拍报 ok。
	maxKeyTypeErrorsPerRound = 100
	// keyErrorLogLimit:一拍 / 一遍里逐条打 ERROR 的单键错误数,其余只计数,在拍 / 遍的汇总日志里出现。
	keyErrorLogLimit = 10

	// 补建 / 审计一遍扫描内,单页失败时对同一游标重试:最多 passPageMaxAttempts 次,退避从 passPageRetryBase
	// 翻倍到 passPageRetryMax。一遍约 9 小时、约 2e7 条命令,一次没被 go-redis 重试吸收的抖动不该让整遍作废。
	// 只有一个 leader 在扫,不存在多个客户端同时重试的惊群,所以不加抖动。
	passPageMaxAttempts = 5
	passPageRetryBase   = time.Second
	passPageRetryMax    = 30 * time.Second
	// auditFailureCooldown:审计失败后至少隔这么久再试(Redis 侧 EX,换主不重置),不让持续的故障每拍重开一遍全键扫描。
	// 审计周期更短时取审计周期。
	auditFailureCooldown = time.Hour
)

// ReconcilerParams 是 StartSessionReconciler 的装配参数。
type ReconcilerParams struct {
	// EtcdClient 复用 Node 注册用的客户端(生命周期归 Node);nil 时对账不运行。
	EtcdClient *clientv3.Client
	// IntervalSeconds 即 Lease.ReconcileIntervalSeconds:0 = 60s,-1 = 关闭。
	IntervalSeconds int64
	// Reconcile 即 Lease.Reconcile(这里再 Normalized 一次,幂等)。
	Reconcile config.ReconcileConf
	// LeaderID 只进选主锁值前缀与日志,方便 redis-cli GET 一眼看出 leader 是哪个 Pod。
	LeaderID string
}

// reconcilerDeps 是对账循环的外部依赖:生产由 productionReconcilerDeps 装配,单测注入假实现(固定 now、
// 不限速的 pacer、受控的拍),不依赖墙钟。
type reconcilerDeps struct {
	view     clusterView
	now      func() time.Time
	newPacer func(perSecond int) pacer
	// newTicker 返回「等到下一拍」与它的停止函数。
	newTicker func(interval time.Duration) (wait func(ctx context.Context) error, stop func())
	// sleep 等一段时间或 ctx 结束:补建循环的间隔、让位后再竞选前的等待、扫描单页重试的退避。
	sleep func(ctx context.Context, d time.Duration) error
	// redisRunID 读会话 Redis 进程的 run_id:一遍完整扫描开始与结束各读一次,变了(主从切换 / 重启)
	// 就不认这一遍的覆盖结果(SCAN 游标只在同一个进程里有覆盖保证)。
	redisRunID func(ctx context.Context, rdb *redis.Client) (string, error)
}

func productionReconcilerDeps(cli *clientv3.Client) reconcilerDeps {
	return reconcilerDeps{
		view:       newEtcdClusterView(cli),
		now:        time.Now,
		newPacer:   newIntervalPacer,
		newTicker:  newRealTicker,
		sleep:      sleepCtx,
		redisRunID: readRedisRunID,
	}
}

// readRedisRunID 经 INFO server 读 Redis 进程的 run_id。
//
// 为什么要它:SCAN / SSCAN 的「整遍覆盖开始时就在的全部元素」只在同一个 Redis 进程里成立 —— 字典的 siphash 种子
// 每个进程随机,主从切换或重启之后,旧游标在新进程上指向的是另一批桶,续扫会漏键且不报错。go-redis 自带的重试
// 本来就可能把一次扫描跨到新主上,单页重试(passPageMaxAttempts)让这个窗口更长,所以一遍结束时核对一次。
// 拿不到 run_id(INFO 被禁用、托管 Redis 不返回该字段)按出错处理:这一遍不设就绪标记 / 不记审计完成(fail-closed),
// 代价是补建永远不会宣布就绪,由 PlayerLocatorGateIndexNotReady 告警暴露。
func readRedisRunID(ctx context.Context, rdb *redis.Client) (string, error) {
	info, err := rdb.Info(ctx, "server").Result()
	if err != nil {
		return "", err
	}
	id, ok := parseRedisRunID(info)
	if !ok {
		return "", errors.New("run_id not found in INFO server")
	}
	return id, nil
}

// parseRedisRunID 从 INFO 文本里取 run_id(纯函数)。
func parseRedisRunID(info string) (string, bool) {
	for _, line := range strings.Split(info, "\n") {
		if id, ok := strings.CutPrefix(strings.TrimSpace(line), "run_id:"); ok && id != "" {
			return id, true
		}
	}
	return "", false
}

func newRealTicker(interval time.Duration) (func(ctx context.Context) error, func()) {
	t := time.NewTicker(interval)
	return func(ctx context.Context) error {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-t.C:
			return nil
		}
	}, t.Stop
}

func sleepCtx(ctx context.Context, d time.Duration) error {
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

// StartSessionReconciler 竞选会话对账 leader,任期内跑索引循环与补建 / 审计循环。阻塞到 ctx 结束,调用方 go 出去。
//
// 必须在 go-zero 的 Prometheus agent 启动之后(zrpc.MustNewServer 之后)调用:agent 启动前的指标写入会被丢弃,
// 预置 0 与 is_leader 就落空了(reconcile_metrics.go 文件头)。
func StartSessionReconciler(ctx context.Context, svcCtx *svc.ServiceContext, p ReconcilerParams) {
	if p.IntervalSeconds < 0 {
		logx.Info("SessionReconciler disabled by config")
		return
	}
	if p.EtcdClient == nil {
		// 看不到 etcd 只能放弃对账(fail-closed:不清理),但要喊出来 ——
		// 这是 gate 崩溃兜底的唯一自动化路径。
		logx.Error("SessionReconciler: etcd client unavailable, reconciler NOT running")
		return
	}
	interval := defaultReconcileInterval
	if p.IntervalSeconds > 0 {
		interval = time.Duration(p.IntervalSeconds) * time.Second
	}
	cfg := p.Reconcile.Normalized()
	primeReconcileMetrics()

	elector := leader.New(leader.NewGoRedisStore(svcCtx.RedisClient), reconcilerLeaderKey, leader.Options{
		TTL:           reconcilerLeaderTTL,
		ID:            p.LeaderID,
		OnStateChange: onReconcilerLeaderChange,
	})
	// 优雅退出时主动让位,不把锁留到 TTL 自然过期(scene_manager 同法):go-zero 的 SIGTERM 走 os.Exit,
	// main 的 defer 不执行。
	proc.AddShutdownListener(elector.Stop)

	deps := productionReconcilerDeps(p.EtcdClient)
	logx.Infof("SessionReconciler started: interval=%v leader_key=%s id=%s audit_interval_s=%d scan_count=%d "+
		"scan_pages_per_s=%d transitions_per_s=%d dead_gate_members_per_round=%d",
		interval, reconcilerLeaderKey, p.LeaderID, cfg.AuditIntervalSeconds, cfg.ScanCount,
		cfg.ScanPagesPerSecond, cfg.MaxTransitionsPerSecond, cfg.DeadGateMembersPerRound)
	campaignSessionReconciler(ctx, elector.Run, reconcilerLeaderTTL, deps.sleep, func(lctx context.Context) bool {
		return newSessionReconciler(svcCtx, cfg, interval, deps).run(lctx)
	})
	logx.Info("SessionReconciler stopped")
}

// reconcilerCampaign 是「竞选并在任期内运行 whileLeader」的函数形状,生产为 (*leader.Elector).Run。
type reconcilerCampaign func(ctx context.Context, whileLeader func(ctx context.Context))

// campaignSessionReconciler 反复竞选,任期内跑 term。term 返回 true 表示主动让位:结束这次 campaign
// (选主器带属主校验释放锁),本进程等 yieldBackoff 再竞选,让健康副本先抢到锁。
// term 返回 false(丢锁 / 关停)时由选主器自己的循环继续竞选;campaign 返回而没有让位 = ctx 结束或选主器已 Stop。
func campaignSessionReconciler(
	ctx context.Context,
	campaign reconcilerCampaign,
	yieldBackoff time.Duration,
	sleep func(ctx context.Context, d time.Duration) error,
	term func(ctx context.Context) bool,
) {
	for ctx.Err() == nil {
		runCtx, cancel := context.WithCancel(ctx)
		yielded := false // 只在 campaign 同步调用的 whileLeader 里写,与这里同一个 goroutine
		campaign(runCtx, func(lctx context.Context) {
			if term(lctx) {
				yielded = true
				cancel()
			}
		})
		cancel()
		if !yielded {
			return
		}
		logx.Errorf("SessionReconciler: leadership yielded, waiting %v before campaigning again", yieldBackoff)
		if err := sleep(ctx, yieldBackoff); err != nil {
			return
		}
	}
}

// sessionReconciler 是一个 leader 任期内的对账状态。换主即新建,去抖表与游标随之重置(最多推迟一拍)。
type sessionReconciler struct {
	svcCtx   *svc.ServiceContext
	rdb      *redis.Client
	cfg      config.ReconcileConf
	interval time.Duration
	deps     reconcilerDeps
	// 两个限速器各归一个 goroutine:transitionPacer 只给索引循环,scanPacer 只给补建 / 审计循环。
	transitionPacer pacer
	scanPacer       pacer
	// memberBudget 是一拍最多检查的死 gate 索引成员数(DeadGateMembersPerRound,再被转换限速 × 半拍截住)。
	memberBudget int

	// 以下只由索引循环访问(单 goroutine)。
	suspects map[string]int    // gate uuid → 连续缺席拍数(封顶 2)
	cursors  map[string]uint64 // 已确认死亡、被预算截断的 gate → 下一拍续扫的 SSCAN 游标
}

func newSessionReconciler(svcCtx *svc.ServiceContext, cfg config.ReconcileConf, interval time.Duration, deps reconcilerDeps) *sessionReconciler {
	budget := cfg.DeadGateMembersPerRound
	if cfg.MaxTransitionsPerSecond > 0 {
		// 限速调低时,一拍的转换耗时不超过半个拍长,拍不会因为在限速里排队而超时(一拍的 ctx 超时就是一个拍长)。
		// 代价:大面积 gate 故障时的**持续**转换速率约为 MaxTransitionsPerSecond / 2(每拍只用半拍去转换),
		// 估恢复时间按这个算(设计文档 §7)。不放宽到更大比例:超时的拍计入让位门槛。
		capped := int(float64(cfg.MaxTransitionsPerSecond) * interval.Seconds() / 2)
		budget = max(min(budget, capped), 1)
	}
	return &sessionReconciler{
		svcCtx:          svcCtx,
		rdb:             svcCtx.RedisClient,
		cfg:             cfg,
		interval:        interval,
		deps:            deps,
		transitionPacer: deps.newPacer(cfg.MaxTransitionsPerSecond),
		scanPacer:       deps.newPacer(cfg.ScanPagesPerSecond),
		memberBudget:    budget,
		suspects:        make(map[string]int),
		cursors:         make(map[string]uint64),
	}
}

// readyLease 是就绪标记的租约:3 拍 + 半拍余量。连续两拍失败后第三拍按时续上,不与过期赛跑;
// leader 连续三拍失败会让位,接任者通常在过期前跑完第一拍并续上;没有任何新版 leader 在跑
// (一次性回滚到旧版、对账被关掉)时,标记最迟 3.5 拍后自然过期,下一任新版 leader 从补建开始。
func (r *sessionReconciler) readyLease() time.Duration {
	return staleSuccessIntervals*r.interval + r.interval/2
}

// run 是一个任期:起补建 / 审计循环,本 goroutine 跑索引循环。返回 true = 主动让位;false = 任期结束(lctx 取消)。
// 返回前取消并等补建循环退出:同一进程再次当选时不会有两个补建循环并发,前任在途的写也不会落在新任之后。
func (r *sessionReconciler) run(lctx context.Context) bool {
	// 每个任期从 0 开始(含 last_success{loop="index"}):Stalled 靠「现任在第一拍成功之前读数为 0」才能在
	// 「每一任都失败」时持续成立。启动预置与上一任期收尾时已清过一次,这里再清一次,不依赖那两处一定生效过。
	resetReconcileLeaderGauges()
	rctx, cancel := context.WithCancel(lctx)
	var wg sync.WaitGroup
	defer func() {
		cancel()
		wg.Wait()
		resetReconcileLeaderGauges()
	}()
	wg.Add(1)
	safego.Go("player_locator.session_reconciler.repair", func() {
		defer wg.Done()
		r.repairLoop(rctx)
	})

	wait, stop := r.deps.newTicker(r.interval)
	defer stop()
	lastSuccess := r.deps.now() // 任期开始作为「上次成功」的基线
	failures := 0
	for {
		reconcileIsLeader.Set(1) // 每拍重申:当选时的那次写可能早于 Prometheus agent 启动而被丢弃
		outcome := r.runIndexRound(rctx)
		if rctx.Err() != nil {
			return false // 丢锁 / 关停造成的取消不计数
		}
		now := r.deps.now()
		reconcileRoundTotal.Inc(reconcileLoopIndex, outcome.result())
		if outcome.err == nil {
			failures = 0
			lastSuccess = now
			reconcileLastSuccessUnix.Set(float64(now.Unix()), reconcileLoopIndex)
		} else {
			failures++
			logx.Errorf("SessionReconciler: index round failed stage=%s consecutive_failures=%d: %v",
				outcome.stage, failures, outcome.err)
		}
		if failures >= maxConsecutiveRoundFailures || now.Sub(lastSuccess) > staleSuccessIntervals*r.interval {
			logx.Errorf("SessionReconciler: yielding leadership: consecutive_failures=%d since_last_success=%v last_stage=%s",
				failures, now.Sub(lastSuccess), outcome.stage)
			return true
		}
		if err := wait(rctx); err != nil {
			return false
		}
	}
}

// ---------------------------------------------------------------------------
// 索引循环
// ---------------------------------------------------------------------------

// roundStage 是一拍失败所在的阶段(只进日志与 result label)。
type roundStage string

const (
	stageEtcd  roundStage = "etcd"
	stageRedis roundStage = "redis"
	stagePanic roundStage = "panic"
)

type roundOutcome struct {
	stage  roundStage
	err    error
	report indexRoundReport
}

func (o roundOutcome) result() string {
	switch {
	case o.err == nil:
		return reconcileResultOK
	case o.stage == stageEtcd:
		return reconcileResultEtcdError
	case o.stage == stagePanic:
		return reconcileResultPanic
	default:
		return reconcileResultRedisError
	}
}

// indexRoundReport 是一拍的统计(日志与单测都看它;指标后端在单测里不生效)。
type indexRoundReport struct {
	legacyWriters   int
	readyRenewed    bool
	deadGates       []string // 本拍确认死亡的 gate(含本拍已释放的)
	released        []string // 本拍索引已空、已移出登记集的死 gate
	deadGateMembers int64    // 本拍预算用完后,仍未释放的死 gate 的索引成员数之和
	liveMembers     int64    // 存活 gate 的索引成员数之和(在线数的上界)
	actions         map[string]int
	keyErrors       keyErrorTally // 本拍吸收的单键错误(上限 maxKeyTypeErrorsPerRound)
}

// isKeyTypeError 判断错误能否归因于某一个键本身(类型不对),而不是 Redis 实例或连接的故障。
//
// 只认 WRONGTYPE:Redis 7 的脚本错误保留错误码前缀("WRONGTYPE ... script: ..."),Redis 6 与 miniredis 包成
// "ERR Error ... script ...: WRONGTYPE ...",两者都含这个词。READONLY / LOADING / MASTERDOWN / CLUSTERDOWN / TRYAGAIN
// (go-redis 重试用尽后同样以 redis.Error 抛出)、OOM、BUSY 也是服务端回复,但它们是实例级故障:当成单键问题跳过,
// 主从切换或 OOM 期间每个转换都会被跳过而本拍报 ok,last_success 照常推进、不让位,真故障就被掩盖了。
func isKeyTypeError(err error) bool {
	var rerr redis.Error
	return errors.As(err, &rerr) && strings.Contains(rerr.Error(), "WRONGTYPE")
}

// keyErrorTally 统计一拍 / 一遍里被吸收的单键错误。limit > 0 时超过即按实例级故障返回错误(索引循环用);
// 完整扫描传 0 不设上限:一个坏掉的索引键会让那个 gate 上的每个候选都报错,设上限会让整遍提前中止、
// 其余键永远扫不到;那一遍改由「有候选被跳过就不设就绪标记」守住(passReport.skippedCandidates)。
type keyErrorTally struct {
	limit  int
	count  int
	logged int
}

// absorb 判断 err 能否按「这一个键的问题」吸收:能则计数、记 action、按上限返回 nil 或错误;不能原样返回 err。
// what 是日志里的定位信息(gate_instance / player),不进指标。
func (t *keyErrorTally) absorb(err error, actions map[string]int, action string, what string) error {
	if !isKeyTypeError(err) {
		return err
	}
	t.count++
	countReconcileAction(actions, action)
	if t.limit > 0 && t.count > t.limit {
		return fmt.Errorf("%d key type errors exceed the per-round limit %d, treating as an instance-level failure: %w",
			t.count, t.limit, err)
	}
	if t.logged < keyErrorLogLimit {
		t.logged++
		logx.Errorf("SessionReconciler: skipped %s: key has the wrong type, left in place for investigation: %v", what, err)
	}
	return nil
}

// runIndexRound 跑一拍,recover 收在这一拍:对账本身就是兜底链路,一条畸形数据 panic 不能让它永久停摆。
func (r *sessionReconciler) runIndexRound(ctx context.Context) roundOutcome {
	out := roundOutcome{stage: stagePanic, err: errors.New("index round panicked")}
	safego.Run("player_locator.session_reconciler.round", func() {
		roundCtx, cancel := context.WithTimeout(ctx, r.interval)
		defer cancel()
		out = r.indexRound(roundCtx)
	})
	return out
}

// indexRound 是一拍,顺序固定:写者检查 → 登记集快照 → etcd 存活集 → 推进去抖 → 预算内处理死 gate → 刷 gauge。
func (r *sessionReconciler) indexRound(ctx context.Context) roundOutcome {
	rep := indexRoundReport{actions: map[string]int{}, keyErrors: keyErrorTally{limit: maxKeyTypeErrorsPerRound}}

	// a) 写者检查。etcd 出错只跳过检查本身:不删就绪标记、不动代次 —— fail-closed 针对的是「转不转换会话」,
	//    不是「出错就认定有旧写者」;后者会让每次 etcd 抖动都作废补建,稳态退化成永久的全键扫描。
	//    本拍照常处理死 gate,但整拍记为失败,计入让位门槛。
	writerStage, writerErr := r.checkSessionWriters(ctx, &rep)

	// b) 登记集快照必须先于 c 的 etcd 读:gate 先在 etcd 注册,login 才会把玩家分给它,
	//    所以快照里的每个 gate 都早于下面那次 Range 注册过,活着就一定在存活集里。
	registry, err := r.rdb.SMembers(ctx, GateIndexRegistryKey).Result()
	if err != nil {
		return roundOutcome{stage: stageRedis, err: fmt.Errorf("read gate index registry: %w", err), report: rep}
	}

	// c) 看不清存活集就不推进去抖、不转换(fail-closed)。
	live, err := r.deps.view.LiveGateInstances(ctx)
	if err != nil {
		return roundOutcome{stage: stageEtcd, err: fmt.Errorf("list live gates: %w", err), report: rep}
	}

	// d)
	dead, alive := r.advanceSuspects(registry, live)
	rep.deadGates = dead

	// e)
	if err := r.processDeadGates(ctx, dead, &rep); err != nil {
		return roundOutcome{stage: stageRedis, err: err, report: rep}
	}

	// f)
	if err := r.refreshIndexGauges(ctx, dead, alive, &rep); err != nil {
		return roundOutcome{stage: stageRedis, err: err, report: rep}
	}
	if len(dead) > 0 || len(rep.actions) > 0 {
		logx.Infof("SessionReconciler: round dead_gates=%d released=%d dead_gate_members_left=%d live_members=%d legacy_writers=%d key_type_errors=%d actions=%v",
			len(dead), len(rep.released), rep.deadGateMembers, rep.liveMembers, rep.legacyWriters, rep.keyErrors.count, rep.actions)
	}
	if writerErr != nil {
		return roundOutcome{stage: writerStage, err: writerErr, report: rep}
	}
	return roundOutcome{report: rep}
}

// checkSessionWriters 比对已注册的 locator 与有能力键的 locator:有旧写者 → 递增代次、删就绪标记;
// 零旧写者 → 给已存在的就绪标记续租。只有一次成功的 etcd 读才会改动标记与代次。
func (r *sessionReconciler) checkSessionWriters(ctx context.Context, rep *indexRoundReport) (roundStage, error) {
	all, capable, err := r.deps.view.SessionWriters(ctx)
	if err != nil {
		return stageEtcd, fmt.Errorf("list session writers: %w", err)
	}
	legacy := legacyWriters(all, capable)
	rep.legacyWriters = len(legacy)
	reconcileLegacyWriters.Set(float64(len(legacy)))
	if len(legacy) > 0 {
		if err := markLegacyWritersScript.Run(ctx, r.rdb, []string{gateIndexGenKey, gateIndexReadyKey}).Err(); err != nil {
			return stageRedis, fmt.Errorf("mark legacy writers: %w", err)
		}
		gateIndexReady.Set(0)
		// 滚动升级期间每拍都会出现,是预期的;长期不消失才告警(PlayerLocatorLegacySessionWriters)。
		logx.Infof("SessionReconciler: %d legacy session writer(s) without gate index capability (first: %s); "+
			"ready marker cleared, gen bumped, backfill keeps running", len(legacy), strings.Join(legacy[:min(3, len(legacy))], ","))
		return "", nil
	}
	renewed, err := renewGateIndexReadyScript.Run(ctx, r.rdb, []string{gateIndexReadyKey},
		r.readyLease().Milliseconds(), gateIndexReadyValuePrefix).Int()
	if err != nil {
		return stageRedis, fmt.Errorf("renew gate index ready marker: %w", err)
	}
	rep.readyRenewed = renewed == 1
	if rep.readyRenewed {
		gateIndexReady.Set(1)
	} else {
		gateIndexReady.Set(0)
	}
	return "", nil
}

// advanceSuspects 推进 gate 级去抖,返回已确认死亡(连续两拍缺席)与存活的 gate(都排过序)。
// 已确认死亡的 gate 只要还在登记集里就保持确认,预算截断的处理下一拍直接续上,不必重新去抖;
// gate 在 etcd 重新出现(重注册沿用同一 uuid)即清掉嫌疑与游标。登记集之外的条目一并丢弃。
func (r *sessionReconciler) advanceSuspects(registry []string, live map[string]struct{}) (dead, alive []string) {
	inRegistry := make(map[string]struct{}, len(registry))
	for _, gate := range registry {
		if gate == "" {
			continue
		}
		inRegistry[gate] = struct{}{}
		if _, ok := live[gate]; ok {
			delete(r.suspects, gate)
			delete(r.cursors, gate)
			alive = append(alive, gate)
			continue
		}
		if r.suspects[gate] < 2 {
			r.suspects[gate]++
		}
		if r.suspects[gate] >= 2 {
			dead = append(dead, gate)
		}
	}
	for gate := range r.suspects {
		if _, ok := inRegistry[gate]; !ok {
			delete(r.suspects, gate)
			delete(r.cursors, gate)
		}
	}
	sort.Strings(dead)
	sort.Strings(alive)
	return dead, alive
}

// processDeadGates 在本拍预算内处理已确认死亡的 gate:逐页 SSCAN 索引、一次 MGET 本页会话、逐个判定后转换或摘除。
// 预算(成员数,另有半拍的时间上限)用完就记下游标留到下一拍,保证这一拍按时完成、新死的 gate 照常被识别。
// 一个 gate 扫完一遍后索引空了就释放;没空(CAS 冲突留下的成员、扫描期间新加的成员、毒成员)就下一拍从头再扫。
// 某个 gate 的索引键类型不对(SSCAN / 释放报 WRONGTYPE)只跳过这个 gate,不中止整拍(rep.keyErrors 计数,有上限)。
func (r *sessionReconciler) processDeadGates(ctx context.Context, dead []string, rep *indexRoundReport) error {
	budget := r.memberBudget
	start := r.deps.now()
	for _, gate := range dead {
		if budget <= 0 || r.deps.now().Sub(start) >= r.interval/2 {
			return nil
		}
		for budget > 0 && r.deps.now().Sub(start) < r.interval/2 {
			members, next, err := r.rdb.SScan(ctx, gateSessionIndexKey(gate), r.cursors[gate], "", r.cfg.ScanCount).Result()
			if err != nil {
				if aerr := rep.keyErrors.absorb(err, rep.actions, reconcileActionGateError,
					"dead gate_instance="+gate+" (SSCAN of its index)"); aerr != nil {
					return fmt.Errorf("sscan dead gate %s index: %w", gate, aerr)
				}
				delete(r.cursors, gate)
				break // 只跳过这个 gate,本拍继续下一个;它保持确认死亡,下一拍重试
			}
			if err := r.sweepIndexPage(ctx, gate, members, true, rep.actions, &rep.keyErrors); err != nil {
				return err
			}
			budget -= len(members)
			if next != 0 {
				r.cursors[gate] = next
				continue
			}
			delete(r.cursors, gate)
			released, err := releaseGateIndexIfEmptyScript.Run(ctx, r.rdb,
				[]string{gateSessionIndexKey(gate), GateIndexRegistryKey}, gate).Int()
			if err != nil {
				if aerr := rep.keyErrors.absorb(err, rep.actions, reconcileActionGateError,
					"dead gate_instance="+gate+" (release of its index)"); aerr != nil {
					return fmt.Errorf("release dead gate %s index: %w", gate, aerr)
				}
				break
			}
			if released == 1 {
				rep.released = append(rep.released, gate)
				delete(r.suspects, gate)
				logx.Infof("SessionReconciler: dead gate index drained and released gate_instance=%s", gate)
			}
			break
		}
	}
	return nil
}

// sweepIndexPage 判定一页索引成员并处置:
//   - 坏成员直接摘;会话缺失 / 解不开 / 已不在这个 gate 上 ONLINE 的,按判定时看到的字节守卫摘;
//   - 仍在这个 gate 上 ONLINE 的:transition=true(死 gate)经限速转成 DISCONNECTING;false(审计)原样保留。
//
// transition=true 只由索引循环调用(transitionPacer 归它);审计只传 false,不碰 transitionPacer。
//
// 单个成员上的 WRONGTYPE(会话键被写成了别的类型)由 tally 吸收:跳过该成员、计 member_error,成员留在索引里。
// 其余错误(连接、超时、READONLY 等实例级故障)原样返回,整拍 / 这一页失败。
func (r *sessionReconciler) sweepIndexPage(ctx context.Context, gate string, members []string, transition bool,
	actions map[string]int, tally *keyErrorTally) error {
	indexKey := gateSessionIndexKey(gate)
	valid := make([]string, 0, len(members))
	pids := make([]uint64, 0, len(members))
	for _, member := range members {
		pid, ok := parseIndexMember(member)
		if !ok {
			if err := r.rdb.SRem(ctx, indexKey, member).Err(); err != nil {
				if aerr := tally.absorb(err, actions, reconcileActionMemberError,
					fmt.Sprintf("bad member %q of gate_instance=%s index", member, gate)); aerr != nil {
					return fmt.Errorf("prune bad member of gate %s index: %w", gate, aerr)
				}
				continue
			}
			countReconcileAction(actions, reconcileActionPrunedBadMember)
			logx.Errorf("SessionReconciler: pruned non-player-id member %q from gate_instance=%s index", member, gate)
			continue
		}
		valid = append(valid, member)
		pids = append(pids, pid)
	}
	raws, found, err := mgetSessions(ctx, r.rdb, pids)
	if err != nil {
		return fmt.Errorf("mget sessions of gate %s index: %w", gate, err)
	}
	attempted, failed := 0, 0 // 本页的转换尝试数与其中报 WRONGTYPE 的数
	for i, member := range valid {
		pid, session, act := classifyIndexedMember(member, raws[i], found[i], gate)
		if act == indexedOnlineHere {
			if !transition {
				continue
			}
			if err := r.transitionPacer.Wait(ctx); err != nil {
				return err
			}
			attempted++
			swapped, err := r.transition(ctx, pid, raws[i], session)
			if err != nil {
				if aerr := tally.absorb(err, actions, reconcileActionMemberError,
					fmt.Sprintf("transition of player=%d on dead gate_instance=%s", pid, gate)); aerr != nil {
					return fmt.Errorf("transition player %d to disconnecting: %w", pid, aerr)
				}
				failed++
				continue
			}
			if swapped {
				countReconcileAction(actions, reconcileActionToDisconnecting)
			} else {
				// 会话在判定之后被改写(重连 / 重登):成员留着,下一次判定重来。
				countReconcileAction(actions, reconcileActionCASConflict)
			}
			continue
		}
		action := reconcileActionPrunedStale
		switch act {
		case indexedMissing:
			action = reconcileActionPrunedMissing
		case indexedMalformed:
			action = reconcileActionPrunedMalformed
			logx.Errorf("SessionReconciler: malformed session behind index member gate_instance=%s player=%d; "+
				"member pruned, session left untouched", gate, pid)
		}
		removed, err := pruneGateIndexMember(ctx, r.rdb, gate, member, pid, raws[i], found[i])
		if err != nil {
			// 典型来源:会话键被写成了 hash / set 等(MGET 对非 string 回 nil,判成 missing,守卫脚本里的 GET 报 WRONGTYPE)。
			if aerr := tally.absorb(err, actions, reconcileActionMemberError,
				fmt.Sprintf("member %s of gate_instance=%s index (session key %s)", member, gate, sessionKey(pid))); aerr != nil {
				return fmt.Errorf("prune member %s of gate %s index: %w", member, gate, aerr)
			}
			continue
		}
		if removed {
			countReconcileAction(actions, action)
		}
	}
	// 转换脚本只碰本页刚 MGET 到、确认是 string 的会话键,这个 gate 的索引键,以及全局的租约簿记(player:leases*)。
	// 同一页里**所有**转换都报 WRONGTYPE,坏的多半是全局键而不是哪个成员:按实例级故障整拍失败、计入让位门槛,
	// 不能逐个跳过后报 ok。摘除路径不做这条判定:摘不掉的毒成员恰恰是死 gate 索引里最后剩下的那些,
	// 「一页全是毒成员」是预期状态,判成整拍失败会让几个坏键拖停全服对账。
	if attempted > 0 && failed == attempted {
		return fmt.Errorf("all %d transitions on a page of dead gate %s index failed with WRONGTYPE: "+
			"a shared lease key likely has the wrong type", attempted, gate)
	}
	return nil
}

// transition 把死 gate 上的 ONLINE 会话转成 DISCONNECTING:与 SetDisconnecting 同一个提交点。
// 会话此刻已被改写(重连 / 重登)则 CAS 失败 no-op,绝不误杀活人。
func (r *sessionReconciler) transition(ctx context.Context, pid uint64, expected []byte, session *pb.PlayerSession) (bool, error) {
	updatedSession := proto.Clone(session).(*pb.PlayerSession)
	updatedSession.State = pb.PlayerSessionState_SESSION_STATE_DISCONNECTING
	updatedSession.SessionVersion++
	updated, err := proto.Marshal(updatedSession)
	if err != nil {
		// 刚解出来的消息重新编码不会失败;真失败也只跳过这一个,不拖垮整拍。
		logx.Errorf("SessionReconciler: marshal updated session player=%d failed: %v", pid, err)
		return false, nil
	}
	ttl := time.Duration(r.svcCtx.Config.Lease.DefaultTTLSeconds) * time.Second
	if ttl == 0 {
		ttl = 30 * time.Second
	}
	swapped, err := transitionToDisconnecting(ctx, r.svcCtx, pid, expected, updated, r.deps.now().Add(ttl).Unix(), session)
	if err != nil || !swapped {
		return false, err
	}
	logx.Infof("SessionReconciler: orphaned ONLINE session -> DISCONNECTING player=%d session=%d dead_gate_instance=%s",
		pid, session.SessionId, session.GateInstanceId)
	return true, nil
}

// refreshIndexGauges 刷新只由 leader 产出的 gauge:死 gate 数与剩余成员(进度)、存活 gate 的成员数(在线数上界)。
func (r *sessionReconciler) refreshIndexGauges(ctx context.Context, dead, alive []string, rep *indexRoundReport) error {
	released := make(map[string]struct{}, len(rep.released))
	for _, gate := range rep.released {
		released[gate] = struct{}{}
	}
	pending := make([]string, 0, len(dead))
	for _, gate := range dead {
		if _, ok := released[gate]; !ok {
			pending = append(pending, gate)
		}
	}
	deadMembers, err := r.sumIndexMembers(ctx, pending, rep)
	if err != nil {
		return fmt.Errorf("count dead gate index members: %w", err)
	}
	liveMembers, err := r.sumIndexMembers(ctx, alive, rep)
	if err != nil {
		return fmt.Errorf("count live gate index members: %w", err)
	}
	rep.deadGateMembers, rep.liveMembers = deadMembers, liveMembers
	reconcileDeadGates.Set(float64(len(pending)))
	reconcileDeadGateMembers.Set(float64(deadMembers))
	gateIndexMembers.Set(float64(liveMembers))
	return nil
}

// sumIndexMembersBatch 是一次 pipeline 里最多的 SCARD 数(N_g 目标规模 1000~4444)。
const sumIndexMembersBatch = 1000

// sumIndexMembers 对一批 gate 的索引 SCARD 求和。某个索引键类型不对只跳过那个 gate(计 gate_error,有上限),
// 不让存活 gate 上的一个坏键把每一拍都打成失败;其余错误(连接、超时)整拍失败。
func (r *sessionReconciler) sumIndexMembers(ctx context.Context, gates []string, rep *indexRoundReport) (int64, error) {
	var total int64
	for start := 0; start < len(gates); start += sumIndexMembersBatch {
		end := min(start+sumIndexMembersBatch, len(gates))
		pipe := r.rdb.Pipeline()
		cmds := make([]*redis.IntCmd, 0, end-start)
		for _, gate := range gates[start:end] {
			cmds = append(cmds, pipe.SCard(ctx, gateSessionIndexKey(gate)))
		}
		// Exec 只返回第一条失败命令的错误;逐条看,才分得清「某个键类型不对」与「整条 pipeline 失败」。
		if _, err := pipe.Exec(ctx); err != nil && !isKeyTypeError(err) {
			return 0, err
		}
		for i, cmd := range cmds {
			if err := cmd.Err(); err != nil {
				if aerr := rep.keyErrors.absorb(err, rep.actions, reconcileActionGateError,
					"gate_instance="+gates[start+i]+" (SCARD of its index)"); aerr != nil {
					return 0, aerr
				}
				continue
			}
			total += cmd.Val()
		}
	}
	return total, nil
}

// countReconcileAction 同时记本拍 / 本遍统计与计数器。
func countReconcileAction(actions map[string]int, action string) {
	actions[action]++
	reconcileSessionsTotal.Inc(action)
}

// ---------------------------------------------------------------------------
// 补建 / 审计循环
// ---------------------------------------------------------------------------

// repairLoop 是任期内的补建 / 审计循环:就绪标记缺失时每隔一拍跑一遍补建;标记存在且审计到期时跑审计。
// 每一步各自 recover(safego.Run),ctx 取消即退出。
func (r *sessionReconciler) repairLoop(ctx context.Context) {
	for ctx.Err() == nil {
		if !safego.Run("player_locator.session_reconciler.repair_step", func() { r.repairStep(ctx) }) {
			reconcileRoundTotal.Inc(reconcileLoopRepair, reconcileResultPanic)
		}
		if err := r.deps.sleep(ctx, r.interval); err != nil {
			return
		}
	}
}

// repairStepResult 是一步补建 / 审计的结果(单测看它;指标后端在单测里不生效)。
type repairStepResult struct {
	loop   string // reconcileLoopRepair / reconcileLoopAudit;"" = 本步什么都没跑
	result string // reconcileResult*;"" = 被取消,不计数
	report passReport
}

// repairStep 做一步:读就绪标记;缺失 → 补建一遍;存在且审计到期 → 审计一遍;否则什么都不做。
func (r *sessionReconciler) repairStep(ctx context.Context) repairStepResult {
	ready, err := r.rdb.Exists(ctx, gateIndexReadyKey).Result()
	if err != nil {
		if ctx.Err() == nil {
			logx.Errorf("SessionReconciler: read gate index ready marker failed, retry next interval: %v", err)
		}
		return repairStepResult{}
	}
	if ready == 0 {
		return r.runRepairPass(ctx)
	}
	if r.cfg.AuditIntervalSeconds < 0 {
		return repairStepResult{}
	}
	due, err := r.auditDue(ctx)
	if err != nil {
		if ctx.Err() == nil {
			logx.Errorf("SessionReconciler: read last audit time failed, retry next interval: %v", err)
		}
		return repairStepResult{}
	}
	if !due {
		return repairStepResult{}
	}
	return r.runAuditPass(ctx)
}

// runRepairPass 跑一遍补建,结束时「代次未变才设就绪标记」:补建期间任何一拍在成功的 etcd 读里看到旧写者,
// 都会递增代次,这一遍就作废(补进来的成员照样有效,只是不能据此宣布就绪),下一拍再来一遍。
func (r *sessionReconciler) runRepairPass(ctx context.Context) repairStepResult {
	out := repairStepResult{loop: reconcileLoopRepair}
	gen, err := readGateIndexGen(ctx, r.rdb)
	if err != nil {
		return r.finishPass(ctx, out, fmt.Errorf("read gate index gen: %w", err))
	}
	// 先读代次、再查一次写者:此刻已有旧写者时照样补(补进来的成员有效),但这次检查本身就会递增代次,
	// 结束时不会设标记 —— 否则在比一拍还短的补建(小环境)里,标记会在旧写者仍在时被设上、下一拍才被删。
	// etcd 读失败就不开始这一遍,下一拍再来:不能凭一次看不清的读开始一遍可能宣布就绪的补建。
	var writers indexRoundReport
	if stage, werr := r.checkSessionWriters(ctx, &writers); werr != nil {
		if ctx.Err() == nil {
			out.result = reconcileResultRedisError
			if stage == stageEtcd {
				out.result = reconcileResultEtcdError
			}
			reconcileRoundTotal.Inc(out.loop, out.result)
			logx.Errorf("SessionReconciler: backfill pass not started, writer check failed stage=%s: %v", stage, werr)
		}
		return out
	}
	runID, err := r.deps.redisRunID(ctx, r.rdb)
	if err != nil {
		return r.finishPass(ctx, out, fmt.Errorf("read redis run_id before backfill: %w", err))
	}
	logx.Infof("SessionReconciler: gate index not ready, starting backfill pass gen=%s legacy_writers=%d redis_run_id=%s",
		gen, writers.legacyWriters, runID)
	out.report, err = r.fullPass(ctx, false, runID)
	if err != nil {
		return r.finishPass(ctx, out, err)
	}
	// 有 ONLINE 会话因键类型错误没能补进索引:I1 的「⇐」方向此刻不成立,不能宣布就绪(fail-closed)。
	// 键一直坏着就一直不就绪,由 PlayerLocatorGateIndexNotReady 与 member_error 计数暴露,要人工处理那个键。
	if out.report.skippedCandidates > 0 {
		out.result = reconcileResultIncomplete
		reconcileRoundTotal.Inc(out.loop, out.result)
		logx.Errorf("SessionReconciler: backfill pass done but %d ONLINE session(s) could not be indexed (key type errors); "+
			"ready marker NOT set: %s", out.report.skippedCandidates, out.report)
		return out
	}
	now := r.deps.now()
	set, err := setGateIndexReadyIfGenScript.Run(ctx, r.rdb,
		[]string{gateIndexGenKey, gateIndexReadyKey, gateIndexLastAuditKey},
		gen, gateIndexReadyValue(now), r.readyLease().Milliseconds(), now.Unix(),
	).Int()
	if err != nil {
		return r.finishPass(ctx, out, fmt.Errorf("set gate index ready marker: %w", err))
	}
	if set != 1 {
		out.result = reconcileResultGenChanged
		reconcileRoundTotal.Inc(out.loop, out.result)
		logx.Infof("SessionReconciler: backfill pass done but gen changed since %s (legacy writers seen); ready marker NOT set: %s",
			gen, out.report)
		return out
	}
	gateIndexReady.Set(1)
	logx.Infof("SessionReconciler: backfill pass done, gate index ready: %s", out.report)
	return r.finishPass(ctx, out, nil)
}

// runAuditPass 跑一遍审计(索引就绪时):补漏成员、清脏成员,完成后记下审计时刻。
// 补进成员说明不变量 I1 被破坏过(非 player_locator 的写者、外部篡改、或旧写者只活在 etcd 故障期间),打 ERROR。
// 失败(含扫描期间 Redis 换了进程)不记审计时刻,并设冷却键(failAudit)。
func (r *sessionReconciler) runAuditPass(ctx context.Context) repairStepResult {
	out := repairStepResult{loop: reconcileLoopAudit}
	runID, err := r.deps.redisRunID(ctx, r.rdb)
	if err != nil {
		return r.failAudit(ctx, out, fmt.Errorf("read redis run_id before audit: %w", err))
	}
	out.report, err = r.fullPass(ctx, true, runID)
	if err != nil {
		return r.failAudit(ctx, out, err)
	}
	if err := r.rdb.Set(ctx, gateIndexLastAuditKey, r.deps.now().Unix(), 0).Err(); err != nil {
		return r.failAudit(ctx, out, fmt.Errorf("record last audit time: %w", err))
	}
	switch {
	case out.report.repaired > 0 || out.report.skippedCandidates > 0:
		logx.Errorf("SessionReconciler: audit found ONLINE session(s) missing from the index while it was ready "+
			"(backfilled %d, could not index %d because of key type errors): invariant I1 was broken "+
			"(non-locator writer, tampering, or a legacy writer alive only during an etcd outage), investigate: %s",
			out.report.repaired, out.report.skippedCandidates, out.report)
	default:
		logx.Infof("SessionReconciler: audit pass done: %s", out.report)
	}
	return r.finishPass(ctx, out, nil)
}

// failAudit 记一次审计失败并设冷却键:持续的故障(Redis 一直抖、扫描总被打断)下不让审计每拍重开一遍全键扫描。
// 冷却用 Redis 侧 EX,换主不重置,也不受各 Pod 时钟偏差影响。被取消(丢锁 / 关停)的不设。
func (r *sessionReconciler) failAudit(ctx context.Context, out repairStepResult, err error) repairStepResult {
	out = r.finishPass(ctx, out, err)
	if ctx.Err() != nil {
		return out
	}
	cooldown := r.auditCooldown()
	if serr := r.rdb.Set(ctx, gateIndexAuditCooldownKey, "1", cooldown).Err(); serr != nil {
		// 设不上冷却只会让下一拍就重试,不影响正确性。
		logx.Errorf("SessionReconciler: set audit cooldown failed, next audit attempt not delayed: %v", serr)
		return out
	}
	logx.Errorf("SessionReconciler: audit pass failed, next attempt in %v", cooldown)
	return out
}

// auditCooldown 是审计失败后的冷却:auditFailureCooldown,审计周期更短时取审计周期。
func (r *sessionReconciler) auditCooldown() time.Duration {
	cooldown := auditFailureCooldown
	if interval := time.Duration(r.cfg.AuditIntervalSeconds) * time.Second; interval > 0 && interval < cooldown {
		cooldown = interval
	}
	return cooldown
}

// finishPass 记一遍补建 / 审计的结果;被取消(丢锁 / 关停)的不计数。
func (r *sessionReconciler) finishPass(ctx context.Context, out repairStepResult, err error) repairStepResult {
	if err != nil {
		if ctx.Err() != nil {
			return out
		}
		out.result = reconcileResultRedisError
		reconcileRoundTotal.Inc(out.loop, out.result)
		logx.Errorf("SessionReconciler: %s pass failed, retry next interval: %v (%s)", out.loop, err, out.report)
		return out
	}
	out.result = reconcileResultOK
	reconcileRoundTotal.Inc(out.loop, out.result)
	reconcileLastSuccessUnix.Set(float64(r.deps.now().Unix()), out.loop)
	return out
}

// auditDue 以 Redis 里的上次审计时刻为准判断审计是否到期:换主不重置计时。冷却键存在(上次审计失败)时不到期。
// 键不存在或值坏了按「从未审计」处理(到期):多审一次只多一轮限速扫描,不审却会让漏成员一直补不上。
//
// 时钟偏差语义:last_audit 是写入它的那个 leader 的墙钟,与本 leader 的墙钟比较。各 Pod 时钟偏差 δ 小于审计周期时,
// 审计最多提前或推迟 δ,可以接受。比本 leader 时钟超前一个审计周期以上的值不可能来自正常偏差(写入者的时钟坏了,
// 例如 NTP 故障快了几天),当坏值处理(到期并打 ERROR);否则此后所有 leader 都算出负数,审计停摆到各自时钟追上为止。
func (r *sessionReconciler) auditDue(ctx context.Context) (bool, error) {
	cooling, err := r.rdb.Exists(ctx, gateIndexAuditCooldownKey).Result()
	if err != nil {
		return false, err
	}
	if cooling > 0 {
		return false, nil
	}
	raw, err := r.rdb.Get(ctx, gateIndexLastAuditKey).Result()
	if err == redis.Nil {
		return true, nil
	}
	if err != nil {
		return false, err
	}
	last, perr := strconv.ParseInt(raw, 10, 64)
	if perr != nil {
		logx.Errorf("SessionReconciler: malformed %s value %q, treating audit as due", gateIndexLastAuditKey, raw)
		return true, nil
	}
	now := r.deps.now().Unix()
	if last > now+r.cfg.AuditIntervalSeconds {
		logx.Errorf("SessionReconciler: %s=%d is more than one audit interval (%ds) ahead of this leader's clock (%d): "+
			"the writer's clock was wrong, treating audit as due", gateIndexLastAuditKey, last, r.cfg.AuditIntervalSeconds, now)
		return true, nil
	}
	return now-last >= r.cfg.AuditIntervalSeconds, nil
}

// passReport 是一遍补建 / 审计的统计。
type passReport struct {
	scannedKeys int
	repaired    int
	gates       int
	// skippedCandidates:ONLINE 且 gate 非空、却因键类型错误(索引键不是 SET 等)没能核对或补进索引的会话数。
	// 补建遍里它非零就不设就绪标记。
	skippedCandidates int
	// pageRetries:本遍单页失败后重试的次数(同一游标)。
	pageRetries int
	actions     map[string]int
	keyErrors   keyErrorTally // 完整扫描不设上限(见 keyErrorTally)
}

func (p passReport) String() string {
	return fmt.Sprintf("scanned_keys=%d repaired=%d gates=%d skipped_candidates=%d key_type_errors=%d page_retries=%d actions=%v",
		p.scannedKeys, p.repaired, p.gates, p.skippedCandidates, p.keyErrors.count, p.pageRetries, p.actions)
}

// fullPass 是补建与审计共用的一遍扫描:先限速 SCAN 全部会话键补漏成员,再逐个 gate SSCAN 索引清脏成员。
// 只补索引、清成员,**不转换会话**:补进来的死 gate 成员由索引循环在两拍内处理,转换只有一条路径。
//
// runID 是开始前读到的 Redis 进程 run_id;扫完再读一次,变了(主从切换 / 重启)就整遍作废:游标跨了进程,
// 不再有「开始时就在的键都扫到」的保证(readRedisRunID)。补进来的成员照样有效,只是不能据此宣布就绪 / 记审计完成。
func (r *sessionReconciler) fullPass(ctx context.Context, audit bool, runID string) (passReport, error) {
	rep := passReport{actions: map[string]int{}}
	if err := r.backfillKeyspace(ctx, audit, &rep); err != nil {
		return rep, err
	}
	if err := r.pruneAllGateIndexes(ctx, &rep); err != nil {
		return rep, err
	}
	endRunID, err := r.deps.redisRunID(ctx, r.rdb)
	if err != nil {
		return rep, fmt.Errorf("read redis run_id after pass: %w", err)
	}
	if endRunID != runID {
		return rep, fmt.Errorf("redis run_id changed during the pass (%s -> %s): failover or restart, "+
			"scan coverage is not guaranteed, discarding this pass", runID, endRunID)
	}
	return rep, nil
}

// retryPassPage 对完整扫描的一页做有上限的指数退避重试,游标由调用方固定(同一游标重试)。
//   - SCAN / SSCAN 游标无状态,在同一个 Redis 进程上重试同一游标是安全的;换了进程由 fullPass 末尾的 run_id 核对兜住。
//   - 页内的写都是幂等的字节守卫脚本 / SADD,重做一页不会重复生效;计数器可能把同一个单键错误多计一次,不影响判定。
//   - ctx 结束立即返回;连续 passPageMaxAttempts 次失败才放弃这一遍。
func (r *sessionReconciler) retryPassPage(ctx context.Context, rep *passReport, what func() string, page func() error) error {
	backoff := passPageRetryBase
	for attempt := 1; ; attempt++ {
		err := page()
		if err == nil {
			return nil
		}
		if ctx.Err() != nil {
			return ctx.Err()
		}
		if attempt >= passPageMaxAttempts {
			return fmt.Errorf("%s: giving up the pass after %d attempts: %w", what(), attempt, err)
		}
		rep.pageRetries++
		logx.Errorf("SessionReconciler: %s failed (attempt %d/%d), retrying the same cursor in %v: %v",
			what(), attempt, passPageMaxAttempts, backoff, err)
		if serr := r.deps.sleep(ctx, backoff); serr != nil {
			return serr
		}
		backoff = min(backoff*2, passPageRetryMax)
	}
}

// backfillKeyspace 限速 SCAN player:session:*,把 ONLINE 且 gate 非空、却不在对应索引里的会话补进去。
func (r *sessionReconciler) backfillKeyspace(ctx context.Context, audit bool, rep *passReport) error {
	var cursor uint64
	for {
		var next uint64
		err := r.retryPassPage(ctx, rep, func() string { return fmt.Sprintf("scan sessions at cursor %d", cursor) }, func() error {
			if err := r.scanPacer.Wait(ctx); err != nil {
				return err
			}
			keys, n, err := r.rdb.Scan(ctx, cursor, sessionKeyPrefix+"*", r.cfg.ScanCount).Result()
			if err != nil {
				return err
			}
			if err := r.backfillPage(ctx, keys, audit, rep); err != nil {
				return err
			}
			rep.scannedKeys += len(keys) // 整页成功才计,重试不重复计
			next = n
			return nil
		})
		if err != nil {
			return err
		}
		if next == 0 {
			return nil
		}
		cursor = next
	}
}

type backfillCandidate struct {
	pid    uint64
	member string
	gate   string
	raw    []byte
}

// backfillPage 处理一页会话键:一次 MGET,对候选会话一次 SISMEMBER pipeline,缺失的逐个经守卫脚本补。
func (r *sessionReconciler) backfillPage(ctx context.Context, keys []string, audit bool, rep *passReport) error {
	pids := make([]uint64, 0, len(keys))
	for _, key := range keys {
		pid, ok := parseIndexMember(strings.TrimPrefix(key, sessionKeyPrefix))
		if !ok {
			continue // 不是写路径会写的键形(player:session:<十进制 id>),与索引无关
		}
		pids = append(pids, pid)
	}
	raws, found, err := mgetSessions(ctx, r.rdb, pids)
	if err != nil {
		return fmt.Errorf("mget session page: %w", err)
	}
	candidates := make([]backfillCandidate, 0, len(pids))
	for i, pid := range pids {
		if !found[i] {
			continue // 扫描与删除竞态,正常
		}
		session := &pb.PlayerSession{}
		if err := proto.Unmarshal(raws[i], session); err != nil || session.PlayerId != pid {
			// 写路径不会写出这种会话;不补索引(补了也只会被判 malformed 摘掉),留给排查。
			logx.Errorf("SessionReconciler: malformed session at %s skipped by backfill", sessionKey(pid))
			continue
		}
		if gate := indexedGate(session); gate != "" {
			candidates = append(candidates, backfillCandidate{
				pid: pid, member: strconv.FormatUint(pid, 10), gate: gate, raw: raws[i],
			})
		}
	}
	if len(candidates) == 0 {
		return nil
	}

	pipe := r.rdb.Pipeline()
	checks := make([]*redis.BoolCmd, len(candidates))
	for i, c := range candidates {
		checks[i] = pipe.SIsMember(ctx, gateSessionIndexKey(c.gate), c.member)
	}
	// Exec 只返回第一条失败命令的错误;逐条看,某个 gate 的索引键类型不对只跳过那个 gate 上的候选。
	if _, err := pipe.Exec(ctx); err != nil && !isKeyTypeError(err) {
		return fmt.Errorf("check index membership: %w", err)
	}
	skip := make([]bool, len(candidates))
	for i, c := range candidates {
		err := checks[i].Err()
		if err == nil {
			continue
		}
		if aerr := rep.keyErrors.absorb(err, rep.actions, reconcileActionMemberError,
			fmt.Sprintf("backfill of player=%d: index of gate_instance=%s", c.pid, c.gate)); aerr != nil {
			return fmt.Errorf("check index membership: %w", aerr)
		}
		skip[i] = true
		rep.skippedCandidates++
	}

	// 已是成员的,确认它的 gate 在登记集里(I2;登记集被局部清库删掉时,不补的话这个 gate 死了也没人处理)。
	registered := make(map[string]struct{})
	for i, c := range candidates {
		if skip[i] || !checks[i].Val() {
			continue
		}
		registered[c.gate] = struct{}{}
	}
	if len(registered) > 0 {
		gates := make([]any, 0, len(registered))
		for g := range registered {
			gates = append(gates, g)
		}
		if err := r.rdb.SAdd(ctx, GateIndexRegistryKey, gates...).Err(); err != nil {
			return fmt.Errorf("ensure gate index registry: %w", err)
		}
	}

	attempted, failed := 0, 0 // 本页的补成员尝试数与其中报 WRONGTYPE 的数
	for i, c := range candidates {
		if skip[i] || checks[i].Val() {
			continue
		}
		attempted++
		added, err := repairGateIndexMemberScript.Run(ctx, r.rdb,
			[]string{sessionKey(c.pid), gateSessionIndexKey(c.gate), GateIndexRegistryKey},
			c.raw, c.member, c.gate,
		).Int()
		if err != nil {
			if aerr := rep.keyErrors.absorb(err, rep.actions, reconcileActionMemberError,
				fmt.Sprintf("backfill of player=%d into gate_instance=%s index", c.pid, c.gate)); aerr != nil {
				return fmt.Errorf("repair index member player %d: %w", c.pid, aerr)
			}
			failed++
			rep.skippedCandidates++
			continue
		}
		if added != 1 {
			continue // -1:会话已改写,新写入自己维护过索引;0:并发补上了
		}
		rep.repaired++
		if audit {
			countReconcileAction(rep.actions, reconcileActionAuditRepaired)
		} else {
			countReconcileAction(rep.actions, reconcileActionRepaired)
		}
	}
	// 补成员脚本只碰刚 MGET 到的 string 会话键、刚 SISMEMBER 成功的索引键,以及全局登记集。同一页里**所有**补成员
	// 都报 WRONGTYPE,坏的多半是登记集(全局键):按这一页失败处理(重试后放弃这一遍),不逐个跳过。
	if attempted > 0 && failed == attempted {
		return fmt.Errorf("all %d index repairs on a session page failed with WRONGTYPE: "+
			"the shared gate index registry likely has the wrong type", attempted)
	}
	return nil
}

// pruneAllGateIndexes 逐个登记过的 gate SSCAN 索引,按字节守卫摘掉不符的成员(含存活 gate)。
// 不做这一步,升级窗口里旧写者换 gate、或局部清库留下的脏成员会在长期存活的 gate 上一直堆着,
// gate_index_members 也会系统性偏高。真在这个 gate 上 ONLINE 的成员原样保留,死 gate 的转换仍归索引循环。
// 某个 gate 的索引键类型不对只跳过这个 gate(计 gate_error);单页失败按 retryPassPage 对同一游标重试。
func (r *sessionReconciler) pruneAllGateIndexes(ctx context.Context, rep *passReport) error {
	var gates []string
	err := r.retryPassPage(ctx, rep, func() string { return "read gate index registry" }, func() error {
		var err error
		gates, err = r.rdb.SMembers(ctx, GateIndexRegistryKey).Result()
		return err
	})
	if err != nil {
		return err
	}
	sort.Strings(gates)
	rep.gates = len(gates)
	for _, gate := range gates {
		if gate == "" {
			continue
		}
		var cursor uint64
		for {
			var next uint64
			skipGate := false
			err := r.retryPassPage(ctx, rep, func() string { return fmt.Sprintf("sscan gate %s index at cursor %d", gate, cursor) }, func() error {
				if err := r.scanPacer.Wait(ctx); err != nil {
					return err
				}
				members, n, err := r.rdb.SScan(ctx, gateSessionIndexKey(gate), cursor, "", r.cfg.ScanCount).Result()
				if err != nil {
					if aerr := rep.keyErrors.absorb(err, rep.actions, reconcileActionGateError,
						"gate_instance="+gate+" (SSCAN of its index during a full pass)"); aerr != nil {
						return aerr
					}
					skipGate = true
					return nil
				}
				if err := r.sweepIndexPage(ctx, gate, members, false, rep.actions, &rep.keyErrors); err != nil {
					return err
				}
				next = n
				return nil
			})
			if err != nil {
				return err
			}
			if skipGate || next == 0 {
				break
			}
			cursor = next
		}
	}
	return nil
}
