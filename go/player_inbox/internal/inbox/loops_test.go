package inbox

import (
	"context"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"player_inbox/internal/metrics"

	componentpb "proto/common/component"
)

// fakeMaintenance 记录每类后台操作被调用的次数与参数。
type fakeMaintenance struct {
	mu           sync.Mutex
	sweeps       []uint64 // cutoffMs
	dueScans     []uint32 // shard
	dueBefore    []uint64
	resends      []NotifyDue
	gaugeScans   int
	reconcileArg []uint64 // sinceMs
	due          map[uint32][]NotifyDue
	calls        atomic.Int64
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

func (f *fakeMaintenance) ScanGauges(_ context.Context, _, _ uint64) (metrics.ScanGauges, error) {
	f.calls.Add(1)
	f.mu.Lock()
	defer f.mu.Unlock()
	f.gaugeScans++
	return metrics.ScanGauges{OpenItems: 3}, nil
}

func (f *fakeMaintenance) ReconcileRecent(_ context.Context, sinceMs uint64, _ LedgerReader) (int, error) {
	f.calls.Add(1)
	f.mu.Lock()
	defer f.mu.Unlock()
	f.reconcileArg = append(f.reconcileArg, sinceMs)
	return 0, nil
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

// TestSweepAndReconcileWindows:清理截止 = now − 保留期;对账起点 = now − 窗口。
func TestSweepAndReconcileWindows(t *testing.T) {
	store := &fakeMaintenance{}
	cfg := testLoopConfig()
	l, err := NewLoops(store, nopLedgers{}, cfg, func(context.Context, func(context.Context)) {}, fixedNow)
	if err != nil {
		t.Fatal(err)
	}
	l.sweepRound(context.Background())
	l.reconcileRound(context.Background())
	if want := testNowMs - uint64(cfg.TerminalRetention.Milliseconds()); len(store.sweeps) != 1 || store.sweeps[0] != want {
		t.Fatalf("sweep cutoff = %v, want [%d]", store.sweeps, want)
	}
	if want := testNowMs - uint64(cfg.ReconcileWindow.Milliseconds()); len(store.reconcileArg) != 1 || store.reconcileArg[0] != want {
		t.Fatalf("reconcile since = %v, want [%d]", store.reconcileArg, want)
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
