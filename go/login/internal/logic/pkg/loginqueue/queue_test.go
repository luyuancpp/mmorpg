package loginqueue

import (
	"context"
	"errors"
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

	// Second Lookup: admit was consumed → EXPIRED, NOT a second ADMITTED.
	// (2026-10-15 更正)此前这里回的是 QUEUEING、名次 0(meta 还在、ZSET 成员已不在),本用例只断言
	// 「不是第二次 ADMITTED」;现在 meta 里的 admitted_at 让 Lookup 回终态 EXPIRED,断言随之收紧。
	state, err = q.Lookup(ctx, res.QueueToken, stubSign)
	if err != nil {
		t.Fatalf("lookup after consume: %v", err)
	}
	if state.Status == StatusAdmitted {
		t.Errorf("admit was consumed twice — leaks gate tokens")
	} else if state.Status != StatusExpired {
		t.Errorf("status after consume = %d, want %d (EXPIRED)", state.Status, StatusExpired)
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

// 20s 是 gate 上报周期(10s)的两倍,见 defaultHandoffGrace 的注释;改它要同步改设计稿与 runbook。
func TestNew_DefaultsHandoffGraceAndClock(t *testing.T) {
	q := New(nil, time.Hour, 60*time.Second, nil)
	if q.handoffGrace != 20*time.Second {
		t.Fatalf("handoffGrace = %v, want 20s", q.handoffGrace)
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
	// 单位传错(比如 .Seconds() 变成 PEXPIRE 120ms)或者漏掉这条 PEXPIRE,要靠这里(以及
	// TestAdmittedKey_EveryWritePathSetsTTLWhenMissing)发现;
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
// 这时取走仍要给这个人占一个交票后宽限:他确实马上要连 gate。所以是普通 ZADD,不是 XX。
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

// 取走只发生一次。取走之后同一令牌还可能再查(回包丢失后的重试、go-redis 超时重发),这些查询不能给占位续期:
// 否则每次续一个交票后宽限,占位永不到期,名额一直被扣着。
// (2026-10-15 更正)此前这里写的是「客户端按协议还会每 2s 轮询(回『排队中、名次 0』,直到条目 1h 过期)」;
// 现在再查回 EXPIRED(见 TestLookup_PollAfterConsume_ReturnsExpired),客户端随即丢掉令牌,但单次再查同样不能续期。
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
	if st.Status != StatusExpired {
		t.Fatalf("repeat lookup status = %d, want %d (EXPIRED since 2026-10-15)", st.Status, StatusExpired)
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

// 整键 TTL 只增不减(spec_G4b-G §11 #6):排队开着时滚动调整 AdmitTTL,新旧两种 W 的副本交替写同一个 zone。
// AdmitTTL 小的副本后写,不能把 AdmitTTL 大的副本续上的整键 TTL 缩短:否则整键先于大值副本写的放行占位
// 消失,下一拍按在途 0 多放一批。三条写路径(快速通道、放行、取走)逐一钉住。
// 反过来,剩余 TTL 不足自己的 W 时照常续上(只增不减不等于「有 TTL 就不动」),三条路径也逐一钉住。
// miniredis 的 TTL 不随墙钟走、只在 FastForward 里递减,两次写之间 PTTL 不会自己变,所以断言写成等式;
// 占位分数走假时钟,FastForward 不影响它们。
// 无条件 PEXPIRE W 的写法在「smaller window: fast path」处红:PTTL 从 6min 被压到 2min。
func TestAdmittedKeyTTL_SmallerWindowNeverShortensIt(t *testing.T) {
	small, rdb, mr, cleanup := newTestQueue(t) // AdmitTTL 60s
	defer cleanup()
	big := New(rdb, time.Hour, 5*time.Minute, []byte("test-secret"))
	clk := newFakeClock()
	small.now, big.now = clk.Now, clk.Now
	ctx := context.Background()

	smallW, bigW := small.admittedKeyTTL(), big.admittedKeyTTL()
	if smallW != 2*time.Minute || bigW != 6*time.Minute {
		t.Fatalf("windows = (%v, %v), want (2m0s, 6m0s): max(AdmitTTL, handoffGrace) + 1min", smallW, bigW)
	}
	key := admittedZSetKey(1)
	mustPTTL := func(want time.Duration, when string) {
		t.Helper()
		got, err := rdb.PTTL(ctx, key).Result()
		if err != nil || got != want {
			t.Fatalf("whole-key PTTL (%s) = %v err=%v, want %v", when, got, err, want)
		}
	}
	enqueue := func(q *Queue) string {
		t.Helper()
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		return res.QueueToken
	}
	admit := func(q *Queue, n int) {
		t.Helper()
		if got, err := q.PopAdmit(ctx, 1, n, pickZone1Gate); err != nil || got != n {
			t.Fatalf("popadmit = (%d, %v), want (%d, nil)", got, err, n)
		}
	}
	consume := func(q *Queue, token string) {
		t.Helper()
		if st, err := q.Lookup(ctx, token, stubSignFn); err != nil || st.Status != StatusAdmitted {
			t.Fatalf("lookup = (%+v, %v), want ADMITTED", st, err)
		}
	}
	reserve := func(q *Queue) {
		t.Helper()
		if ok, err := q.TryReserveFastPathSlot(ctx, 1, 100); err != nil || !ok {
			t.Fatalf("reserve = (%v, %v), want (true, nil)", ok, err)
		}
	}

	// AdmitTTL 大的副本先放行两人。tokB 没人来取,它的占位要活到放行后 5min,整键 TTL 必须盖得住它。
	tokA, tokB := enqueue(big), enqueue(big)
	admit(big, 2)
	mustPTTL(bigW, "larger window admits first")

	// AdmitTTL 小的副本随后走三条写路径,每一条都不能把整键 TTL 缩到 smallW。
	reserve(small)
	mustPTTL(bigW, "smaller window: fast path")
	enqueue(small)
	admit(small, 1)
	mustPTTL(bigW, "smaller window: admit")
	consume(small, tokA) // 取走的是大值副本放行的条目
	mustPTTL(bigW, "smaller window: consume")

	// 拨到整键只剩 smallW − 1s:三条路径各写一次,都要续回 smallW。每段之前再拨 1s。
	// FastForward 同时让小值副本 60s 的放行槽过期;tokB 的放行槽是大值副本写的(5min),还在。
	mr.FastForward(bigW - smallW + time.Second)
	mustPTTL(smallW-time.Second, "fast-forwarded below the smaller window")
	reserve(small)
	mustPTTL(smallW, "renewed by the fast path")
	mr.FastForward(time.Second)
	mustPTTL(smallW-time.Second, "1s after the fast-path renewal")
	enqueue(small)
	admit(small, 1)
	mustPTTL(smallW, "renewed by admit")
	mr.FastForward(time.Second)
	mustPTTL(smallW-time.Second, "1s after the admit renewal")
	consume(small, tokB)
	mustPTTL(smallW, "renewed by consume")

	// 大值副本再写一次,整键 TTL 延长回 bigW。
	reserve(big)
	mustPTTL(bigW, "larger window extends it again")
}

// 占位键不存在、或者在但没有 TTL 时,任何一条写路径写完都一定有整键 TTL:否则 zone 下线之后键永久残留。
// 钉住 admittedKeyTTL 注释里的两个陷阱:换成 PEXPIRE … GT,没有 TTL 的键永远设不上;先读 PTTL 再 ZADD,
// 键不存在时 PEXPIRE 打空,随后 ZADD 建出的键没有 TTL。两种写错都在这里红(写完 PTTL = -1)。
// 「键不存在」覆盖首次写入与止血 DEL 之后;「键在但没有 TTL」用 PERSIST 造(例如有人手工 ZADD 过)。
func TestAdmittedKey_EveryWritePathSetsTTLWhenMissing(t *testing.T) {
	ctx := context.Background()
	key := admittedZSetKey(1)

	shapes := []struct {
		name  string
		apply func(t *testing.T, rdb *redis.Client)
	}{
		{"key missing", func(t *testing.T, rdb *redis.Client) {
			t.Helper()
			if err := rdb.Del(ctx, key).Err(); err != nil {
				t.Fatalf("del: %v", err)
			}
		}},
		{"key without TTL", func(t *testing.T, rdb *redis.Client) {
			t.Helper()
			// 分数远在未来:快速通道脚本先清过期成员,种子若已过期会被清掉、键随之消失,就测不到这种形状。
			if err := rdb.ZAdd(ctx, key, redis.Z{Score: 4e12, Member: "seed"}).Err(); err != nil {
				t.Fatalf("seed: %v", err)
			}
			if err := rdb.Persist(ctx, key).Err(); err != nil {
				t.Fatalf("persist: %v", err)
			}
		}},
	}
	// 每条路径先做自己的准备,再调 shape 把占位键摆成要测的形状,最后做被测的那一次写。
	paths := []struct {
		name string
		run  func(t *testing.T, q *Queue, shape func())
	}{
		{"fast path", func(t *testing.T, q *Queue, shape func()) {
			t.Helper()
			shape()
			if ok, err := q.TryReserveFastPathSlot(ctx, 1, 10); err != nil || !ok {
				t.Fatalf("reserve = (%v, %v), want (true, nil)", ok, err)
			}
		}},
		{"admit", func(t *testing.T, q *Queue, shape func()) {
			t.Helper()
			if _, err := q.Enqueue(ctx, 1, "u", "d"); err != nil {
				t.Fatalf("enqueue: %v", err)
			}
			shape()
			if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
				t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
			}
		}},
		{"consume", func(t *testing.T, q *Queue, shape func()) {
			t.Helper()
			res, err := q.Enqueue(ctx, 1, "u", "d")
			if err != nil {
				t.Fatalf("enqueue: %v", err)
			}
			if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
				t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
			}
			shape()
			if st, err := q.Lookup(ctx, res.QueueToken, stubSignFn); err != nil || st.Status != StatusAdmitted {
				t.Fatalf("lookup = (%+v, %v), want ADMITTED", st, err)
			}
		}},
	}

	for _, p := range paths {
		t.Run(p.name, func(t *testing.T) {
			for _, s := range shapes {
				t.Run(s.name, func(t *testing.T) {
					q, rdb, _ := newClockedQueue(t)
					p.run(t, q, func() {
						t.Helper()
						s.apply(t, rdb)
						// go-redis 把 PTTL 的 -1(无 TTL)/ -2(键不存在)原样放进 Duration,两者都 < 0。
						if ttl, err := rdb.PTTL(ctx, key).Result(); err != nil || ttl >= 0 {
							t.Fatalf("shape %q: PTTL = %v err=%v, want no TTL (-1 or -2)", s.name, ttl, err)
						}
					})
					if ttl, err := rdb.PTTL(ctx, key).Result(); err != nil || ttl != q.admittedKeyTTL() {
						t.Fatalf("whole-key PTTL after the write = %v err=%v, want %v: a placeholder key must never be left without a TTL",
							ttl, err, q.admittedKeyTTL())
					}
				})
			}
		})
	}
}

// ── 放行窗口已过:给终态 EXPIRED,不停在名次 0(2026-10-15) ─────────────────

// expiredTotalValue 从默认 registry 读 login_queue_expired_total{zone, reason} 的当前值,序列还没出现时回 0。
// 计数器是进程级的,用例读前后差值,不依赖绝对值。
func expiredTotalValue(t *testing.T, zone, reason string) float64 {
	t.Helper()
	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather metrics: %v", err)
	}
	for _, mf := range families {
		if mf.GetName() != "login_queue_expired_total" {
			continue
		}
		for _, m := range mf.GetMetric() {
			var z, r string
			for _, l := range m.GetLabel() {
				switch l.GetName() {
				case "zone":
					z = l.GetValue()
				case "reason":
					r = l.GetValue()
				}
			}
			if z == zone && r == reason {
				return m.GetCounter().GetValue()
			}
		}
	}
	return 0
}

// signMustNotRun:没有放行槽可取时 signFn 不该被调用。
func signMustNotRun(t *testing.T) SignAdmitFn {
	return func(*AdmitSlot) (*AdmitToken, error) {
		t.Fatal("signFn called although there is no admit slot to take")
		return nil, nil
	}
}

// (i) 放行后没人来取,放行槽按 AdmitTTL 过期:再查回 EXPIRED(gateway 映射 410,客户端丢掉 token 重走
// /assign-gate),记一次 admit_window_passed。
// 修复前红:回「排队中、名次 0」(meta 与 token 索引还在、ZRANK 落空),客户端一直轮询到 meta 1h 过期,也不计数。
// miniredis 的 TTL 只在 FastForward 里递减,拨 AdmitTTL 正好让放行槽过期,meta 与 token 索引(1h)还在。
// 计数器要指标开关打开才写(go-zero metric.update);开关是进程级、打开后关不回去,本包其余用例不依赖它关着
// (dispatcher_test 的 leader gauge 用例自己也会打开它)。
// (2026-10-15 补注)存储布局(admitted_at 字段、meta TTL 不变)挪到 TestPopAdmit_MarksAdmittedAtKeepingMetaTTL;本用例只断言行为:
// EXPIRED、admit_window_passed 计数 +1、这次查询不给在途占位续期(假时钟与 FastForward 同拨 AdmitTTL 后计数为 0)。
// 修复前红在 EXPIRED 断言上(回 QUEUEING、名次 0)。EXPIRED 分支若误把占位改成「现在 + handoffGrace」,红在最后的计数上。
func TestLookup_AdmitSlotExpiredUnclaimed_ReturnsExpired(t *testing.T) {
	gzprom.Enable()
	q, rdb, mr, cleanup := newTestQueue(t)
	defer cleanup()
	// newClockedQueue 不返回 mr,这里手工换成假时钟:放行槽(miniredis TTL)与占位(假时钟分数)要一起拨。
	clk := newFakeClock()
	q.now = clk.Now
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}

	// 放行槽(miniredis TTL)与占位(假时钟分数)一起走到「放行时刻 + AdmitTTL」。
	mr.FastForward(q.admitTTL)
	clk.Advance(q.admitTTL)
	if n, err := rdb.Exists(ctx, admitKey(res.QueueID)).Result(); err != nil || n != 0 {
		t.Fatalf("admit slot exists=%d err=%v after AdmitTTL, want gone", n, err)
	}

	before := expiredTotalValue(t, "1", ExpireReasonAdmitWindowPassed)
	st, err := q.Lookup(ctx, res.QueueToken, signMustNotRun(t))
	if err != nil || st.Status != StatusExpired {
		t.Fatalf("lookup after the admit slot expired = (%+v, %v), want EXPIRED, not QUEUEING rank 0 until the 1h meta TTL", st, err)
	}
	if got := expiredTotalValue(t, "1", ExpireReasonAdmitWindowPassed) - before; got != 1 {
		t.Fatalf(`login_queue_expired_total{zone="1",reason="admit_window_passed"} grew by %v, want 1`, got)
	}
	// 占位按「放行时刻 + AdmitTTL」到期(计数的清理含端点);回 EXPIRED 的这次查询不能把它续成「现在 + handoffGrace」。
	mustAdmittedCount(t, q, 0, "at AdmitTTL after an EXPIRED lookup")
}

// (ii) 取走之后同一令牌再查(回包丢失后的重试):EXPIRED,记一次 admit_window_passed;签票只在第一次。
// 修复前红:第二次回「排队中、名次 0」,直到 meta 1h 过期。
func TestLookup_PollAfterConsume_ReturnsExpired(t *testing.T) {
	gzprom.Enable() // 见 TestLookup_AdmitSlotExpiredUnclaimed_ReturnsExpired
	q, _, _ := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	signs := 0
	countingSign := func(slot *AdmitSlot) (*AdmitToken, error) {
		signs++
		return stubSignFn(slot)
	}
	if st, err := q.Lookup(ctx, res.QueueToken, countingSign); err != nil || st.Status != StatusAdmitted {
		t.Fatalf("first lookup = (%+v, %v), want ADMITTED", st, err)
	}

	before := expiredTotalValue(t, "1", ExpireReasonAdmitWindowPassed)
	st, err := q.Lookup(ctx, res.QueueToken, countingSign)
	if err != nil || st.Status != StatusExpired {
		t.Fatalf("lookup after consume = (%+v, %v), want EXPIRED, not QUEUEING rank 0 until the 1h meta TTL", st, err)
	}
	if got := expiredTotalValue(t, "1", ExpireReasonAdmitWindowPassed) - before; got != 1 {
		t.Fatalf(`login_queue_expired_total{zone="1",reason="admit_window_passed"} grew by %v, want 1`, got)
	}
	if signs != 1 {
		t.Fatalf("signFn ran %d times, want exactly 1 (only the first lookup takes the slot)", signs)
	}
}

// (iii) 在途窗口:dispatcher 已 ZPOPMIN、写放行槽的 MULTI 还没落地。手工模拟:只从队列 ZSET 删掉成员,
// 不写放行槽、不写 admitted_at。仍回「排队中、名次 0」,让客户端照常再查,不能判成 EXPIRED。
// 修复前后都绿:钉住修复没有把在途窗口一并判成终态。
// (2026-10-15 更正)放行写入现为 admitEntryScript 一段脚本(放行槽、admitted_at、占位同时可见),不再是 MULTI;
// 模拟的时序与断言不变。
func TestLookup_InFlightPop_StillQueueingRankZero(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if n, err := rdb.ZRem(ctx, zoneZSetKey(1), res.QueueID).Result(); err != nil || n != 1 {
		t.Fatalf("zrem = (%d, %v), want (1, nil)", n, err)
	}

	st, err := q.Lookup(ctx, res.QueueToken, signMustNotRun(t))
	if err != nil || st.Status != StatusQueueing || st.Rank != 0 {
		t.Fatalf("lookup in the in-flight window = (%+v, %v), want QUEUEING rank 0", st, err)
	}
}

// (iv) 竞态:第一次 GETDEL 落空之后、读 meta 之前,dispatcher 的 MULTI 才落地。用注入点在这两步之间跑一次
// 真实的 PopAdmit(ZPOPMIN + 写放行槽、admitted_at 与占位的 MULTI)。Lookup 看到 admitted_at,再取一次:
// 回 ADMITTED、只签一次票,占位改为「取走时刻 + handoffGrace」。
// 修复前红(注入点照样放进旧代码):读 meta 只拿 zone_id,ZRANK 落空回「排队中、名次 0」。只加 admitted_at
// 判定、不做二次 GETDEL 的写法也红:把一个刚放行、还没取走的条目判成 EXPIRED,放行槽白白等到 AdmitTTL。
// (2026-10-15 更正)上文两处「MULTI」现为 admitEntryScript 一段脚本(放行槽、admitted_at、占位同时可见);
// 模拟的时序与断言不变。
func TestLookup_AdmitLandsBetweenGetDelAndMetaRead_Admits(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	hookCalls := 0
	q.afterAdmitMiss = func(queueID string) {
		hookCalls++
		if queueID != res.QueueID {
			t.Fatalf("afterAdmitMiss(%q), want %q", queueID, res.QueueID)
		}
		if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
			t.Fatalf("popadmit inside the window = (%d, %v), want (1, nil)", n, err)
		}
	}
	signs := 0
	countingSign := func(slot *AdmitSlot) (*AdmitToken, error) {
		signs++
		return stubSignFn(slot)
	}

	st, err := q.Lookup(ctx, res.QueueToken, countingSign)
	if err != nil || st.Status != StatusAdmitted || st.Admit == nil || st.Admit.IP != "10.0.0.1" {
		t.Fatalf("lookup = (%+v, %v), want ADMITTED with the gate picked at admit time", st, err)
	}
	if hookCalls != 1 {
		t.Fatalf("afterAdmitMiss ran %d times, want 1: the race window was not exercised", hookCalls)
	}
	if signs != 1 {
		t.Fatalf("signFn ran %d times, want exactly 1", signs)
	}
	if n, err := rdb.Exists(ctx, admitKey(res.QueueID)).Result(); err != nil || n != 0 {
		t.Fatalf("admit slot exists=%d err=%v after lookup, want taken", n, err)
	}
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if want := float64(clk.Now().Add(q.handoffGrace).UnixMilli()); err != nil || score != want {
		t.Fatalf("placeholder score = %.0f err=%v, want %.0f (consume time + handoff grace, not admit time + AdmitTTL)",
			score, err, want)
	}
}

