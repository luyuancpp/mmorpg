package loginqueue

import (
	"context"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/redis/go-redis/v9"
	gzprom "github.com/zeromicro/go-zero/core/prometheus"
)

// integrationProvider satisfies CapacityProvider for end-to-end tests.
// Capacity and candidate list are mutable so a single test can model
// "gates fill up, then drain" without rebuilding the harness.
type integrationProvider struct {
	mu        sync.Mutex
	caps      map[uint32]uint32
	cands     map[uint32][]GateCandidate
	signCount atomic.Int32
}

func (p *integrationProvider) CandidatesForZone(_ context.Context, zoneID uint32) ([]GateCandidate, error) {
	p.mu.Lock()
	defer p.mu.Unlock()
	out := make([]GateCandidate, len(p.cands[zoneID]))
	copy(out, p.cands[zoneID])
	return out, nil
}
func (p *integrationProvider) ZoneCapacity(zoneID uint32) uint32 {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.caps[zoneID]
}

// startIntegrationFixture spins up miniredis + a Queue + a Dispatcher
// running on a tight 50ms tick so tests don't have to sleep seconds. The
// returned cleanup stops the dispatcher and miniredis.
func startIntegrationFixture(t *testing.T, prov *integrationProvider) (*Queue, *Dispatcher, *redis.Client, func()) {
	t.Helper()
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	secret := []byte("integration-test-secret")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	// 这个 fixture 的两个用例(QueueDrainAdmits / DispatcherRespectsCapacity)里 PlayerCount 是写死的,
	// 靠「取走即腾出名额」推进。交票后宽限(生产 15s)在这里设 0:取走时分数 = 现在,下一次计数就清掉,
	// 与 G4b-G 之前的行为等价。宽限本身由 TestConsume_* 与
	// TestDispatchOnce_ConsumedAdmitsHoldCapacityForHandoffGrace 用假时钟钉住。
	q.handoffGrace = 0

	// 用它的两个用例都只有 zone 1:dispatcher 只出自己那一个 zone(G1b)。
	d, err := NewDispatcher(
		q, prov, rdb, secret,
		300*time.Second,     // gateTokenTTL
		50*time.Millisecond, // dispatch interval — fast for tests
		1.5,
		2*time.Second, // lock TTL
		"dispatcher:lock:test",
		1,
	)
	if err != nil {
		t.Fatalf("NewDispatcher: %v", err)
	}
	d.Start()
	cleanup := func() { d.Stop(); mr.Close(); _ = rdb.Close() }
	return q, d, rdb, cleanup
}

// mustNewDispatcher 用测试节拍(30ms 一拍、2s 锁)构造一个只负责 zoneID 的 dispatcher。
func mustNewDispatcher(t *testing.T, q *Queue, prov CapacityProvider, rdb *redis.Client,
	secret []byte, lockKeyBase string, zoneID uint32) *Dispatcher {
	t.Helper()
	d, err := NewDispatcher(q, prov, rdb, secret, 300*time.Second, 30*time.Millisecond, 1.5,
		2*time.Second, lockKeyBase, zoneID)
	if err != nil {
		t.Fatalf("NewDispatcher(zone=%d): %v", zoneID, err)
	}
	return d
}

// stubSignFn is the test-side gate-token signer. The Dispatcher (in the
// fixture above) writes AdmitSlot into admit:{queueId}; Lookup needs a
// SignAdmitFn to turn that slot into an AdmitToken at consume time. The
// stub copies slot fields straight through and stamps a fixed deadline,
// matching what production handlers do via SignGateToken.
func stubSignFn(slot *AdmitSlot) (*AdmitToken, error) {
	return &AdmitToken{
		IP:            slot.IP,
		Port:          slot.Port,
		TokenDeadline: time.Now().Unix() + 300,
	}, nil
}

// waitFor polls fn at 10ms intervals until it returns true or timeout.
// Used to assert against eventual conditions (the dispatcher runs async).
func waitFor(t *testing.T, timeout time.Duration, msg string, fn func() bool) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if fn() {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatalf("timeout waiting for: %s", msg)
}

