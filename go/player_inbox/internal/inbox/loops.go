package inbox

import (
	"context"
	"errors"
	"sync"
	"time"

	"player_inbox/internal/constants"
	"player_inbox/internal/metrics"

	"shared/safego"

	"github.com/zeromicro/go-zero/core/logx"
)

// 后台循环(设计 §6.3、§9.1、§9.3、§9.4、§10):清理、提醒补发、扫描型 gauge、对账。
//
// **只在领导者上跑**:领导者由 go/shared/leader 选举(核心 Redis 上的 player_inbox:maintenance:leader)。
//   - 清理只删终态行、逐行 autocommit、不持流行锁;领导者切换期间重叠执行无害(删除幂等)。
//   - 补发的每一步都是 next_seq CAS,重叠执行最多多发一次提醒。
//   - gauge 只由领导者产出,卸任时归零,避免 max() 聚合里留着卸任副本的旧值。
//   - 对账只读,重叠只会让 mismatch 计数在两个副本上各记一次(告警用速率 > 0,不影响结论)。
//
// 关停(设计 §10):先取消循环与选举,有界等待它们退出,再由装配层关连接池 —— Run 返回即全部轮次已结束。

// LoopConfig 是后台循环参数(来自 config.PlayerInboxConf)。
type LoopConfig struct {
	TerminalRetention time.Duration
	SweepInterval     time.Duration
	NotifyInterval    time.Duration
	GaugeInterval     time.Duration
	// ReconcileInterval 为 0 = 不跑对账。
	ReconcileInterval time.Duration
	ReconcileWindow   time.Duration
}

// 每轮的工作量上限。
const (
	// sweepBudgetPerRound:每轮最多删 500 行(设计 §9.1)。
	sweepBudgetPerRound = 500
	// notifyPerShardLimit:补发每个分片每轮最多 100 条(设计 §6.3)。
	notifyPerShardLimit = 100
	// notifyGrace:补发只扫 notify_due_ms ≤ now − 5s 的行,不与请求内那次提醒抢(设计 §6.3)。
	notifyGrace = 5 * time.Second
	// notifyOverdue:提醒积压超过 5 分钟计入 player_inbox_notify_backlog(设计 §9.4 warning)。
	notifyOverdue = 5 * time.Minute
	// roundBudgetCap:单轮的整体上限,避免一轮拖到下一轮之后;关停时 ctx 取消立即生效。
	roundBudgetCap = 2 * time.Minute
)

// Campaign 是「竞选领导者并在任期内运行 whileLeader」的函数形状,生产为 (*leader.Elector).Run。
// 抽成函数让「只在领导者上执行」可单测:假实现可以从不授予领导权。
type Campaign func(ctx context.Context, whileLeader func(ctx context.Context))

// MaintenanceStore 是后台循环需要的存储能力(*Store 实现它;单测注入假实现)。
type MaintenanceStore interface {
	SweepTerminal(ctx context.Context, cutoffMs uint64, budget int) (int, error)
	DueNotifications(ctx context.Context, shard uint32, dueBeforeMs uint64, limit int) ([]NotifyDue, error)
	ResendNotify(ctx context.Context, d NotifyDue) NotifyResult
	ScanGauges(ctx context.Context, nowMs, notifyOverdueBeforeMs uint64) (metrics.ScanGauges, error)
	ReconcileRecent(ctx context.Context, sinceMs uint64, ledgers LedgerReader) (int, error)
}

// Loops 持有后台循环的依赖。构造后只读。
type Loops struct {
	store    MaintenanceStore
	ledgers  LedgerReader
	cfg      LoopConfig
	campaign Campaign
	now      func() time.Time
}

// NewLoops 校验参数并构造。ledgers 可为 nil:此时不跑对账(只在 ReconcileInterval > 0 时要求非 nil)。
// now 为 nil 时取 time.Now。
func NewLoops(store MaintenanceStore, ledgers LedgerReader, cfg LoopConfig, campaign Campaign, now func() time.Time) (*Loops, error) {
	if store == nil || campaign == nil {
		return nil, errors.New("inbox: NewLoops requires store and campaign")
	}
	if now == nil {
		now = time.Now
	}
	if cfg.TerminalRetention <= constants.IssuedAtMaxPast {
		return nil, errors.New("inbox: terminal retention must exceed the issued_at window (7d)")
	}
	if cfg.SweepInterval <= 0 || cfg.NotifyInterval <= 0 || cfg.GaugeInterval <= 0 || cfg.ReconcileInterval < 0 {
		return nil, errors.New("inbox: loop intervals must be positive (reconcile may be 0 = disabled)")
	}
	if cfg.ReconcileInterval > 0 && (ledgers == nil || cfg.ReconcileWindow <= 0) {
		return nil, errors.New("inbox: reconcile enabled but ledger reader or window missing")
	}
	return &Loops{store: store, ledgers: ledgers, cfg: cfg, campaign: campaign, now: now}, nil
}

