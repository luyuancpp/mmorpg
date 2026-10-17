package inbox

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"player_inbox/internal/metrics"

	componentpb "proto/common/component"
)

// fakeMaintenance 记录每类后台操作被调用的次数与参数。
type fakeMaintenance struct {
	mu             sync.Mutex
	sweeps         []uint64 // cutoffMs
	dueScans       []uint32 // shard
	dueBefore      []uint64
	resends        []NotifyDue
	gaugeScans     int
	gaugeNow       []uint64
	gaugeCuts      []GaugeCutoffs
	reconcileArg   []uint64 // sinceMs
	reconcileStart []uint32 // startShard
	due            map[uint32][]NotifyDue
	calls          atomic.Int64
}

func (f *fakeMaintenance) SweepTerminal(_ context.Context, cutoffMs uint64, _ int) (int, error) {
	f.calls.Add(1)
	f.mu.Lock()
	defer f.mu.Unlock()
	f.sweeps = append(f.sweeps, cutoffMs)
	return 0, nil
}

func (f *fakeMaintenance) DueNotifications(_ context.Context, shard uint32, dueBeforeMs uint64, _ int) ([]NotifyDue, error) {
	f.calls.Add(1)
	f.mu.Lock()
	defer f.mu.Unlock()
	f.dueScans = append(f.dueScans, shard)
	f.dueBefore = append(f.dueBefore, dueBeforeMs)
	return f.due[shard], nil
}

func (f *fakeMaintenance) ResendNotify(_ context.Context, d NotifyDue) NotifyResult {
	f.calls.Add(1)
	f.mu.Lock()
	defer f.mu.Unlock()
	f.resends = append(f.resends, d)
	return NotifySent
}

func (f *fakeMaintenance) ScanGauges(_ context.Context, nowMs uint64, cut GaugeCutoffs) (metrics.ScanGauges, error) {
	f.calls.Add(1)
	f.mu.Lock()
	defer f.mu.Unlock()
	f.gaugeScans++
	f.gaugeNow = append(f.gaugeNow, nowMs)
	f.gaugeCuts = append(f.gaugeCuts, cut)
	return metrics.ScanGauges{OpenItems: 3}, nil
}

func (f *fakeMaintenance) ReconcileRecent(_ context.Context, sinceMs uint64, startShard uint32, _ LedgerReader) (ReconcileRoundResult, error) {
	f.calls.Add(1)
	f.mu.Lock()
	defer f.mu.Unlock()
	f.reconcileArg = append(f.reconcileArg, sinceMs)
	f.reconcileStart = append(f.reconcileStart, startShard)
	return ReconcileRoundResult{}, nil
}

type nopLedgers struct{}

func (nopLedgers) ReadInboxLedger(context.Context, uint64) (*componentpb.PlayerInboxLedgerComp, bool, error) {
	return nil, false, nil
}

func testLoopConfig() LoopConfig {
	return LoopConfig{
		TerminalRetention: 30 * 24 * time.Hour,
		SweepInterval:     time.Millisecond,
		NotifyInterval:    time.Millisecond,
		GaugeInterval:     time.Millisecond,
		ReconcileInterval: time.Millisecond,
		ReconcileWindow:   24 * time.Hour,
	}
}

func fixedNow() time.Time { return time.UnixMilli(int64(testNowMs)) }

// TestLoopsNeverRunWithoutLeadership:选主没当选,任何后台操作都不执行(设计 §6.3、§10:只在领导者上跑)。
func TestLoopsNeverRunWithoutLeadership(t *testing.T) {
	store := &fakeMaintenance{}
	follower := func(ctx context.Context, _ func(context.Context)) { <-ctx.Done() } // 永远当跟随者
	l, err := NewLoops(store, nopLedgers{}, testLoopConfig(), follower, fixedNow)
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() { l.Run(ctx); close(done) }()
	time.Sleep(50 * time.Millisecond) // 足以跑几十个 1ms 间隔 —— 如果它会跑的话
	cancel()
	<-done
	if n := store.calls.Load(); n != 0 {
		t.Fatalf("跟随者执行了 %d 次后台操作", n)
	}
}