// TestIntegration_QueueDrainAdmits is the core e2e: enqueue more entries
// than capacity, dispatcher drains over time, every entry eventually
// lands in StatusAdmitted with a unique gate token.
func TestIntegration_QueueDrainAdmits(t *testing.T) {
	prov := &integrationProvider{
		caps: map[uint32]uint32{1: 2}, // small ceiling forces queueing
		cands: map[uint32][]GateCandidate{
			1: {
				{NodeID: 1, IP: "10.0.0.1", Port: 9000, PlayerCount: 0, ZoneID: 1},
				{NodeID: 2, IP: "10.0.0.2", Port: 9000, PlayerCount: 0, ZoneID: 1},
			},
		},
	}
	q, _, _, cleanup := startIntegrationFixture(t, prov)
	defer cleanup()

	ctx := context.Background()

	// Enqueue 5 entries — capacity is only 2, so 3 sit in queue first tick.
	tokens := make([]string, 0, 5)
	for i := range 5 {
		res, err := q.Enqueue(ctx, 1, "alice", "dev")
		if err != nil {
			t.Fatalf("enqueue %d: %v", i, err)
		}
		tokens = append(tokens, res.QueueToken)
	}

	// Dispatcher should drain all 5 within ~1s (capacity 2 + dispatcher
	// PopAdmit each tick; the fixture sets handoffGrace=0, so a consumed
	// admit frees its slot at the next count).
	admitted := make(map[string]string) // token → ip
	waitFor(t, 3*time.Second, "all 5 entries admitted", func() bool {
		for _, tok := range tokens {
			if _, ok := admitted[tok]; ok {
				continue
			}
			st, err := q.Lookup(ctx, tok, stubSignFn)
			if err != nil {
				continue
			}
			if st.Status == StatusAdmitted && st.Admit != nil {
				admitted[tok] = st.Admit.IP
			}
		}
		return len(admitted) == len(tokens)
	})

	// Sanity: each gate token must have a non-empty IP.
	for tok, ip := range admitted {
		if ip == "" {
			t.Errorf("token %s admitted with empty IP", tok)
		}
	}
}

// TestIntegration_DispatcherRespectsCapacity ensures the dispatcher stops
// admitting once free slots are exhausted, even with the queue still
// non-empty. This is the property that prevents over-issuing gate tokens
// during the round-trip window between admit and connect.
func TestIntegration_DispatcherRespectsCapacity(t *testing.T) {
	prov := &integrationProvider{
		caps: map[uint32]uint32{1: 3},
		cands: map[uint32][]GateCandidate{
			1: {
				// Already 3 online — zero free slots up front.
				{NodeID: 1, IP: "10.0.0.1", Port: 9000, PlayerCount: 3, ZoneID: 1},
			},
		},
	}
	q, _, _, cleanup := startIntegrationFixture(t, prov)
	defer cleanup()

	ctx := context.Background()
	// Enqueue 4 entries. Capacity (3) - online (3) = 0 free → none should
	// be admitted while the gate stays full.
	var stuck []string
	for range 4 {
		res, _ := q.Enqueue(ctx, 1, "u", "d")
		stuck = append(stuck, res.QueueToken)
	}

	// Wait long enough for several dispatcher ticks. Then verify NO entry
	// was admitted — capacity is full, queue must hold.
	time.Sleep(500 * time.Millisecond)
	for _, tok := range stuck {
		st, err := q.Lookup(ctx, tok, stubSignFn)
		if err != nil {
			t.Fatalf("lookup: %v", err)
		}
		if st.Status == StatusAdmitted {
			t.Errorf("token %s admitted while gates were full — capacity ignored", tok)
		}
	}

	// Now drop online to 1 → 2 free slots open up. Dispatcher should
	// admit exactly 2 of the 4 within a few ticks.
	prov.mu.Lock()
	prov.cands[1] = []GateCandidate{
		{NodeID: 1, IP: "10.0.0.1", Port: 9000, PlayerCount: 1, ZoneID: 1},
	}
	prov.mu.Unlock()

	admittedCount := 0
	waitFor(t, 2*time.Second, "exactly 2 admitted after capacity opened", func() bool {
		admittedCount = 0
		for _, tok := range stuck {
			st, err := q.Lookup(ctx, tok, stubSignFn)
			if err == nil && st.Status == StatusAdmitted {
				admittedCount++
			}
		}
		return admittedCount >= 2
	})
	if admittedCount > 2 {
		t.Errorf("admitted %d while only 2 slots opened — over-issued tokens", admittedCount)
	}
}