// PopAdmit 在 meta 里记下 admitted_at(包注释 Redis 布局:Lookup 据此区分「放行窗口已过」与「在途窗口」),
// 且不动 meta 的 TTL:仍按入队时的 QueueEntryTTL 到期。miniredis 的 TTL 只在 FastForward 里递减,入队后先拨
// waited,meta 剩 entryTTL-waited;写 admitted_at 时被抹掉(-1)、被续期(回到 entryTTL)、被缩短都在等式处红。
// 不拨时间的区间断言 (0, entryTTL] 分不出「保持」与「续期到 entryTTL」。
// 这是存储布局用例,行为由 TestLookup_AdmitSlotExpiredUnclaimed_ReturnsExpired 钉住;修复前红在 admitted_at 断言上。
func TestPopAdmit_MarksAdmittedAtKeepingMetaTTL(t *testing.T) {
	q, rdb, mr, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if v, err := rdb.HGet(ctx, metaKey(res.QueueID), metaFieldAdmittedAt).Result(); !errors.Is(err, redis.Nil) {
		t.Fatalf("meta %s before admit = (%q, %v), want absent", metaFieldAdmittedAt, v, err)
	}
	const waited = 10 * time.Minute // 在队里等过的时间,须小于 entryTTL(1h)
	mr.FastForward(waited)
	want := q.entryTTL - waited
	if ttl, err := rdb.PTTL(ctx, metaKey(res.QueueID)).Result(); err != nil || ttl != want {
		t.Fatalf("meta PTTL before admit = %v err=%v, want %v", ttl, err, want)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil)", n, err)
	}
	if v, err := rdb.HGet(ctx, metaKey(res.QueueID), metaFieldAdmittedAt).Result(); err != nil || v == "" {
		t.Fatalf("meta %s after admit = (%q, %v), want the admit time", metaFieldAdmittedAt, v, err)
	}
	if ttl, err := rdb.PTTL(ctx, metaKey(res.QueueID)).Result(); err != nil || ttl != want {
		t.Fatalf("meta PTTL after admit = %v err=%v, want %v: writing admitted_at must neither drop, renew nor shorten the entry TTL", ttl, err, want)
	}
}