// TestLoopsRunWhileLeaderAndStopOnLoss:当选后各循环都跑;任期结束 whileLeader 等全部轮次退出才返回。
func TestLoopsRunWhileLeaderAndStopOnLoss(t *testing.T) {
	store := &fakeMaintenance{}
	leaderCtx, loseLeadership := context.WithCancel(context.Background())
	returned := make(chan struct{})
	campaign := func(ctx context.Context, whileLeader func(context.Context)) {
		whileLeader(leaderCtx)
		close(returned)
		<-ctx.Done()
	}
	l, err := NewLoops(store, nopLedgers{}, testLoopConfig(), campaign, fixedNow)
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go l.Run(ctx)

	deadline := time.Now().Add(5 * time.Second)
	for {
		store.mu.Lock()
		ok := len(store.sweeps) > 0 && len(store.dueScans) > 0 && store.gaugeScans > 0 && len(store.reconcileArg) > 0
		store.mu.Unlock()
		if ok {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("当选后 5s 内各循环没有全部跑过")
		}
		time.Sleep(time.Millisecond)
	}
	loseLeadership()
	select {
	case <-returned:
	case <-time.After(5 * time.Second):
		t.Fatal("丢失领导权后 whileLeader 没有返回")
	}
	after := store.calls.Load()
	time.Sleep(20 * time.Millisecond)
	if store.calls.Load() != after {
		t.Fatal("whileLeader 返回之后仍有后台操作在执行")
	}
}

// TestNotifyRoundScansAllShardsWithGrace:补发一轮按 64 个分片轮转,只扫 now−5s 之前到期的行,并逐条补发。
func TestNotifyRoundScansAllShardsWithGrace(t *testing.T) {
	store := &fakeMaintenance{due: map[uint32][]NotifyDue{
		3:  {{PlayerID: 3, Domain: gmDomain, NextSeq: 10}},
		63: {{PlayerID: 127, Domain: gmDomain, NextSeq: 20}, {PlayerID: 191, Domain: gmDomain, NextSeq: 30}},
	}}
	l, err := NewLoops(store, nopLedgers{}, testLoopConfig(), func(context.Context, func(context.Context)) {}, fixedNow)
	if err != nil {
		t.Fatal(err)
	}
	l.notifyRound(context.Background())
	if len(store.dueScans) != ShardCount {
		t.Fatalf("扫了 %d 个分片,want %d", len(store.dueScans), ShardCount)
	}
	for _, before := range store.dueBefore {
		if before != testNowMs-5000 {
			t.Fatalf("dueBefore = %d, want now-5000 = %d", before, testNowMs-5000)
		}
	}
	if len(store.resends) != 3 {
		t.Fatalf("补发 %d 条,want 3", len(store.resends))
	}
	// 令牌必须是读出时的 next_seq,原样传给 CAS。
	if store.resends[1].NextSeq != 20 || store.resends[2].NextSeq != 30 {
		t.Fatalf("补发令牌 = %+v", store.resends)
	}
}

// TestGaugeRoundCutoffs:扫描型 gauge 的两个截止都按 now 算 —— 提醒积压 = 悬了 5 分钟以上;
// 已投递未确认的告警口径只看最后投递在 24 小时内的行,更早的归入 delivered_unacked_stale(评审 M4)。
func TestGaugeRoundCutoffs(t *testing.T) {
	store := &fakeMaintenance{}
	l, err := NewLoops(store, nopLedgers{}, testLoopConfig(), func(context.Context, func(context.Context)) {}, fixedNow)
	if err != nil {
		t.Fatal(err)
	}
	l.gaugeRound(context.Background())
	want := GaugeCutoffs{
		NotifyOverdueBeforeMs:  testNowMs - uint64((5 * time.Minute).Milliseconds()),
		DeliveredStaleBeforeMs: testNowMs - uint64((24 * time.Hour).Milliseconds()),
	}
	if len(store.gaugeCuts) != 1 || store.gaugeCuts[0] != want || store.gaugeNow[0] != testNowMs {
		t.Fatalf("ScanGauges(now=%v, cut=%+v), want now=%d cut=%+v", store.gaugeNow, store.gaugeCuts, testNowMs, want)
	}
	// 时钟早于窗口(只可能是测试或时钟错乱):截止取 0,不下溢成一个巨大的数。
	if got := gaugeCutoffs(uint64((time.Hour).Milliseconds())); got.DeliveredStaleBeforeMs != 0 ||
		got.NotifyOverdueBeforeMs != uint64((55 * time.Minute).Milliseconds()) {
		t.Fatalf("gaugeCutoffs(1h) = %+v", got)
	}
}