// TestIntegration_LeaderElectionSerializesDispatch starts two dispatchers
// against the same Redis. Only one should hold the leader lock at a time,
// so admitted counts should not double up.
func TestIntegration_LeaderElectionSerializesDispatch(t *testing.T) {
	mr, _ := miniredis.Run()
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()

	prov := &integrationProvider{
		caps: map[uint32]uint32{1: 100},
		cands: map[uint32][]GateCandidate{
			1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, PlayerCount: 0, ZoneID: 1}},
		},
	}
	secret := []byte("test")
	q := New(rdb, time.Hour, 60*time.Second, secret)

	// 同一个 zone、同一个前缀 = 同一把 zone 锁,两个副本互为主备。
	d1 := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:dual", 1)
	d2 := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:dual", 1)

	d1.Start()
	d2.Start()
	defer d1.Stop()
	defer d2.Stop()

	ctx := context.Background()
	const N = 10
	for range N {
		_, _ = q.Enqueue(ctx, 1, "u", "d")
	}

	// All N must drain — the property is "no entry vanishes due to
	// double-pop", not "drain happens twice as fast".
	waitFor(t, 3*time.Second, "all entries drained", func() bool {
		// We cheat by checking ZCARD: dispatcher pops members out, so
		// queue length monotonically drops to zero when all admitted.
		n, _ := q.QueueLen(ctx, 1)
		return n == 0
	})

	// 同前缀同 zone 是同一把锁:库里以这个前缀开头的键恰好一把,就是 zone 1 的锁键。
	// 只看队列出空证明不了这一点,两把互不相干的锁(双 leader)也能出空。
	var lockKeys []string
	for _, key := range mr.Keys() {
		if strings.HasPrefix(key, "dispatcher:lock:dual") {
			lockKeys = append(lockKeys, key)
		}
	}
	if want := ZoneDispatcherLockKey("dispatcher:lock:dual", 1); len(lockKeys) != 1 || lockKeys[0] != want {
		t.Fatalf("lock keys with prefix dispatcher:lock:dual = %v, want exactly [%s]", lockKeys, want)
	}
}

// 锁键按 zone 拆:同一前缀下每个 zone 一把,互不相同,也都不等于前缀本身(旧的全服键)。
func TestZoneDispatcherLockKey_IsPerZone(t *testing.T) {
	const base = "dispatcher:lock:login_queue"
	if got, want := ZoneDispatcherLockKey(base, 1), "dispatcher:lock:login_queue:zone:1"; got != want {
		t.Fatalf("ZoneDispatcherLockKey(%q, 1) = %q, want %q", base, got, want)
	}
	seen := make(map[string]uint32)
	for _, zone := range []uint32{1, 2, 10, 101} {
		key := ZoneDispatcherLockKey(base, zone)
		if key == base {
			t.Fatalf("zone %d lock key must not equal the legacy global key %q", zone, base)
		}
		if prev, dup := seen[key]; dup {
			t.Fatalf("zone %d and zone %d share lock key %q", prev, zone, key)
		}
		seen[key] = zone
	}
}

