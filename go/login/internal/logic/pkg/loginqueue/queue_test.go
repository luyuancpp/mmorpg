package loginqueue

import (
	"context"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
)

// newTestQueue spins up an in-process miniredis + go-redis client and a
// Queue with short TTLs. The returned cleanup function stops miniredis;
// callers should defer it to avoid cross-test goroutine leaks.
func newTestQueue(t *testing.T) (*Queue, *redis.Client, *miniredis.Miniredis, func()) {
	t.Helper()
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	q := New(rdb, time.Hour, 60*time.Second, []byte("test-secret"))
	return q, rdb, mr, func() { mr.Close(); _ = rdb.Close() }
}

// TestEnqueueAssignsAscendingRanks verifies the FIFO contract — first to
// ZADD must rank 0, second 1, third 2 — which is what the client UI ("you
// are #N in line") fundamentally relies on. A bug here is most likely to
// surface as ranks scrambled by the meta/token writes in the same
// pipeline; this test pins the contract.
func TestEnqueueAssignsAscendingRanks(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	res1, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue 1: %v", err)
	}
	if res1.Rank != 0 {
		t.Errorf("first enqueue rank = %d, want 0", res1.Rank)
	}

	// Force a different score by advancing miniredis's clock between
	// enqueues so the ZSET tie-break is well-defined.
	res2, err := q.Enqueue(ctx, 1, "bob", "dev2")
	if err != nil {
		t.Fatalf("enqueue 2: %v", err)
	}
	if res2.Rank != 1 {
		t.Errorf("second enqueue rank = %d, want 1", res2.Rank)
	}

	res3, err := q.Enqueue(ctx, 1, "carol", "dev3")
	if err != nil {
		t.Fatalf("enqueue 3: %v", err)
	}
	if res3.Rank != 2 {
		t.Errorf("third enqueue rank = %d, want 2", res3.Rank)
	}
	if res3.Total != 3 {
		t.Errorf("total = %d, want 3", res3.Total)
	}
	if res1.QueueToken == res2.QueueToken || res2.QueueToken == res3.QueueToken {
		t.Error("queue tokens must be distinct across enqueues")
	}
}

// TestZonesAreIndependent guards against accidentally mixing zones into the
// same ZSET — a tempting refactor that would break per-zone capacity logic.
func TestZonesAreIndependent(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	if _, err := q.Enqueue(ctx, 1, "alice", "dev1"); err != nil {
		t.Fatalf("enqueue zone 1: %v", err)
	}
	res, err := q.Enqueue(ctx, 2, "bob", "dev2")
	if err != nil {
		t.Fatalf("enqueue zone 2: %v", err)
	}
	if res.Rank != 0 {
		t.Errorf("zone 2 first enqueue rank = %d, want 0 (zones must be independent)", res.Rank)
	}

	zone1Len, _ := q.QueueLen(ctx, 1)
	zone2Len, _ := q.QueueLen(ctx, 2)
	if zone1Len != 1 || zone2Len != 1 {
		t.Errorf("zone lengths = (%d, %d), want (1, 1)", zone1Len, zone2Len)
	}
}

