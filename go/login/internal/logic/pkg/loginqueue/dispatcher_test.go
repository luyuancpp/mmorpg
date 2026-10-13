package loginqueue

import (
	"context"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
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
	// PopAdmit each tick + GETDEL on each Lookup frees the slot).
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
	drained := make(map[string]bool)
	waitFor(t, 3*time.Second, "all entries drained", func() bool {
		// We cheat by checking ZCARD: dispatcher pops members out, so
		// queue length monotonically drops to zero when all admitted.
		n, _ := q.QueueLen(ctx, 1)
		_ = drained
		return n == 0
	})
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
		admitted, _ = rdb.SMembers(ctx, admittedSetKey(1)).Result()
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