// TestSweepAndReconcileWindows:清理截止 = now − 保留期;对账起点 = now − 窗口,起始分片 = (now / 间隔) % 64。
func TestSweepAndReconcileWindows(t *testing.T) {
	store := &fakeMaintenance{}
	cfg := testLoopConfig()
	cfg.ReconcileInterval = time.Hour
	at := time.UnixMilli(int64(testNowMs) + 5*time.Hour.Milliseconds()) // 落在第 (testNowMs/1h + 5) 个间隔
	l, err := NewLoops(store, nopLedgers{}, cfg, func(context.Context, func(context.Context)) {}, func() time.Time { return at })
	if err != nil {
		t.Fatal(err)
	}
	l.sweepRound(context.Background())
	l.reconcileRound(context.Background())
	nowMs := uint64(at.UnixMilli())
	if want := nowMs - uint64(cfg.TerminalRetention.Milliseconds()); len(store.sweeps) != 1 || store.sweeps[0] != want {
		t.Fatalf("sweep cutoff = %v, want [%d]", store.sweeps, want)
	}
	if want := nowMs - uint64(cfg.ReconcileWindow.Milliseconds()); len(store.reconcileArg) != 1 || store.reconcileArg[0] != want {
		t.Fatalf("reconcile since = %v, want [%d]", store.reconcileArg, want)
	}
	wantStart := uint32((nowMs / uint64(time.Hour.Milliseconds())) % ShardCount)
	if len(store.reconcileStart) != 1 || store.reconcileStart[0] != wantStart {
		t.Fatalf("reconcile start shard = %v, want [%d]", store.reconcileStart, wantStart)
	}
}

// TestReconcileStartShardRotates:起始分片按墙钟所在的间隔轮转、每个间隔前进一片、64 片后回绕;
// 同一个间隔内(例如易主后新领导者立刻补跑)起点不变 —— 不依赖任何内存状态(评审 M5)。
func TestReconcileStartShardRotates(t *testing.T) {
	const interval = time.Hour
	ms := uint64(interval.Milliseconds())
	at := func(k, offsetMs uint64) uint32 { return reconcileStartShard(k*ms+offsetMs, interval) }
	if at(0, 0) != 0 || at(1, 0) != 1 || at(63, 0) != 63 || at(64, 0) != 0 || at(65, 0) != 1 {
		t.Fatalf("轮转: k=0→%d k=1→%d k=63→%d k=64→%d k=65→%d", at(0, 0), at(1, 0), at(63, 0), at(64, 0), at(65, 0))
	}
	if at(7, 1) != 7 || at(7, ms-1) != 7 {
		t.Fatalf("同一个间隔内起点不变: %d %d", at(7, 1), at(7, ms-1))
	}
	for k := uint64(1000); k < 1000+ShardCount; k++ {
		if next := at(k+1, 0); next != (at(k, 0)+1)%ShardCount {
			t.Fatalf("k=%d → %d,下一个间隔应前进一片,得到 %d", k, at(k, 0), next)
		}
	}
	if reconcileStartShard(123456, 0) != 0 {
		t.Fatal("间隔为 0(不跑对账)时起点取 0,不除零")
	}
}