// TestPopAdmitWritesAdmitToken exercises the dispatcher path. We pop one
// entry, supply a stub pickFn, and verify Lookup transitions through
// QUEUEING → ADMITTED → EXPIRED on subsequent polls. This pins the
// "exactly-once admit" contract that prevents double-issued gate tokens.
//
// Post-R17-R1 (2026-06-04): the dispatcher only PICKS a gate at admit
// time (writing an AdmitSlot to admit:{queueId}); the gate token's HMAC
// + expire are computed inside Lookup via signFn. The test asserts both
// halves work — pickFn writes the slot, signFn turns it into a token.
func TestPopAdmitWritesAdmitToken(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}

	stubSign := func(slot *AdmitSlot) (*AdmitToken, error) {
		return &AdmitToken{
			IP:            slot.IP,
			Port:          slot.Port,
			TokenDeadline: time.Now().Unix() + 300,
		}, nil
	}

	// Before PopAdmit: status must be QUEUEING.
	state, err := q.Lookup(ctx, res.QueueToken, stubSign)
	if err != nil {
		t.Fatalf("lookup before admit: %v", err)
	}
	if state.Status != StatusQueueing {
		t.Errorf("status before admit = %d, want %d", state.Status, StatusQueueing)
	}

	pickFn := func(zoneID uint32, queueID string) (*AdmitSlot, error) {
		return &AdmitSlot{NodeID: 1, IP: "10.0.0.5", Port: 9000, ZoneID: zoneID}, nil
	}
	n, err := q.PopAdmit(ctx, 1, 5, pickFn)
	if err != nil {
		t.Fatalf("popadmit: %v", err)
	}
	if n != 1 {
		t.Errorf("popadmit count = %d, want 1", n)
	}

	// First Lookup after admit: ADMITTED with admit token signed by stubSign
	// at consume time (the slot we wrote at admit time has IP=10.0.0.5).
	state, err = q.Lookup(ctx, res.QueueToken, stubSign)
	if err != nil {
		t.Fatalf("lookup after admit: %v", err)
	}
	if state.Status != StatusAdmitted {
		t.Fatalf("status after admit = %d, want %d", state.Status, StatusAdmitted)
	}
	if state.Admit == nil || state.Admit.IP != "10.0.0.5" || state.Admit.Port != 9000 {
		t.Errorf("admit token mismatch: got %+v", state.Admit)
	}

	// Second Lookup: admit was consumed → EXPIRED-equivalent (still
	// QUEUEING with rank 0 in our impl because the meta key is alive but
	// the ZSET member is gone). Either way, NOT a second ADMITTED.
	state, err = q.Lookup(ctx, res.QueueToken, stubSign)
	if err != nil {
		t.Fatalf("lookup after consume: %v", err)
	}
	if state.Status == StatusAdmitted {
		t.Errorf("admit was consumed twice — leaks gate tokens")
	}
}

// TestQueueTokenRoundTrip ensures HMAC sign+verify is symmetric and
// rejects tampering. Without this, a forged queue_token would let a
// client jump the queue.
func TestQueueTokenRoundTrip(t *testing.T) {
	secret := []byte("test-secret")
	token, err := signQueueToken(secret, "qid-123", 7, time.Now().Add(time.Hour).Unix())
	if err != nil {
		t.Fatalf("sign: %v", err)
	}
	qid, zone, err := ParseAndVerifyQueueToken(secret, token)
	if err != nil {
		t.Fatalf("verify: %v", err)
	}
	if qid != "qid-123" || zone != 7 {
		t.Errorf("parse mismatch: qid=%s zone=%d", qid, zone)
	}

	// Tampered token: flip a byte.
	bad := []byte(token)
	bad[0] ^= 0x01
	if _, _, err := ParseAndVerifyQueueToken(secret, string(bad)); err == nil {
		t.Error("verify accepted tampered token")
	}

	// Wrong secret.
	if _, _, err := ParseAndVerifyQueueToken([]byte("other-secret"), token); err == nil {
		t.Error("verify accepted token signed with different secret")
	}

	// Expired.
	old, _ := signQueueToken(secret, "qid-x", 1, time.Now().Add(-time.Second).Unix())
	if _, _, err := ParseAndVerifyQueueToken(secret, old); err == nil {
		t.Error("verify accepted expired token")
	}
}

// fakeProvider is a CapacityProvider stub for FreeSlots tests. It returns a
// fixed list of GateCandidates and an optional config-pinned capacity.
type fakeProvider struct {
	cands map[uint32][]GateCandidate
	caps  map[uint32]uint32
}

func (f *fakeProvider) CandidatesForZone(_ context.Context, zoneID uint32) ([]GateCandidate, error) {
	return f.cands[zoneID], nil
}
func (f *fakeProvider) ZoneCapacity(zoneID uint32) uint32 { return f.caps[zoneID] }