// 写 admitted_at 不凭空建出 meta:meta 已不在(在队里待满 QueueEntryTTL 或被内存淘汰)时 PopAdmit 照常写放行槽,
// 但不建出一个没有 TTL、永不过期的 meta。直接 HSET 的写法在这里红(meta 存在且 TTL = -1)。
// 修复前也绿(那时不写 meta),钉住的是新写入的安全性。
// (2026-10-15 补注)后半段断言翻转:meta 不在时 PopAdmit 整条丢弃——不写放行槽、不占在途名额、不计入放行数,
// 条目也不回队列。生产里 meta 不在时 token 索引同时不在(Enqueue 同一个 MULTI、同一个 TTL 设下,部署不淘汰键),
// 没有人能取到放行槽,写了只是白占一个 AdmitTTL 的名额。本用例手工只删 meta、留下 token 索引,这个形状生产里
// 只有人工删键才造得出;此时查询回 EXPIRED(meta_gone),客户端重新入队。修复前红在 n == 0 上(回 1)。
// 走真实到期路径的版本见 TestPopAdmit_EntryOlderThanQueueEntryTTL_NotAdmitted。
func TestPopAdmit_MetaGone_DoesNotRecreateMeta(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	if err := rdb.Del(ctx, metaKey(res.QueueID)).Err(); err != nil {
		t.Fatalf("del meta: %v", err)
	}
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 0 {
		t.Fatalf("popadmit = (%d, %v), want (0, nil): an entry whose meta is gone is not admitted", n, err)
	}
	if n, err := rdb.Exists(ctx, metaKey(res.QueueID)).Result(); err != nil || n != 0 {
		t.Fatalf("meta exists=%d err=%v after admit, want still gone", n, err)
	}
	if n, err := rdb.Exists(ctx, admitKey(res.QueueID)).Result(); err != nil || n != 0 {
		t.Fatalf("admit slot exists=%d err=%v, want none written for an orphan", n, err)
	}
	mustAdmittedCount(t, q, 0, "orphan not held")
	if n, err := rdb.ZCard(ctx, zoneZSetKey(1)).Result(); err != nil || n != 0 {
		t.Fatalf("queue length = %d err=%v, want 0 (the orphan is popped and dropped, not re-queued)", n, err)
	}
	if st, err := q.Lookup(ctx, res.QueueToken, signMustNotRun(t)); err != nil || st.Status != StatusExpired {
		t.Fatalf("lookup = (%+v, %v), want EXPIRED (meta_gone)", st, err)
	}
}