// TestReconcileFirstDelay:当选后第一轮 = 1 分钟 + [0, 4) 分钟抖动,不超过一个间隔;注入的随机源越界也被钳住(评审 L4)。
func TestReconcileFirstDelay(t *testing.T) {
	fixed := func(d time.Duration) func(time.Duration) time.Duration {
		return func(time.Duration) time.Duration { return d }
	}
	cases := []struct {
		name     string
		interval time.Duration
		jitter   time.Duration
		want     time.Duration
	}{
		{"抖动 0 → 1 分钟", time.Hour, 0, time.Minute},
		{"抖动 90s", time.Hour, 90 * time.Second, 150 * time.Second},
		{"抖动顶格 → 不到 5 分钟", time.Hour, 4*time.Minute - time.Nanosecond, 5*time.Minute - time.Nanosecond},
		{"随机源越界(过大)钳到 5 分钟", time.Hour, time.Hour, 5 * time.Minute},
		{"随机源越界(负数)钳到 1 分钟", time.Hour, -time.Minute, time.Minute},
		{"间隔比 1–5 分钟还短:不超过一个间隔", 2 * time.Minute, 3 * time.Minute, 2 * time.Minute},
	}
	for _, tc := range cases {
		if got := reconcileFirstDelay(tc.interval, fixed(tc.jitter)); got != tc.want {
			t.Errorf("%s: first delay = %v, want %v", tc.name, got, tc.want)
		}
	}
	var asked time.Duration
	_ = reconcileFirstDelay(time.Hour, func(n time.Duration) time.Duration { asked = n; return 0 })
	if asked != reconcileFirstDelaySpread {
		t.Errorf("随机源被要求的区间 = %v, want %v", asked, reconcileFirstDelaySpread)
	}
	for i := 0; i < 100; i++ {
		if d := randomJitter(reconcileFirstDelaySpread); d < 0 || d >= reconcileFirstDelaySpread {
			t.Fatalf("randomJitter 越界: %v", d)
		}
	}
	if randomJitter(0) != 0 || randomJitter(-time.Second) != 0 {
		t.Error("n ≤ 0 时 randomJitter 应为 0")
	}
}

// TestNewLoopsUsesInjectedJitter:LoopConfig.Jitter 注入后,当选后第一轮的等待由它决定(单测的确定性来源)。
func TestNewLoopsUsesInjectedJitter(t *testing.T) {
	cfg := testLoopConfig()
	cfg.ReconcileInterval = time.Hour
	cfg.Jitter = func(time.Duration) time.Duration { return 2 * time.Minute }
	l, err := NewLoops(&fakeMaintenance{}, nopLedgers{}, cfg, func(context.Context, func(context.Context)) {}, fixedNow)
	if err != nil {
		t.Fatal(err)
	}
	if got := reconcileFirstDelay(l.cfg.ReconcileInterval, l.jitter); got != 3*time.Minute {
		t.Fatalf("first delay = %v, want 3m(1m + 注入的 2m)", got)
	}
}

// TestEveryAfterHonorsFirstDelay:everyAfter 等 first 跑第一轮,之后按间隔;间隔 1 小时内不会有第二轮。
// 不比较墙钟时长,只看「first 很短时第一轮发生、第二轮没有」。
func TestEveryAfterHonorsFirstDelay(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	var wg sync.WaitGroup
	var rounds atomic.Int64
	everyAfter(ctx, &wg, "player_inbox.test_every_after", time.Millisecond, time.Hour, func(context.Context) { rounds.Add(1) })
	deadline := time.Now().Add(5 * time.Second)
	for rounds.Load() == 0 {
		if time.Now().After(deadline) {
			cancel()
			t.Fatal("first = 1ms 时 5s 内没有跑第一轮")
		}
		time.Sleep(time.Millisecond)
	}
	cancel()
	wg.Wait()
	if n := rounds.Load(); n != 1 {
		t.Fatalf("间隔 1 小时,应只跑了第一轮,got %d", n)
	}
}