// TestFreeSlotsAccountsForAdmittedInFlight is the core safety property:
// admitted-but-not-yet-connected entries must subtract from freeSlots, or
// the dispatcher over-issues gate tokens during the round-trip window.
func TestFreeSlotsAccountsForAdmittedInFlight(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	prov := &fakeProvider{
		cands: map[uint32][]GateCandidate{
			1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, PlayerCount: 50, ZoneID: 1}},
		},
		caps: map[uint32]uint32{1: 100},
	}

	// Baseline: capacity 100, online 50, admitted 0 → free=50.
	free, capacity, online, admitted, err := FreeSlots(ctx, prov, q, 1, 1.5)
	if err != nil {
		t.Fatalf("freeslots: %v", err)
	}
	if free != 50 || capacity != 100 || online != 50 || admitted != 0 {
		t.Errorf("baseline mismatch: free=%d cap=%d online=%d admitted=%d", free, capacity, online, admitted)
	}

	// Enqueue + admit one entry. admitted should now be 1, free should
	// drop by exactly 1.
	res, _ := q.Enqueue(ctx, 1, "alice", "dev1")
	if _, err := q.PopAdmit(ctx, 1, 1, func(z uint32, qid string) (*AdmitSlot, error) {
		return &AdmitSlot{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: z}, nil
	}); err != nil {
		t.Fatalf("popadmit: %v", err)
	}
	_ = res

	free, _, _, admitted, err = FreeSlots(ctx, prov, q, 1, 1.5)
	if err != nil {
		t.Fatalf("freeslots after admit: %v", err)
	}
	if admitted != 1 {
		t.Errorf("admitted = %d, want 1", admitted)
	}
	if free != 49 {
		t.Errorf("free after admit = %d, want 49", free)
	}
}

// TestFreeSlotsSoftCapFallback verifies the multiplier formula kicks in
// when no ZoneCapacity is configured. Without this, freshly-deployed
// zones (online=0, capacity=0) would lock everyone out.
func TestFreeSlotsSoftCapFallback(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	prov := &fakeProvider{
		cands: map[uint32][]GateCandidate{
			1: {{NodeID: 1, IP: "x", Port: 1, PlayerCount: 100, ZoneID: 1}},
		},
		caps: map[uint32]uint32{}, // unconfigured
	}

	free, capacity, _, _, err := FreeSlots(ctx, prov, q, 1, 1.5)
	if err != nil {
		t.Fatalf("freeslots: %v", err)
	}
	// online=100 * 1.5 = 150 cap; 150 - 100 = 50 free.
	if capacity != 150 {
		t.Errorf("cap = %d, want 150 (online 100 * 1.5)", capacity)
	}
	if free != 50 {
		t.Errorf("free = %d, want 50", free)
	}
}

// TestFreeSlotsErrorsWhenNoGates ensures we DON'T silently enqueue into a
// zone with zero gates — the entry would never drain.
func TestFreeSlotsErrorsWhenNoGates(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	prov := &fakeProvider{
		cands: map[uint32][]GateCandidate{}, // zone 1 has nothing
	}
	_, _, _, _, err := FreeSlots(ctx, prov, q, 1, 1.5)
	if err == nil {
		t.Error("freeslots returned nil err for zone with zero gates — would create a black hole")
	}
}

// TestPopAdmitReQueuesOnSignFailure: pickFn returning err must not lose
// the queue entry. We re-add at the front (score=0) so the next dispatcher
// tick retries with hopefully-recovered state.
func TestPopAdmitReQueuesOnSignFailure(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	res, _ := q.Enqueue(ctx, 1, "alice", "dev1")

	pickFnFail := func(z uint32, qid string) (*AdmitSlot, error) {
		return nil, context.DeadlineExceeded
	}
	n, err := q.PopAdmit(ctx, 1, 1, pickFnFail)
	if err != nil {
		t.Fatalf("popadmit: %v", err)
	}
	if n != 0 {
		t.Errorf("admitted count on fail = %d, want 0", n)
	}

	// Entry should still be queryable via Lookup. signFn would never be
	// invoked here because no admit slot exists yet — pass a stub that
	// loudly fails the test if it somehow runs.
	stubSign := func(slot *AdmitSlot) (*AdmitToken, error) {
		t.Fatal("signFn called for an entry that never got an admit slot")
		return nil, nil
	}
	state, err := q.Lookup(ctx, res.QueueToken, stubSign)
	if err != nil {
		t.Fatalf("lookup after failed admit: %v", err)
	}
	if state.Status != StatusQueueing {
		t.Errorf("status after failed admit = %d, want StatusQueueing — entry was lost", state.Status)
	}
}

// ── G4b-G:在途占位逐个到期 ─────────────────────────────────────────────

// legacyAdmittedSetKeyZone1 是 G4b-G 之前的在途占位键(SET)。新实现不再读写它,
// 用例拿它断言「旧键没人写」。新旧版本混跑时两种键并存、互不可见。
const legacyAdmittedSetKeyZone1 = "admitted:zone:1"