// 构造期校验先于任何使用:zone 0 或空前缀直接报错,q / prov / rdb 传 nil 也不会被碰到。
func TestNewDispatcher_RejectsZeroZoneOrEmptyLockKeyBase(t *testing.T) {
	cases := []struct {
		name        string
		lockKeyBase string
		zoneID      uint32
	}{
		{name: "zero zone", lockKeyBase: "dispatcher:lock:login_queue", zoneID: 0},
		{name: "empty lock key base", lockKeyBase: "", zoneID: 1},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			d, err := NewDispatcher(nil, nil, nil, nil, 300*time.Second, 30*time.Millisecond, 1.5,
				2*time.Second, tc.lockKeyBase, tc.zoneID)
			if d != nil || err == nil {
				t.Fatalf("NewDispatcher(lockKeyBase=%q, zone=%d) = (%v, %v), want (nil, error)",
					tc.lockKeyBase, tc.zoneID, d, err)
			}
		})
	}
}

// 出队间隔或锁 TTL 非正同样是「开了排队却出不了队」:前者让 drainLoop 的 NewTicker 在拿到锁之后
// panic,后者让锁键不带过期、心跳不起,leader 异常退出后该 zone 永久没有 leader。构造期就拒绝,
// 与 zone 0 / 空前缀同一口径;q / prov / rdb 传 nil,证明校验先于任何使用。
func TestNewDispatcher_RejectsNonPositiveIntervalOrLockTTL(t *testing.T) {
	cases := []struct {
		name     string
		interval time.Duration
		lockTTL  time.Duration
	}{
		{name: "zero interval", interval: 0, lockTTL: 2 * time.Second},
		{name: "zero lock ttl", interval: 30 * time.Millisecond, lockTTL: 0},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			d, err := NewDispatcher(nil, nil, nil, nil, 300*time.Second, tc.interval, 1.5,
				tc.lockTTL, "dispatcher:lock:login_queue", 1)
			if d != nil || err == nil {
				t.Fatalf("NewDispatcher(interval=%v, lockTTL=%v) = (%v, %v), want (nil, error)",
					tc.interval, tc.lockTTL, d, err)
			}
		})
	}
}

// G1b 回归:两个 zone 的 login 共用一个 Redis,配置里的 DispatcherLockKey 一模一样(login.yaml 默认值),
// 各自只看得见本 zone 的 gate。每个 zone 都要有自己的 leader,队列都要出空。
// 旧实现锁键全服一把,只有抢到锁的 zone 出队,另一个 zone 一个人都放不进来。
func TestIntegration_TwoZonesSharedRedis_BothQueuesDrain(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	const base = "dispatcher:lock:login_queue"
	newZoneDispatcher := func(zone uint32) *Dispatcher {
		// 照生产形状:每个 zone 的 provider 只认得本 zone 的 gate(GateWatcher 前缀带 ZoneId)。
		prov := &integrationProvider{
			caps:  map[uint32]uint32{zone: 100},
			cands: map[uint32][]GateCandidate{zone: {{NodeID: zone, IP: "10.0.0.1", Port: 9000, ZoneID: zone}}},
		}
		return mustNewDispatcher(t, q, prov, rdb, secret, base, zone)
	}
	d1, d2 := newZoneDispatcher(1), newZoneDispatcher(2)
	d1.Start()
	defer d1.Stop()
	d2.Start()
	defer d2.Stop()

	for range 5 {
		if _, err := q.Enqueue(ctx, 1, "zone1-account", "dev"); err != nil {
			t.Fatalf("enqueue zone 1: %v", err)
		}
		if _, err := q.Enqueue(ctx, 2, "zone2-account", "dev"); err != nil {
			t.Fatalf("enqueue zone 2: %v", err)
		}
	}
	waitFor(t, 3*time.Second, "zone 1 and zone 2 queues both drained", func() bool {
		n1, err1 := q.QueueLen(ctx, 1)
		n2, err2 := q.QueueLen(ctx, 2)
		return err1 == nil && err2 == nil && n1 == 0 && n2 == 0
	})

	// 锁的形状:每个 zone 一把,全服旧键没人写。
	n, err := rdb.Exists(ctx, ZoneDispatcherLockKey(base, 1), ZoneDispatcherLockKey(base, 2)).Result()
	if err != nil || n != 2 {
		t.Fatalf("each zone must hold its own leader lock: exists=%d err=%v", n, err)
	}
	if mr.Exists(base) {
		t.Fatalf("legacy global lock key %q must not be written any more", base)
	}
}