// 真实到期路径:A 在队里待满 QueueEntryTTL,meta 与 token 索引同时过期(Enqueue 同一个 MULTI、同一个 TTL);
// 队列键因 B 入队续了 TTL,A 仍是队首成员。同一批弹出 A、B:A 不写放行槽、不占在途名额、不计入放行数,
// B 照常放行并能取走。修复前红在 n == 1 上(回 2:孤儿 A 也写了放行槽和占位,白占一个 AdmitTTL 的名额)。
// miniredis 的 TTL 只在 FastForward 里递减;占位走假时钟。
func TestPopAdmit_EntryOlderThanQueueEntryTTL_NotAdmitted(t *testing.T) {
	q, rdb, mr, cleanup := newTestQueue(t)
	defer cleanup()
	q.now = newFakeClock().Now
	ctx := context.Background()

	a, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue A: %v", err)
	}
	mr.FastForward(q.entryTTL / 2)
	b, err := q.Enqueue(ctx, 1, "bob", "dev2") // 给队列键续上 entryTTL;A 的 meta 与 token 索引不续
	if err != nil {
		t.Fatalf("enqueue B: %v", err)
	}
	mr.FastForward(q.entryTTL - q.entryTTL/2)
	if n, err := rdb.Exists(ctx, metaKey(a.QueueID), tokenIndexKey(a.QueueToken)).Result(); err != nil || n != 0 {
		t.Fatalf("A's meta + token index exist=%d err=%v after QueueEntryTTL, want both expired", n, err)
	}
	if n, err := rdb.ZCard(ctx, zoneZSetKey(1)).Result(); err != nil || n != 2 {
		t.Fatalf("queue length = %d err=%v, want 2 (A is still a member)", n, err)
	}

	if n, err := q.PopAdmit(ctx, 1, 2, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil): only B is admitted", n, err)
	}
	if n, err := rdb.Exists(ctx, admitKey(a.QueueID)).Result(); err != nil || n != 0 {
		t.Fatalf("A's admit slot exists=%d err=%v, want none", n, err)
	}
	mustAdmittedCount(t, q, 1, "only B holds a slot")
	if st, err := q.Lookup(ctx, a.QueueToken, signMustNotRun(t)); err != nil || st.Status != StatusExpired {
		t.Fatalf("lookup A = (%+v, %v), want EXPIRED", st, err)
	}
	if st, err := q.Lookup(ctx, b.QueueToken, stubSignFn); err != nil || st.Status != StatusAdmitted {
		t.Fatalf("lookup B = (%+v, %v), want ADMITTED", st, err)
	}
}