// TestReconcileRoundResult 钉住 reconcile_rounds_total{result} 的归类:领导权丢失 / 关停不计数;
// 驱动没把 ctx 错误包进返回值时看 ctx.Err()。
func TestReconcileRoundResult(t *testing.T) {
	live := context.Background()
	canceled, cancel := context.WithCancel(context.Background())
	cancel()
	expired, cancelExpired := context.WithDeadline(context.Background(), time.Unix(0, 0))
	defer cancelExpired()
	other := errors.New("invalid connection")
	cases := []struct {
		name string
		ctx  context.Context
		err  error
		want string
	}{
		{"成功", live, nil, "ok"},
		{"预算用完", live, context.DeadlineExceeded, "timeout"},
		{"预算用完、驱动回了别的错误", expired, other, "timeout"},
		{"包了一层的超时", live, fmt.Errorf("scan: %w", context.DeadlineExceeded), "timeout"},
		{"领导权丢失 / 关停", live, context.Canceled, ""},
		{"ctx 已取消、驱动回了别的错误", canceled, other, ""},
		{"读库失败", live, other, "error"},
	}
	for _, tc := range cases {
		if got := reconcileRoundResult(tc.ctx, tc.err); got != tc.want {
			t.Errorf("%s: result = %q, want %q", tc.name, got, tc.want)
		}
	}
}

// TestReconcileGaugeUpdates:跑完的轮次照实刷新不一致行数;失败 / 超时的轮次只在发现了不一致时刷新(那是确凿的下界),
// 一处没发现就保留上一轮的读数;取消的轮次不刷新(评审 L11)。
func TestReconcileGaugeUpdates(t *testing.T) {
	cases := []struct {
		result string
		found  int
		want   bool
	}{
		{"ok", 0, true}, {"ok", 3, true},
		{"timeout", 0, false}, {"timeout", 2, true},
		{"error", 0, false}, {"error", 1, true},
		{"", 5, false},
	}
	for _, tc := range cases {
		if got := reconcileGaugeUpdates(tc.result, tc.found); got != tc.want {
			t.Errorf("reconcileGaugeUpdates(%q, %d) = %v, want %v", tc.result, tc.found, got, tc.want)
		}
	}
}

// TestNewLoopsValidates:保留期必须大于签发窗;间隔必须为正;开对账必须有账本读取器。
func TestNewLoopsValidates(t *testing.T) {
	campaign := func(context.Context, func(context.Context)) {}
	store := &fakeMaintenance{}
	bad := []func(c *LoopConfig){
		func(c *LoopConfig) { c.TerminalRetention = 7 * 24 * time.Hour },
		func(c *LoopConfig) { c.SweepInterval = 0 },
		func(c *LoopConfig) { c.NotifyInterval = 0 },
		func(c *LoopConfig) { c.GaugeInterval = -1 },
		func(c *LoopConfig) { c.ReconcileWindow = 0 },
		// 窗口不足两个间隔(评审 L4,同 config.Validate)。
		func(c *LoopConfig) { c.ReconcileInterval, c.ReconcileWindow = time.Hour, 2*time.Hour-time.Millisecond },
	}
	for i, mutate := range bad {
		cfg := testLoopConfig()
		mutate(&cfg)
		if _, err := NewLoops(store, nopLedgers{}, cfg, campaign, fixedNow); err == nil {
			t.Errorf("第 %d 个非法配置应被拒绝", i)
		}
	}
	if _, err := NewLoops(store, nil, testLoopConfig(), campaign, fixedNow); err == nil {
		t.Error("开着对账却没有账本读取器应被拒绝")
	}
	cfg := testLoopConfig()
	cfg.ReconcileInterval = 0
	if _, err := NewLoops(store, nil, cfg, campaign, fixedNow); err != nil {
		t.Errorf("关掉对账时不需要账本读取器: %v", err)
	}
	if _, err := NewLoops(nil, nopLedgers{}, testLoopConfig(), campaign, fixedNow); err == nil {
		t.Error("缺 store 应被拒绝")
	}
}