// 滚动升级窗口:老版本持全服旧键、新版本持 zone 键,两把锁不互斥,同一个 zone 出现两个 leader。
// 两个 leader 同时出队同一个 zone 时:不丢人、不重复放行(ZPOPMIN 原子),超发有界(≤ 2×cap)。
func TestIntegration_DualLeadersSameZone_NoLossBoundedOverAdmit(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-dual")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	const capacity, total = 4, 20
	prov := &integrationProvider{
		caps:  map[uint32]uint32{1: capacity},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}
	want := make(map[string]bool, total)
	for range total {
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		want[res.QueueID] = true
	}
	// 两个不同前缀 = 两把互不相干的锁 = 同一 zone 两个 leader(滚动升级窗口的形状)。
	const newBase, oldBase = "dispatcher:lock:login_queue", "dispatcher:lock:legacy_window"
	dNew := mustNewDispatcher(t, q, prov, rdb, secret, newBase, 1)
	dOld := mustNewDispatcher(t, q, prov, rdb, secret, oldBase, 1)
	dNew.Start()
	defer dNew.Stop()
	dOld.Start()
	defer dOld.Stop()

	// 两把锁都被持有 = 真的是双 leader;否则下面的断言在单 leader 下也会过,证明不了什么。
	// 用 waitFor 而不是一次性检查:两个 goroutine 抢锁的先后不确定,先饱和的那一刻另一把可能还没拿到。
	// 两把键互不竞争,拿到后由心跳续着(miniredis 不按墙钟过期),不会再丢。
	waitFor(t, 3*time.Second, "dual-leader shape: both zone locks held", func() bool {
		held, err := rdb.Exists(ctx, ZoneDispatcherLockKey(newBase, 1), ZoneDispatcherLockKey(oldBase, 1)).Result()
		return err == nil && held == 2
	})

	snapshot := func() (queued, admitted []string) {
		queued, _ = rdb.ZRange(ctx, zoneZSetKey(1), 0, -1).Result()
		admitted, _ = rdb.ZRange(ctx, admittedZSetKey(1), 0, -1).Result()
		return queued, admitted
	}
	waitFor(t, 3*time.Second, "both leaders saturated the zone", func() bool {
		qd, ad := snapshot()
		return len(ad) >= capacity && len(qd)+len(ad) == total
	})
	time.Sleep(300 * time.Millisecond) // 再给每个 leader 约十拍,确认余量为 0 后不再放行
	queued, admitted := snapshot()
	if len(admitted) > 2*capacity {
		t.Fatalf("over-admission must be bounded by 2×cap=%d, got %d", 2*capacity, len(admitted))
	}
	seen := make(map[string]int, total)
	for _, id := range append(queued, admitted...) {
		seen[id]++
	}
	if len(seen) != total {
		t.Fatalf("entries lost: tracked %d of %d", len(seen), total)
	}
	for id, c := range seen {
		if c != 1 || !want[id] {
			t.Fatalf("queueId %s appears %d times (known=%v)", id, c, want[id])
		}
	}
}

// countingProvider 包一层 CapacityProvider,数 CandidatesForZone 被调了几次,
// 用来证明空队列那一拍没碰 gate 目录(生产实现每次都要取 etcd 快照、查排空标记)。
type countingProvider struct {
	CapacityProvider
	candidateCalls atomic.Int32
}

func (p *countingProvider) CandidatesForZone(ctx context.Context, zoneID uint32) ([]GateCandidate, error) {
	p.candidateCalls.Add(1)
	return p.CapacityProvider.CandidatesForZone(ctx, zoneID)
}