// 停服(Stop → cancel)或丢租约(onLost → drainCancel)取消的是 dispatcher 传进来的 ctx。ZPOPMIN 之后这批条目
// 只在本进程内存里,取消不能让剩下的写入作废:否则它们既不在队列、也没有放行槽和 admitted_at,玩家停在名次 0
// 直到 meta 过期(1h)。用 pickFn 在批中途取消 ctx,确定地落在「已弹出、未写完」之间。
// 修复前红在 n == 3 上(回 1:第 2、3 条的写入在 go-redis 取连接时就以 context.Canceled 返回)。
func TestPopAdmit_CtxCanceledMidBatch_StillWritesPoppedEntries(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	bg := context.Background()

	var queued []*EnqueueResult
	for _, account := range []string{"alice", "bob", "carol"} {
		res, err := q.Enqueue(bg, 1, account, "dev")
		if err != nil {
			t.Fatalf("enqueue %s: %v", account, err)
		}
		queued = append(queued, res)
	}

	ctx, cancel := context.WithCancel(bg)
	defer cancel()
	picks := 0
	cancelOnSecondPick := func(zoneID uint32, queueID string) (*AdmitSlot, error) {
		picks++
		if picks == 2 {
			cancel() // 第 1 条已写完,第 2、3 条已弹出、还没写
		}
		return pickZone1Gate(zoneID, queueID)
	}
	if n, err := q.PopAdmit(ctx, 1, 3, cancelOnSecondPick); err != nil || n != 3 {
		t.Fatalf("popadmit = (%d, %v), want (3, nil): every popped entry must still be written after the caller ctx is canceled", n, err)
	}
	if picks != 3 {
		t.Fatalf("pickFn ran %d times, want 3", picks)
	}
	if n, err := rdb.ZCard(bg, zoneZSetKey(1)).Result(); err != nil || n != 0 {
		t.Fatalf("queue length = %d err=%v, want 0", n, err)
	}
	mustAdmittedCount(t, q, 3, "all three popped entries hold a slot")
	for _, res := range queued {
		if st, err := q.Lookup(bg, res.QueueToken, stubSignFn); err != nil || st.Status != StatusAdmitted {
			t.Fatalf("lookup %s = (%+v, %v), want ADMITTED", res.QueueID, st, err)
		}
	}
}