// fakeClock 是在途占位记账用的可拨时钟。占位的到期分数与清理都读 Queue.now,
// 用例直接拨它:不 sleep,也不依赖 miniredis 的时间(miniredis 里键的 TTL 不随它走)。
type fakeClock struct{ ms atomic.Int64 }

func newFakeClock() *fakeClock {
	c := &fakeClock{}
	c.ms.Store(1_800_000_000_000)
	return c
}

func (c *fakeClock) Now() time.Time          { return time.UnixMilli(c.ms.Load()) }
func (c *fakeClock) Advance(d time.Duration) { c.ms.Add(d.Milliseconds()) }

// newClockedQueue 在 newTestQueue 的基础上把占位时钟换成假时钟(起点固定,结果可复现)。
func newClockedQueue(t *testing.T) (*Queue, *redis.Client, *fakeClock) {
	t.Helper()
	q, rdb, _, cleanup := newTestQueue(t)
	t.Cleanup(cleanup)
	clk := newFakeClock()
	q.now = clk.Now
	return q, rdb, clk
}

// oneGateProvider:zone 1 一台 gate、固定容量。在线数由用例直接改 cands[1][0].PlayerCount
// (fakeProvider 返回的是同一个切片,不是副本)。
func oneGateProvider(capacity, online uint32) *fakeProvider {
	return &fakeProvider{
		cands: map[uint32][]GateCandidate{
			1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, PlayerCount: online, ZoneID: 1}},
		},
		caps: map[uint32]uint32{1: capacity},
	}
}

func pickZone1Gate(zoneID uint32, _ string) (*AdmitSlot, error) {
	return &AdmitSlot{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: zoneID}, nil
}

func mustAdmittedCount(t *testing.T, q *Queue, want uint32, when string) {
	t.Helper()
	got, err := q.AdmittedCount(context.Background(), 1)
	if err != nil {
		t.Fatalf("admitted count (%s): %v", when, err)
	}
	if got != want {
		t.Fatalf("admitted count (%s) = %d, want %d", when, got, want)
	}
}

func mustFree(t *testing.T, prov CapacityProvider, q *Queue, want uint32, when string) {
	t.Helper()
	free, capacity, online, admitted, err := FreeSlots(context.Background(), prov, q, 1, 1.5)
	if err != nil {
		t.Fatalf("freeslots (%s): %v", when, err)
	}
	if free != want {
		t.Fatalf("free (%s) = %d, want %d (cap=%d online=%d admitted=%d)",
			when, free, want, capacity, online, admitted)
	}
}

func mustLegacyKeyUntouched(t *testing.T, rdb *redis.Client) {
	t.Helper()
	n, err := rdb.Exists(context.Background(), legacyAdmittedSetKeyZone1).Result()
	if err != nil || n != 0 {
		t.Fatalf("legacy key %q must not be written any more: exists=%d err=%v", legacyAdmittedSetKeyZone1, n, err)
	}
}

// 15s 与 gate 的上报周期(10s)配套,见 defaultHandoffGrace 的注释;改它要同步改设计稿与 runbook。
func TestNew_DefaultsHandoffGraceAndClock(t *testing.T) {
	q := New(nil, time.Hour, 60*time.Second, nil)
	if q.handoffGrace != 15*time.Second {
		t.Fatalf("handoffGrace = %v, want 15s", q.handoffGrace)
	}
	if q.now == nil {
		t.Fatal("Queue.now must default to time.Now")
	}
	if got, want := q.admittedKeyTTL(), 2*time.Minute; got != want {
		t.Fatalf("admittedKeyTTL = %v, want %v (max(AdmitTTL, handoffGrace) + 1min)", got, want)
	}
}

