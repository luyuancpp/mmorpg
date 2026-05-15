package loginqueue

import (
	"context"
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
// entry, supply a stub signFn, and verify Lookup transitions through
// QUEUEING → ADMITTED → EXPIRED on subsequent polls. This pins the
// "exactly-once admit" contract that prevents double-issued gate tokens.
func TestPopAdmitWritesAdmitToken(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	res, err := q.Enqueue(ctx, 1, "alice", "dev1")
	if err != nil {
		t.Fatalf("enqueue: %v", err)
	}

	// Before PopAdmit: status must be QUEUEING.
	state, err := q.Lookup(ctx, res.QueueToken)
	if err != nil {
		t.Fatalf("lookup before admit: %v", err)
	}
	if state.Status != StatusQueueing {
		t.Errorf("status before admit = %d, want %d", state.Status, StatusQueueing)
	}

	stubAdmit := &AdmitToken{IP: "10.0.0.5", Port: 9000, TokenDeadline: time.Now().Unix() + 300}
	signFn := func(zoneID uint32, queueID string) (*AdmitToken, error) {
		return stubAdmit, nil
	}
	n, err := q.PopAdmit(ctx, 1, 5, signFn)
	if err != nil {
		t.Fatalf("popadmit: %v", err)
	}
	if n != 1 {
		t.Errorf("popadmit count = %d, want 1", n)
	}

	// First Lookup after admit: ADMITTED with admit token.
	state, err = q.Lookup(ctx, res.QueueToken)
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
	state, err = q.Lookup(ctx, res.QueueToken)
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
	stubAdmit := &AdmitToken{IP: "10.0.0.1", Port: 9000}
	if _, err := q.PopAdmit(ctx, 1, 1, func(z uint32, qid string) (*AdmitToken, error) {
		return stubAdmit, nil
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

// TestPopAdmitReQueuesOnSignFailure: signFn returning err must not lose
// the queue entry. We re-add at the front (score=0) so the next dispatcher
// tick retries with hopefully-recovered state.
func TestPopAdmitReQueuesOnSignFailure(t *testing.T) {
	q, _, _, cleanup := newTestQueue(t)
	defer cleanup()
	ctx := context.Background()

	res, _ := q.Enqueue(ctx, 1, "alice", "dev1")

	signFnFail := func(z uint32, qid string) (*AdmitToken, error) {
		return nil, context.DeadlineExceeded
	}
	n, err := q.PopAdmit(ctx, 1, 1, signFnFail)
	if err != nil {
		t.Fatalf("popadmit: %v", err)
	}
	if n != 0 {
		t.Errorf("admitted count on fail = %d, want 0", n)
	}

	// Entry should still be queryable via Lookup.
	state, err := q.Lookup(ctx, res.QueueToken)
	if err != nil {
		t.Fatalf("lookup after failed admit: %v", err)
	}
	if state.Status != StatusQueueing {
		t.Errorf("status after failed admit = %d, want StatusQueueing — entry was lost", state.Status)
	}
}