// 调用方 ctx 在弹出之前就已取消:ZPOPMIN 仍随它失败,一个条目都不弹出、不写。钉住「只有弹出之后的写入不随
// 调用方取消」:把 ZPOPMIN 也换成不可取消 ctx 的写法在这里红(停服或丢租约之后还会弹出新的一批)。修复前后都绿。
func TestPopAdmit_CtxCanceledBeforePop_PopsNothing(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	bg := context.Background()

	res, err := q.Enqueue(bg, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	ctx, cancel := context.WithCancel(bg)
	cancel()
	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err == nil || n != 0 {
		t.Fatalf("popadmit with a canceled ctx = (%d, %v), want (0, error)", n, err)
	}
	if n, err := rdb.ZCard(bg, zoneZSetKey(1)).Result(); err != nil || n != 1 {
		t.Fatalf("queue length = %d err=%v, want 1 (nothing popped)", n, err)
	}
	if n, err := rdb.Exists(bg, admitKey(res.QueueID)).Result(); err != nil || n != 0 {
		t.Fatalf("admit slot exists=%d err=%v, want none", n, err)
	}
}

// enqueueAccounts 往 zone 1 依次入队,返回 queueId → 排队令牌。入队分数是毫秒墙钟,同一毫秒内的条目按 queueId
// 字典序出队,用例不能假定出队顺序等于入队顺序,要按 pickFn 实际看到的 queueId 对号。
func enqueueAccounts(t *testing.T, q *Queue, accounts ...string) map[string]string {
	t.Helper()
	tokens := make(map[string]string, len(accounts))
	for _, account := range accounts {
		res, err := q.Enqueue(context.Background(), 1, account, "dev")
		if err != nil {
			t.Fatalf("enqueue %s: %v", account, err)
		}
		tokens[res.QueueID] = res.QueueToken
	}
	return tokens
}

// 没有取消时写入预算不起算:一批写多久都照写,与改动前写入沿用 drainCtx(没有截止)一致。开区首拍一批可达数千条,
// 串行写可能超过 3s;预算若自弹出时起算,尾部条目会丢失、停在名次 0 约 1h,且不占在途名额、下一拍 free 不降,
// 每拍的尾部都这样丢。q.popAdmitWriteBudget = 0 让「自弹出起算」的写法当场到期,确定地代表「这一批写超了预算」,
// 不依赖墙钟。修复前(预算自 ZPOPMIN 返回时起算的写法)红在 n == 3 上:回 (0, nil),三条都已弹出、都没写成。
// HEAD(写入沿用 drainCtx)本来就绿,这条钉住的是「不随取消结束」的改动不引入截断。
func TestPopAdmit_NoCancel_LongBatchNotCutByWriteBudget(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	q.popAdmitWriteBudget = 0
	ctx := context.Background()

	tokens := enqueueAccounts(t, q, "alice", "bob", "carol")
	if n, err := q.PopAdmit(ctx, 1, 3, pickZone1Gate); err != nil || n != 3 {
		t.Fatalf("popadmit = (%d, %v), want (3, nil): without a cancel the write budget must not start", n, err)
	}
	if n, err := rdb.ZCard(ctx, zoneZSetKey(1)).Result(); err != nil || n != 0 {
		t.Fatalf("queue length = %d err=%v, want 0", n, err)
	}
	mustAdmittedCount(t, q, 3, "all three popped entries hold a slot")
	for queueID, token := range tokens {
		if st, err := q.Lookup(ctx, token, stubSignFn); err != nil || st.Status != StatusAdmitted {
			t.Fatalf("lookup %s = (%+v, %v), want ADMITTED", queueID, st, err)
		}
	}
}

// 预算有上限:批中途取消、预算为 0(取消一被看到就到期)时,取消之前写成的第 1 条照常放行,其余两条照旧丢失——
// 不在队列、没有放行槽、不占在途名额,查询停在名次 0。这是登记过的残余丢条目路径(停服时预算内写不完的尾部,
// 汇总 ERROR 记下),钉住「取消之后不会无限写下去」,也钉住预算从看到取消时才起算。
// 会红的写法:取消之后也不设截止,红在 n == 1 上(回 3);预算自弹出时起算,同样红在 n == 1 上(回 0:第 1 条在取消
// 之前也写不成)。HEAD(写入沿用 drainCtx)这里是绿的。
func TestPopAdmit_CtxCanceledMidBatch_WriteBudgetExhausted_DropsRest(t *testing.T) {
	q, rdb, _ := newClockedQueue(t)
	q.popAdmitWriteBudget = 0
	bg := context.Background()

	tokens := enqueueAccounts(t, q, "alice", "bob", "carol")
	ctx, cancel := context.WithCancel(bg)
	defer cancel()
	var picked []string
	cancelOnSecondPick := func(zoneID uint32, queueID string) (*AdmitSlot, error) {
		picked = append(picked, queueID)
		if len(picked) == 2 {
			cancel() // 第 1 条已写完,第 2、3 条已弹出、还没写
		}
		return pickZone1Gate(zoneID, queueID)
	}
	if n, err := q.PopAdmit(ctx, 1, 3, cancelOnSecondPick); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil): only the entry written before the cancel is admitted", n, err)
	}
	if len(picked) != 3 {
		t.Fatalf("pickFn ran %d times, want 3", len(picked))
	}
	if n, err := rdb.ZCard(bg, zoneZSetKey(1)).Result(); err != nil || n != 0 {
		t.Fatalf("queue length = %d err=%v, want 0 (dropped entries are not re-queued)", n, err)
	}
	mustAdmittedCount(t, q, 1, "only the entry written before the cancel holds a slot")
	if st, err := q.Lookup(bg, tokens[picked[0]], stubSignFn); err != nil || st.Status != StatusAdmitted {
		t.Fatalf("lookup %s = (%+v, %v), want ADMITTED", picked[0], st, err)
	}
	for _, queueID := range picked[1:] {
		if n, err := rdb.Exists(bg, admitKey(queueID)).Result(); err != nil || n != 0 {
			t.Fatalf("admit slot of %s exists=%d err=%v, want none", queueID, n, err)
		}
		st, err := q.Lookup(bg, tokens[queueID], signMustNotRun(t))
		if err != nil || st.Status != StatusQueueing || st.Rank != 0 {
			t.Fatalf("lookup %s = (%+v, %v), want QUEUEING at rank 0 (dropped after the budget ran out)", queueID, st, err)
		}
	}
}