// 放行写的是新键 ZSET,分数 = 到期毫秒;没人来取时按自己的分数到期,不靠整键 TTL。
func TestPopAdmit_HoldsPlaceholderInZSetUntilAdmitTTL(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	admittedAt := clk.Now()
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}

	key := admittedZSetKey(1)
	if key != "admitted:zset:zone:1" {
		t.Fatalf("admitted key = %q, want admitted:zset:zone:1", key)
	}
	if typ, err := rdb.Type(ctx, key).Result(); err != nil || typ != "zset" {
		t.Fatalf("type(%s) = %q err=%v, want zset", key, typ, err)
	}
	score, err := rdb.ZScore(ctx, key, res.QueueID).Result()
	if err != nil {
		t.Fatalf("zscore: %v", err)
	}
	if want := float64(admittedAt.Add(q.admitTTL).UnixMilli()); score != want {
		t.Fatalf("placeholder score = %.0f, want %.0f (admit time + AdmitTTL, unix ms)", score, want)
	}
	if ttl, err := rdb.PTTL(ctx, key).Result(); err != nil || ttl <= q.admitTTL || ttl > q.admittedKeyTTL() {
		t.Fatalf("whole-key PTTL = %v err=%v, want (%v, %v]", ttl, err, q.admitTTL, q.admittedKeyTTL())
	}

	clk.Advance(q.admitTTL - time.Millisecond)
	mustAdmittedCount(t, q, 1, "1ms before AdmitTTL")
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "at AdmitTTL, never picked up")
	mustLegacyKeyUntouched(t, rdb)
}

func TestTryReserveFastPathSlot_BudgetAndPerMemberExpiry(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	reserve := func(budget uint32) bool {
		t.Helper()
		ok, err := q.TryReserveFastPathSlot(ctx, 1, budget)
		if err != nil {
			t.Fatalf("reserve(budget=%d): %v", budget, err)
		}
		return ok
	}

	if !reserve(2) || !reserve(2) {
		t.Fatal("first two reservations must fit budget 2")
	}
	if reserve(2) {
		t.Fatal("third reservation must be refused: budget 2 is full")
	}
	if reserve(0) {
		t.Fatal("budget 0 must never reserve")
	}

	wantScore := float64(clk.Now().Add(q.handoffGrace).UnixMilli())
	members, err := rdb.ZRangeWithScores(ctx, admittedZSetKey(1), 0, -1).Result()
	if err != nil || len(members) != 2 {
		t.Fatalf("placeholders = %v err=%v, want 2 members", members, err)
	}
	for _, m := range members {
		name, _ := m.Member.(string)
		if !strings.HasPrefix(name, "fast:") || m.Score != wantScore {
			t.Fatalf("placeholder %q score %.0f, want fast:<uuid> with score %.0f (now + handoff grace)",
				name, m.Score, wantScore)
		}
	}
	// 快速通道脚本给整键续的垃圾回收 TTL(ARGV[5])。miniredis 的 TTL 不随墙钟走、只在 FastForward 里递减,
	// 单位传错(比如 .Seconds() 变成 PEXPIRE 120ms)或者漏掉这条 PEXPIRE,只有这里能发现;
	// 到了真实 Redis 上,整键会在 120ms 后连同未到期的占位一起消失(或永不过期)。
	if ttl, err := rdb.PTTL(ctx, admittedZSetKey(1)).Result(); err != nil || ttl <= q.admitTTL || ttl > q.admittedKeyTTL() {
		t.Fatalf("fast-path whole-key PTTL = %v err=%v, want (%v, %v]", ttl, err, q.admitTTL, q.admittedKeyTTL())
	}

	clk.Advance(q.handoffGrace - time.Millisecond)
	if reserve(2) {
		t.Fatal("placeholders are still live 1ms before they expire")
	}
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "handoff grace elapsed")
	if !reserve(2) {
		t.Fatal("expired fast-path placeholders must free their slots")
	}
	mustLegacyKeyUntouched(t, rdb)
}