// G1b 之后本 zone 每拍都在出队范围内(以前要本 zone 有 gate 才进 activeZones)。
// 先判空再算容量:空闲的 zone 不碰 gate 目录,非空才走 FreeSlots。
func TestDispatchOnce_EmptyQueueSkipsGateDirectory(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-empty")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	prov := &countingProvider{CapacityProvider: &integrationProvider{
		caps:  map[uint32]uint32{1: 10},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}}
	// 不 Start:直接调 dispatchOnce,没有 goroutine,结果确定。没 Start 过的 dispatcher 不能 Stop。
	d := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:empty", 1)

	for range 3 {
		d.dispatchOnce(ctx)
	}
	if n := prov.candidateCalls.Load(); n != 0 {
		t.Fatalf("empty queue must not touch the gate directory, CandidatesForZone called %d times", n)
	}

	if _, err := q.Enqueue(ctx, 1, "u", "d"); err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	d.dispatchOnce(ctx)
	if prov.candidateCalls.Load() == 0 {
		t.Fatal("non-empty queue must compute capacity from the gate directory")
	}
	if n, err := q.QueueLen(ctx, 1); err != nil || n != 0 {
		t.Fatalf("queue should be drained after one pass: len=%d err=%v", n, err)
	}
}

// G4b-G(Q2 回归,dispatcher 层):取走放行槽后,gate 要到下一次上报才把人数算进 PlayerCount。
// 这几拍里占位必须留着,否则每一拍都按「在线 0、在途 0」再放一满批。
func TestDispatchOnce_ConsumedAdmitsHoldCapacityForHandoffGrace(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g4bg")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	clk := newFakeClock()
	q.now = clk.Now
	ctx := context.Background()

	prov := &integrationProvider{
		caps:  map[uint32]uint32{1: 2},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}
	// 不 Start:直接调 dispatchOnce,没有 goroutine,结果确定。
	d := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:g4bg", 1)

	tokens := make([]string, 0, 6)
	for range 6 {
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		tokens = append(tokens, res.QueueToken)
	}
	mustQueueLen := func(want uint32, when string) {
		t.Helper()
		n, err := q.QueueLen(ctx, 1)
		if err != nil || n != want {
			t.Fatalf("queue len (%s) = %d err=%v, want %d", when, n, err, want)
		}
	}

	d.dispatchOnce(ctx)
	mustQueueLen(4, "first tick admits exactly the capacity")

	taken := 0
	for _, token := range tokens {
		st, err := q.Lookup(ctx, token, stubSignFn)
		if err != nil {
			t.Fatalf("lookup: %v", err)
		}
		if st.Status == StatusAdmitted {
			taken++
		}
	}
	if taken != 2 {
		t.Fatalf("consumed %d admits, want 2", taken)
	}

	// gate 还没上报的 5 拍:一个都不能再放。
	for range 5 {
		clk.Advance(time.Second)
		d.dispatchOnce(ctx)
	}
	mustQueueLen(4, "ticks before the gate reports: consumed admits still hold their slots")

	// 宽限过去,这两个人始终没出现在 PlayerCount 里:名额放回,再放一批。
	clk.Advance(q.handoffGrace)
	d.dispatchOnce(ctx)
	mustQueueLen(2, "handoff grace elapsed")
}

// 规格 §4.3:队列非空,但本 zone 一台 gate 都没有。每拍报一条 freeSlots ERROR,条目留在队列里,不丢人、不放行。
// T9 的 provider 始终有一台 gate,覆盖不到这条。
func TestDispatchOnce_NonEmptyQueueWithoutGates_KeepsEntries(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-nogate")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	prov := &integrationProvider{caps: map[uint32]uint32{1: 10}, cands: map[uint32][]GateCandidate{}}
	// 不 Start:直接调 dispatchOnce,没有 goroutine,结果确定。
	d := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:nogate", 1)

	if _, err := q.Enqueue(ctx, 1, "u", "d"); err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	for range 3 {
		d.dispatchOnce(ctx)
	}
	if n, err := q.QueueLen(ctx, 1); err != nil || n != 1 {
		t.Fatalf("queue len = %d err=%v, want 1: the entry must stay queued while the zone has no gate", n, err)
	}
	if n, err := q.AdmittedCount(ctx, 1); err != nil || n != 0 {
		t.Fatalf("admitted count = %d err=%v, want 0: nothing may be admitted without a gate", n, err)
	}
}