// replayAdmitScriptHook 模拟 go-redis 对单条命令的自动重发:第一次发给 admit:{queueID} 的脚本(EVAL / EVALSHA)
// 执行成功之后,先跑 between(回包丢失期间发生的事),再把同一条命令原样再发一次,以第二次的结果作为这条命令的结果。
// go-redis 对单条命令在读超时、连接断开后会重发(baseClient.process),对 MULTI 读失败不重发(txPipelineProcessCmds)。
type replayAdmitScriptHook struct {
	admitKey string
	between  func()
	replays  *int
}

func (h replayAdmitScriptHook) DialHook(next redis.DialHook) redis.DialHook { return next }

func (h replayAdmitScriptHook) ProcessHook(next redis.ProcessHook) redis.ProcessHook {
	return func(ctx context.Context, cmd redis.Cmder) error {
		if err := next(ctx, cmd); err != nil || *h.replays > 0 {
			return err
		}
		if name := cmd.Name(); name != "eval" && name != "evalsha" {
			return nil
		}
		// EVAL / EVALSHA 的参数:命令名、脚本或 sha、键数、键...
		if args := cmd.Args(); len(args) < 4 || args[3] != h.admitKey {
			return nil
		}
		*h.replays++
		h.between()
		return next(ctx, cmd)
	}
}

func (h replayAdmitScriptHook) ProcessPipelineHook(next redis.ProcessPipelineHook) redis.ProcessPipelineHook {
	return next
}