// Q1 回归:每秒 1 人走快速通道,10 秒后(gate 的上报周期)出现在 PlayerCount 里,连续 60 秒。
// 旧实现的 fast:<uuid> 占位永远不单独回收,整集合 TTL 又被每次占位续上:
// 连上 gate 的人同时算进 online 和 admitted,第 55 秒余量归零。
func TestFastPath_SustainedLoginsDoNotStarveBudget(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	const capacity = 100
	prov := oneGateProvider(capacity, 0)

	for sec := 0; sec < 60; sec++ {
		online := uint32(0)
		if sec > 10 {
			online = uint32(sec - 10)
		}
		prov.cands[1][0].PlayerCount = online

		free, _, _, admitted, err := FreeSlots(ctx, prov, q, 1, 1.5)
		if err != nil {
			t.Fatalf("sec=%d freeslots: %v", sec, err)
		}
		if free == 0 {
			t.Fatalf("sec=%d: free hit 0 with %d online of %d (admitted=%d): connected players are double-counted",
				sec, online, capacity, admitted)
		}
		ok, err := q.TryReserveFastPathSlot(ctx, 1, capacity-online)
		if err != nil || !ok {
			t.Fatalf("sec=%d: fast-path reservation refused (ok=%v err=%v) with %d online of %d",
				sec, ok, err, online, capacity)
		}
		clk.Advance(time.Second)
	}

	// 稳态下占位数 = 最近 handoffGrace 内交出去的票数,不随登录总数增长。
	n, err := q.AdmittedCount(ctx, 1)
	if err != nil {
		t.Fatalf("admitted count: %v", err)
	}
	if limit := uint32(q.handoffGrace / time.Second); n > limit {
		t.Fatalf("placeholders = %d after 60 logins, want <= %d (one per second of handoff grace)", n, limit)
	}
	mustLegacyKeyUntouched(t, rdb)
}

// Q2 回归:取走放行槽之后,gate 要到下一次上报才把人数算进 PlayerCount。
// 这段时间占位必须留着;旧实现取走即 SREM,下一拍按「在线 0、在途 0」再放一满批。
func TestConsume_KeepsPlaceholderForHandoffGraceThenReleases(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	prov := oneGateProvider(2, 0)

	queueIDs := make(map[string]string) // token → queueId
	for range 6 {
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		queueIDs[res.QueueToken] = res.QueueID
	}
	mustFree(t, prov, q, 2, "before any admit")
	if n, err := q.PopAdmit(ctx, 1, 2, pickZone1Gate); err != nil || n != 2 {
		t.Fatalf("popadmit = (%d, %v), want (2, nil)", n, err)
	}

	clk.Advance(2 * time.Second) // 客户端的下一次轮询
	consumedAt := clk.Now()
	var taken []string
	for token, queueID := range queueIDs {
		st, err := q.Lookup(ctx, token, stubSignFn)
		if err != nil {
			t.Fatalf("lookup: %v", err)
		}
		if st.Status == StatusAdmitted {
			taken = append(taken, queueID)
		}
	}
	if len(taken) != 2 {
		t.Fatalf("consumed %d admits, want 2", len(taken))
	}
	wantScore := float64(consumedAt.Add(q.handoffGrace).UnixMilli())
	for _, queueID := range taken {
		score, err := rdb.ZScore(ctx, admittedZSetKey(1), queueID).Result()
		if err != nil || score != wantScore {
			t.Fatalf("placeholder %s score = %.0f err=%v, want %.0f (consume time + handoff grace)",
				queueID, score, err, wantScore)
		}
	}

	mustFree(t, prov, q, 0, "right after consume, gate still reports 0 online")
	clk.Advance(q.handoffGrace - time.Millisecond)
	mustFree(t, prov, q, 0, "1ms before the handoff grace ends")
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "handoff grace elapsed")
	mustFree(t, prov, q, 2, "handoff grace elapsed, players never showed up")
}

// login 的钟比 Redis 快时,占位(login 钟)可能先于放行槽(Redis TTL)到期被清掉。
// 这时取走仍要给这个人占 15s:他确实马上要连 gate。所以是普通 ZADD,不是 XX。
func TestConsume_HoldsPlaceholderEvenIfAlreadyExpired(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	clk.Advance(q.admitTTL) // miniredis 里放行槽的 TTL 不随假时钟走,槽还在
	mustAdmittedCount(t, q, 0, "placeholder expired by score")

	st, err := q.Lookup(ctx, res.QueueToken, stubSignFn)
	if err != nil || st.Status != StatusAdmitted {
		t.Fatalf("lookup = (%+v, %v), want ADMITTED", st, err)
	}
	mustAdmittedCount(t, q, 1, "consume re-holds the slot")
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if want := float64(clk.Now().Add(q.handoffGrace).UnixMilli()); err != nil || score != want {
		t.Fatalf("placeholder score = %.0f err=%v, want %.0f", score, err, want)
	}
}