// 同 zone 主备切换:leader 正常停机放锁,同 zone 的备机在下一次重试时接管并出队
// (测试参数下重试周期 max(2s/3, 1s) = 1s)。T7 只断言出空,覆盖不到「Stop 放锁 → 备机接管」这条路径。
func TestIntegration_SameZoneStandbyTakesOverAfterLeaderStops(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-failover")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	const base = "dispatcher:lock:failover"
	lockKey := ZoneDispatcherLockKey(base, 1)
	prov := &integrationProvider{
		caps:  map[uint32]uint32{1: 100},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}
	d1 := mustNewDispatcher(t, q, prov, rdb, secret, base, 1)
	d2 := mustNewDispatcher(t, q, prov, rdb, secret, base, 1)

	d1.Start()
	defer d1.Stop() // Stop 可重复调用;中途 Fatal 时由它收尾
	// d2 还没起,锁出现就是 d1 拿到的。
	waitFor(t, 3*time.Second, "d1 holds the zone lock", func() bool { return mr.Exists(lockKey) })
	d2.Start()
	defer d2.Stop()
	d1.Stop()

	const N = 5
	for range N {
		if _, err := q.Enqueue(ctx, 1, "u", "d"); err != nil {
			t.Fatalf("enqueue: %v", err)
		}
	}
	// d1 已停,只剩 d2 能出队。
	waitFor(t, 3*time.Second, "standby d2 took over and drained the queue", func() bool {
		n, err := q.QueueLen(ctx, 1)
		return err == nil && n == 0
	})
	if !mr.Exists(lockKey) {
		t.Fatalf("zone lock %q must be held by the new leader", lockKey)
	}
}

// panicOnCandidatesProvider 模拟出队路径上的代码缺陷:一碰 gate 目录就 panic。
// 队列非空时 dispatchOnce 才会走到 CandidatesForZone(先判空再算容量)。
type panicOnCandidatesProvider struct {
	calls atomic.Int32
}

func (p *panicOnCandidatesProvider) CandidatesForZone(context.Context, uint32) ([]GateCandidate, error) {
	p.calls.Add(1)
	panic("loginqueue test: injected gate directory defect")
}

func (p *panicOnCandidatesProvider) ZoneCapacity(uint32) uint32 { return 100 }

// G1b-consistency-2 回归:任期内 panic(这里是出队路径上的 provider 缺陷,队列非空才碰得到)。
// 任期的收尾(停心跳、leader 指标归 0、放锁)全在 defer 里,panic 展开时照样执行;safego 按任期接住后
// 退避再参选。同 zone 的另一个副本因此能抢到锁、把队列出空。
// 修复前收尾是直线代码:panic 被 safego 接住、选主 goroutine 退出,心跳 goroutine 却还在续期,
// 这个副本成了「持锁不出队」的僵尸 leader —— 只 panic 一次、锁键一直在,本用例在第一个 waitFor 处红。
func TestIntegration_PanickedTermReleasesLockForSameZoneStandby(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-panic")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	const base = "dispatcher:lock:panic"
	lockKey := ZoneDispatcherLockKey(base, 1)
	for range 3 {
		if _, err := q.Enqueue(ctx, 1, "u", "d"); err != nil {
			t.Fatalf("enqueue: %v", err)
		}
	}

	bad := &panicOnCandidatesProvider{}
	dBad := mustNewDispatcher(t, q, bad, rdb, secret, base, 1)
	dBad.Start()
	defer dBad.Stop()

	// 第二次 panic 说明选主循环挺过了第一次:退避(测试参数下 1s)后又参选、又当选。
	waitFor(t, 3*time.Second, "panicking dispatcher runs for election again after backoff", func() bool {
		return bad.calls.Load() >= 2
	})
	// 退避期间锁键不在:panic 的那个任期已经放锁(不靠 TTL 过期,miniredis 也不按墙钟过期)。
	waitFor(t, 3*time.Second, "lock released by the panicked term", func() bool {
		return !mr.Exists(lockKey)
	})
	// panic 发生在 FreeSlots 里、PopAdmit 之前:条目一个没动。
	if n, err := q.QueueLen(ctx, 1); err != nil || n != 3 {
		t.Fatalf("queue len after panicked terms = %d err=%v, want 3", n, err)
	}

	good := &integrationProvider{
		caps:  map[uint32]uint32{1: 100},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}
	dGood := mustNewDispatcher(t, q, good, rdb, secret, base, 1)
	dGood.Start()
	defer dGood.Stop()

	// dBad 每轮只持锁一瞬(当选 → panic → 放锁)就退避 1s,dGood 每 1s 重试一次,几轮之内必然抢到。
	waitFor(t, 5*time.Second, "same-zone standby drained the queue", func() bool {
		n, err := q.QueueLen(ctx, 1)
		return err == nil && n == 0
	})
}