// 放行脚本被重发:第一次已执行、回包丢了,期间客户端取走了放行槽,随后 go-redis 重发同一次脚本。重发不能再写
// 一个放行槽(同一排队票会被签出第二张 gate 票),也不能把占位分数拨回「放行时刻 + AdmitTTL」;这次放行照常计数。
// 修复前(MULTI 写法)红在钩子没有触发:go-redis 不重发 MULTI,这条风险是改成单条脚本之后才有的;
// 脚本去掉 admitted_at 守卫的写法红在「放行槽被重写」上(再往下,第二次 Lookup 会回 ADMITTED、签出第二张票)。
func TestPopAdmit_AdmitScriptReplayedAfterConsume_DoesNotReissue(t *testing.T) {
	q, rdb, clk := newClockedQueue(t)
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	signs := 0
	countingSign := func(slot *AdmitSlot) (*AdmitToken, error) {
		signs++
		return stubSignFn(slot)
	}
	replays := 0
	var consumedAt time.Time
	rdb.AddHook(replayAdmitScriptHook{
		admitKey: admitKey(res.QueueID),
		replays:  &replays,
		between: func() {
			clk.Advance(2 * time.Second) // 客户端的下一次轮询
			consumedAt = clk.Now()
			if st, err := q.Lookup(ctx, res.QueueToken, countingSign); err != nil || st.Status != StatusAdmitted {
				t.Fatalf("lookup between the two runs = (%+v, %v), want ADMITTED", st, err)
			}
		},
	})

	if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err != nil || n != 1 {
		t.Fatalf("popadmit = (%d, %v), want (1, nil): the replayed admit still counts once", n, err)
	}
	if replays != 1 {
		t.Fatalf("admit script replayed %d times, want 1: the retry window was not exercised", replays)
	}
	if n, err := rdb.Exists(ctx, admitKey(res.QueueID)).Result(); err != nil || n != 0 {
		t.Fatalf("admit slot exists=%d err=%v after the replay, want none: the replay must not write a second slot", n, err)
	}
	score, err := rdb.ZScore(ctx, admittedZSetKey(1), res.QueueID).Result()
	if want := float64(consumedAt.Add(q.handoffGrace).UnixMilli()); err != nil || score != want {
		t.Fatalf("placeholder score = %.0f err=%v, want %.0f (consume time + handoff grace, not reset by the replay)",
			score, err, want)
	}
	if st, err := q.Lookup(ctx, res.QueueToken, countingSign); err != nil || st.Status != StatusExpired {
		t.Fatalf("lookup after the replay = (%+v, %v), want EXPIRED, not a second ADMITTED", st, err)
	}
	if signs != 1 {
		t.Fatalf("signFn ran %d times, want exactly 1", signs)
	}
}

// AdmitTTL 配成 0(显式写 0s 不回落默认值)时 PopAdmit 一个都不弹出、回错误,条目原样留在队列,不写放行槽。
// 修复前红在「回错误」上:回 (1, nil),写出一个不带 TTL、永不过期的放行槽(go-redis 的 Set(..., 0) 不设过期)。
// 只改放行脚本、不加这道拦截的写法也红,红在回 (0, nil) 上:SET PX 0 报错,条目已弹出、丢失(队列长度 0)。
// (2026-10-15 补注)加了 500µs 一组:放行脚本以 AdmitTTL.Milliseconds() 作 SET PX,不足 1ms 取整为 0,
// miniredis 与 Redis 一样拒绝 SET PX 0。ValidateTTLs 只拒 <=0 的写法红在这一组的「回错误」上(回 (0, nil):
// 条目已弹出、脚本报错、丢失);HEAD 同样红在这里(回 (1, nil),go-redis 的 Set 把 500µs 截成 PX 1)。
func TestPopAdmit_NonPositiveAdmitTTL_PopsNothing(t *testing.T) {
	for _, admitTTL := range []time.Duration{0, 500 * time.Microsecond} {
		t.Run(admitTTL.String(), func(t *testing.T) {
			_, rdb, _, cleanup := newTestQueue(t)
			defer cleanup()
			q := New(rdb, time.Hour, admitTTL, []byte("test-secret"))
			ctx := context.Background()

			res, err := q.Enqueue(ctx, 1, "alice", "dev1")
			if err != nil {
				t.Fatalf("enqueue: %v", err)
			}
			if n, err := q.PopAdmit(ctx, 1, 1, pickZone1Gate); err == nil || n != 0 {
				t.Fatalf("popadmit with AdmitTTL=%v = (%d, %v), want (0, error)", admitTTL, n, err)
			}
			if n, err := rdb.ZCard(ctx, zoneZSetKey(1)).Result(); err != nil || n != 1 {
				t.Fatalf("queue length = %d err=%v, want 1 (the entry stays queued)", n, err)
			}
			if n, err := rdb.Exists(ctx, admitKey(res.QueueID)).Result(); err != nil || n != 0 {
				t.Fatalf("admit slot exists=%d err=%v, want none", n, err)
			}
		})
	}
}

// AdmitTTL、QueueEntryTTL 非正时排队照样能起,但放行槽写不进(SET PX 报错)或写成永不过期、EXPIRE ≤0 当场删掉
// 整个队列、token 索引不带 TTL。起服装配据 ValidateTTLs 拒绝。不要求 admitTTL < entryTTL(最后一组钉住)。
// 修复前没有这个函数(编不过即红)。
// (2026-10-15 补注)AdmitTTL 另有 1ms 下界(放行脚本的 SET PX 取整到毫秒,见 ValidateTTLs):500µs 一组在只拒 <=0
// 的写法下红,1ms 一组钉住下界本身可用。
func TestValidateTTLs_RejectsNonPositive(t *testing.T) {
	cases := []struct {
		entryTTL, admitTTL time.Duration
		wantErr            bool
	}{
		{0, 60 * time.Second, true},
		{-time.Second, 60 * time.Second, true},
		{time.Hour, 0, true},
		{time.Hour, -time.Second, true},
		{time.Hour, 60 * time.Second, false},
		{time.Hour, 2 * time.Hour, false},
		{time.Hour, 500 * time.Microsecond, true},
		{time.Hour, time.Millisecond, false},
	}
	for _, c := range cases {
		err := ValidateTTLs(c.entryTTL, c.admitTTL)
		if (err != nil) != c.wantErr {
			t.Errorf("ValidateTTLs(entry=%v, admit=%v) = %v, want error=%v", c.entryTTL, c.admitTTL, err, c.wantErr)
		}
		if err != nil && !strings.Contains(err.Error(), "ttl must be positive") {
			t.Errorf("ValidateTTLs(entry=%v, admit=%v) error = %q, want it to say the ttl must be positive", c.entryTTL, c.admitTTL, err)
		}
	}
}