// 清理是闭区间:分数 <= now 的成员算已到期。
func TestAdmittedCount_DropsOnlyExpiredMembers(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()
	mustAdmittedCount(t, q, 0, "key does not exist")

	now := clk.Now().UnixMilli()
	key := admittedZSetKey(1)
	if err := rdb.ZAdd(ctx, key,
		redis.Z{Score: float64(now - 1), Member: "expired"},
		redis.Z{Score: float64(now), Member: "expires-now"},
		redis.Z{Score: float64(now + 1), Member: "live"},
	).Err(); err != nil {
		t.Fatalf("seed: %v", err)
	}
	mustAdmittedCount(t, q, 1, "only the member with score > now is live")
	left, err := rdb.ZRange(ctx, key, 0, -1).Result()
	if err != nil || len(left) != 1 || left[0] != "live" {
		t.Fatalf("members after count = %v err=%v, want [live]", left, err)
	}
}

// 清理、判定、占位在一段 Lua 里:并发调用成功的个数恰好等于 budget。
func TestTryReserveFastPathSlot_ConcurrentNeverExceedsBudget(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()
	const budget, callers = 8, 64

	var granted atomic.Int32
	var wg sync.WaitGroup
	errs := make(chan error, callers)
	for range callers {
		wg.Add(1)
		go func() {
			defer wg.Done()
			ok, err := q.TryReserveFastPathSlot(ctx, 1, budget)
			if err != nil {
				errs <- err
				return
			}
			if ok {
				granted.Add(1)
			}
		}()
	}
	wg.Wait()
	close(errs)
	for err := range errs {
		t.Fatalf("reserve: %v", err)
	}
	if got := granted.Load(); got != budget {
		t.Fatalf("granted %d reservations, want exactly %d", got, budget)
	}
	if n, err := rdb.ZCard(ctx, admittedZSetKey(1)).Result(); err != nil || n != budget {
		t.Fatalf("placeholders = %d err=%v, want %d", n, err, budget)
	}
}

// 新版本只读写新键。旧键 admitted:zone:{z} 靠旧版本最后一次写入后的 TTL 自己消失。
func TestLegacyAdmittedSetKey_NeverWritten(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()
	prov := oneGateProvider(10, 0)

	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 10); err != nil || !ok {
		t.Fatalf("reserve = (%v, %v), want (true, nil)", ok, err)
	}
	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	mustFree(t, prov, q, 9, "one fast-path placeholder")
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	if st, err := q.Lookup(ctx, res.QueueToken, stubSignFn); err != nil || st.Status != StatusAdmitted {
		t.Fatalf("lookup = (%+v, %v), want ADMITTED", st, err)
	}
	mustAdmittedCount(t, q, 2, "fast-path placeholder + consumed admit")

	keys, err := rdb.Keys(ctx, "admitted:*").Result()
	if err != nil || len(keys) != 1 || keys[0] != "admitted:zset:zone:1" {
		t.Fatalf("keys admitted:* = %v err=%v, want only admitted:zset:zone:1", keys, err)
	}
}

// Redis 出错时的方向不变:读不出在途占位就不放行。
func TestAdmittedAccounting_RedisDown_FailsClosed(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	// MaxRetries -1 关命令重试;DialerRetries 1 关拨号重试(缺省拨 5 次、每次失败后固定等 100ms)。
	// 下面三条会走到 Redis 的调用各拨号一次,每次最多等一个 DialTimeout。
	rdb := redis.NewClient(&redis.Options{
		Addr:               mr.Addr(),
		MaxRetries:         -1,
		DialTimeout:        200 * time.Millisecond,
		DialerRetries:      1,
		DialerRetryTimeout: time.Millisecond,
	})
	defer rdb.Close()
	q := New(rdb, time.Hour, 60*time.Second, []byte("test-secret"))
	ctx := context.Background()
	prov := oneGateProvider(10, 0)
	mr.Close()

	if _, err := q.AdmittedCount(ctx, 1); err == nil {
		t.Fatal("AdmittedCount must surface the Redis error")
	}
	if free, _, _, _, err := FreeSlots(ctx, prov, q, 1, 1.5); err == nil || free != 0 {
		t.Fatalf("FreeSlots = (free=%d, err=%v), want (0, error): no accounting, no admission", free, err)
	}
	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 5); err == nil || ok {
		t.Fatalf("TryReserveFastPathSlot = (%v, %v), want (false, error)", ok, err)
	}
	// budget 0 不碰 Redis:Redis 挂着也不报错。
	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 0); err != nil || ok {
		t.Fatalf("TryReserveFastPathSlot(budget=0) = (%v, %v), want (false, nil)", ok, err)
	}
}