// leaderGaugeValue 从默认 registry 读 login_queue_dispatcher_is_leader 里标签恰为 {pod, zone} 的序列。
// Gather 出来的标签按名字排序,这里同时钉住标签集:多一个、少一个、名字或位置对不上都找不到。
func leaderGaugeValue(t *testing.T, pod, zone string) (float64, bool) {
	t.Helper()
	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather metrics: %v", err)
	}
	for _, mf := range families {
		if mf.GetName() != "login_queue_dispatcher_is_leader" {
			continue
		}
		for _, m := range mf.GetMetric() {
			labels := m.GetLabel()
			if len(labels) == 2 &&
				labels[0].GetName() == "pod" && labels[0].GetValue() == pod &&
				labels[1].GetName() == "zone" && labels[1].GetValue() == zone {
				return m.GetGauge().GetValue(), true
			}
		}
	}
	return 0, false
}

// G1b-spec-1 回归:leader 指标是电平,不是边沿。生产上 dispatcher 在 go-zero 指标开关打开之前就 Start
// 并当选(login.go 先 ctx.Start(),zrpc.MustNewServer 里的 SetUp → StartAgent 才打开开关),那时写的
// 样本被直接丢弃。leader 每拍重发 1,开关一开就补上。
// pod / zone 标签独立,不与包内其它用例的 {unknown, 1} 序列互相覆盖。开关是进程级的、打开后关不回去:
// 修复前只在全新进程里第一次跑时红(-run 单跑、-count=1),修复后恒绿。
func TestDispatcher_LeaderGaugeRepublishedAfterMetricsEnabled(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-gauge")
	q := New(rdb, time.Hour, 60*time.Second, secret)

	const base, zone = "dispatcher:lock:gauge", uint32(4242)
	// 队列一直是空的,provider 不会被碰到。
	d := mustNewDispatcher(t, q, &integrationProvider{}, rdb, secret, base, zone)
	d.SetPodID("gauge-test-pod")
	d.Start()
	defer d.Stop()

	waitFor(t, 3*time.Second, "zone 4242 leader lock acquired", func() bool {
		return mr.Exists(ZoneDispatcherLockKey(base, zone))
	})
	// 再等两拍(30ms 一拍):避开「锁键已写、goroutine 还没走到当选那次 setLeaderGauge(1)」的窗口,
	// 让当选那次写入确定落在开关打开之前。这一步只影响修复前能不能稳定红,不影响修复后的结果。
	time.Sleep(60 * time.Millisecond)

	gzprom.Enable()
	waitFor(t, 3*time.Second, `login_queue_dispatcher_is_leader{pod="gauge-test-pod",zone="4242"} == 1`, func() bool {
		v, ok := leaderGaugeValue(t, "gauge-test-pod", "4242")
		return ok && v == 1
	})
}