// Run 阻塞运行候选者循环直到 ctx 结束;当选后起各循环,丢失领导权或 ctx 结束即等全部轮次退出。
func (l *Loops) Run(ctx context.Context) {
	l.campaign(ctx, l.whileLeader)
}

// whileLeader 在任期内跑各循环;lctx 取消(丢锁 / 关停)后等它们全部退出再返回,
// 这样选主器释放锁、装配层关库时不会有在途轮次。
func (l *Loops) whileLeader(lctx context.Context) {
	logx.Info("[player_inbox] Maintenance leader acquired: sweep / notify resend / gauges / reconcile run on this replica")
	var wg sync.WaitGroup
	every(lctx, &wg, "player_inbox.sweep", l.cfg.SweepInterval, l.sweepRound)
	every(lctx, &wg, "player_inbox.notify_resend", l.cfg.NotifyInterval, l.notifyRound)
	every(lctx, &wg, "player_inbox.gauges", l.cfg.GaugeInterval, l.gaugeRound)
	if l.cfg.ReconcileInterval > 0 {
		every(lctx, &wg, "player_inbox.reconcile", l.cfg.ReconcileInterval, l.reconcileRound)
	}
	<-lctx.Done()
	wg.Wait()
	metrics.ResetScanGauges()
	logx.Info("[player_inbox] Maintenance leader released; loops stopped")
}

// every 起一个可等待的定时循环:先等一个间隔再跑第一轮(启动风暴期不抢库),每一轮各自 recover
// (safego.Run),ctx 取消即退出;wg 在 goroutine 退出时 Done。
func every(ctx context.Context, wg *sync.WaitGroup, point string, interval time.Duration, round func(context.Context)) {
	wg.Add(1)
	safego.Go(point, func() {
		defer wg.Done()
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				safego.Run(point, func() {
					roundCtx, cancel := context.WithTimeout(ctx, min(interval, roundBudgetCap))
					defer cancel()
					round(roundCtx)
				})
			}
		}
	})
}

func (l *Loops) nowMs() uint64 {
	ms := l.now().UnixMilli()
	if ms < 0 {
		return 0
	}
	return uint64(ms)
}

// sweepRound 清理保留期外的终态行(设计 §9.1)。
func (l *Loops) sweepRound(ctx context.Context) {
	nowMs := l.nowMs()
	retentionMs := uint64(l.cfg.TerminalRetention.Milliseconds())
	if nowMs <= retentionMs {
		return
	}
	deleted, err := l.store.SweepTerminal(ctx, nowMs-retentionMs, sweepBudgetPerRound)
	if err != nil && !errors.Is(err, context.Canceled) {
		logx.Errorf("[player_inbox] Sweep round failed after deleting %d rows: %v", deleted, err)
		return
	}
	if deleted > 0 {
		logx.Infof("[player_inbox] Sweep deleted %d terminal rows older than %v", deleted, l.cfg.TerminalRetention)
	}
}

// notifyRound 按分片轮转一轮补发到期提醒(设计 §6.3)。
func (l *Loops) notifyRound(ctx context.Context) {
	nowMs := l.nowMs()
	grace := uint64(notifyGrace.Milliseconds())
	if nowMs <= grace {
		return
	}
	for shard := uint32(0); shard < ShardCount; shard++ {
		if ctx.Err() != nil {
			return
		}
		due, err := l.store.DueNotifications(ctx, shard, nowMs-grace, notifyPerShardLimit)
		if err != nil {
			logx.Errorf("[player_inbox] Notify resend scan failed shard=%d: %v", shard, err)
			continue
		}
		for _, d := range due {
			if ctx.Err() != nil {
				return
			}
			notifyCtx, cancel := context.WithTimeout(ctx, constants.NotifyTimeout*5)
			l.store.ResendNotify(notifyCtx, d)
			cancel()
		}
	}
}

// gaugeRound 刷新扫描型 gauge(设计 §9.4)。
func (l *Loops) gaugeRound(ctx context.Context) {
	nowMs := l.nowMs()
	overdue := uint64(notifyOverdue.Milliseconds())
	if nowMs <= overdue {
		return
	}
	g, err := l.store.ScanGauges(ctx, nowMs, nowMs-overdue)
	if err != nil {
		if !errors.Is(err, context.Canceled) {
			logx.Errorf("[player_inbox] Gauge scan failed: %v", err)
		}
		return
	}
	metrics.SetScanGauges(g)
}

// reconcileRound 跑一轮定时对账(设计 §9.3)。
func (l *Loops) reconcileRound(ctx context.Context) {
	nowMs := l.nowMs()
	window := uint64(l.cfg.ReconcileWindow.Milliseconds())
	since := uint64(0)
	if nowMs > window {
		since = nowMs - window
	}
	mismatches, err := l.store.ReconcileRecent(ctx, since, l.ledgers)
	if err != nil && !errors.Is(err, context.Canceled) {
		logx.Errorf("[player_inbox] Reconcile round failed: %v", err)
		return
	}
	if mismatches > 0 {
		logx.Errorf("[player_inbox] Reconcile round found %d mismatches (see preceding ERROR lines)", mismatches)
	}
}