// 取走时占位记账失败不让这次取走作废(票已签好);但之后读不出记账,容量判定一律不放行。
func TestConsume_PlaceholderWriteFailureDoesNotFailAdmit(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	// 把占位键换成 STRING:取走时那次 ZADD 必然 WRONGTYPE。
	key := admittedZSetKey(1)
	if err := rdb.Del(ctx, key).Err(); err != nil {
		t.Fatalf("del: %v", err)
	}
	if err := rdb.Set(ctx, key, "not-a-zset", 0).Err(); err != nil {
		t.Fatalf("set: %v", err)
	}

	st, err := q.Lookup(ctx, res.QueueToken, stubSignFn)
	if err != nil || st.Status != StatusAdmitted || st.Admit == nil {
		t.Fatalf("lookup = (%+v, %v), want ADMITTED despite the accounting failure", st, err)
	}
	if _, err := q.AdmittedCount(ctx, 1); err == nil {
		t.Fatal("AdmittedCount must fail on a wrong-type key")
	}
	if ok, err := q.TryReserveFastPathSlot(ctx, 1, 5); err == nil || ok {
		t.Fatalf("TryReserveFastPathSlot = (%v, %v), want (false, error)", ok, err)
	}
}

// 取走只发生一次。取走之后客户端按协议还会每 2s 轮询(回「排队中、名次 0」,直到条目 1h 过期),
// 这些轮询不能给占位续期:否则每次续 15s,占位永不到期,名额一直被扣着。
func TestConsume_RepeatPollDoesNotExtendPlaceholder(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	if st, err := q.Lookup(ctx, res.QueueToken, stubSignFn); err != nil || st.Status != StatusAdmitted {
		t.Fatalf("first lookup = (%+v, %v), want ADMITTED", st, err)
	}
	want := float64(clk.Now().Add(q.handoffGrace).UnixMilli())

	clk.Advance(5 * time.Second)
	st, err := q.Lookup(ctx, res.QueueToken, stubSignFn)
	if err != nil {
		t.Fatalf("repeat lookup: %v", err)
	}
	if st.Status == StatusAdmitted {
		t.Fatal("admit was consumed twice")
	}
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if err != nil || score != want {
		t.Fatalf("placeholder score after a repeat poll = %.0f err=%v, want %.0f (first consume + handoff grace, not extended)",
			score, err, want)
	}
	mustAdmittedCount(t, q, 1, "5s after consume")

	clk.Advance(q.handoffGrace - 5*time.Second)
	mustAdmittedCount(t, q, 0, "handoff grace elapsed since the first consume")
}

// signFn 失败:放行槽已被 GETDEL 删掉,这个人拿不到票;占位不改分数,保持「放行时刻 + AdmitTTL」,到点自己消失。
func TestConsume_SignFailureKeepsAdmitTimeScore(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	admittedAt := clk.Now()
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	want := float64(admittedAt.Add(q.admitTTL).UnixMilli())

	clk.Advance(2 * time.Second) // 客户端的下一次轮询
	failSign := func(*AdmitSlot) (*AdmitToken, error) { return nil, context.DeadlineExceeded }
	if st, err := q.Lookup(ctx, res.QueueToken, failSign); err == nil {
		t.Fatalf("lookup = (%+v, nil), want the signing error", st)
	}
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if err != nil || score != want {
		t.Fatalf("placeholder score after a failed sign = %.0f err=%v, want %.0f (admit time + AdmitTTL, untouched)",
			score, err, want)
	}
	// 放行槽已经没了:再来轮询也拿不到票。
	if st, err := q.Lookup(ctx, res.QueueToken, stubSignFn); err != nil || st.Status == StatusAdmitted {
		t.Fatalf("lookup after a failed sign = (%+v, %v), want not ADMITTED and no error", st, err)
	}

	clk.Advance(q.admitTTL - 2*time.Second - time.Millisecond)
	mustAdmittedCount(t, q, 1, "1ms before AdmitTTL")
	clk.Advance(time.Millisecond)
	mustAdmittedCount(t, q, 0, "at AdmitTTL after a failed sign")
}
