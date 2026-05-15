// Package loginqueue implements a Redis ZSET-backed FIFO queue for AssignGate.
//
// Why this lives here (not in Java Gateway):
//   - Out-of-queue admission requires reading EnterGame state + reconnect
//     decision from sessionmanager + player_locator, both of which are
//     in-process for go-zero login.
//   - Java Gateway's existing RateLimitDecision.QUEUE is a Bucket4j retry
//     hint, not a real ordered queue. The two layers compose: Bucket4j
//     filters obvious spam first, the surviving traffic enters this queue.
//
// Redis layout (per zone):
//
//	queue:zone:{zoneId}        ZSET   member=queueId, score=enqueue unix ms
//	admit:{queueId}            STRING JSON{ip, port, payload, signature, deadline}
//	admitted:zone:{zoneId}     SET    queueId  (sliding TTL, drives in-flight count)
//	queue:meta:{queueId}       HASH   {account, device, zoneId, createdAt}
//	queue:token:{queueToken}   STRING queueId  (token → queueId reverse index)
//
// Lifecycle:
//
//	Enqueue   → ZADD + meta + token mapping (one Redis round-trip via pipeline)
//	Rank      → ZRANK + ZCARD               (cheap, hot path for QueryQueueStatus)
//	Admit     → ZPOPMIN N → write admit:{queueId} + SADD admitted (dispatcher)
//	Consume   → GET admit:{queueId}, DEL it, SREM admitted, return gate token
//
// All operations are designed so a missing intermediate state is recoverable:
// dropped admit:{queueId} just means the client re-enqueues (small UX hiccup,
// not a stuck game). queue:meta TTL is the global ceiling.
package loginqueue

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
)

// Status mirrors AssignGateResponse.status / QueryQueueStatusResponse.status.
type Status uint32

const (
	StatusAdmitted Status = 0
	StatusQueueing Status = 1
	StatusError    Status = 2
	StatusExpired  Status = 3
)

// AdmitToken is what dispatcher writes to admit:{queueId} and what the
// client retrieves on its next AssignGate / QueryQueueStatus call. The
// fields mirror AssignGateResponse so handlers can copy directly.
type AdmitToken struct {
	IP             string `json:"ip"`
	Port           uint32 `json:"port"`
	TokenPayload   []byte `json:"token_payload"`
	TokenSignature []byte `json:"token_signature"`
	TokenDeadline  int64  `json:"token_deadline"`
}

// EnqueueResult bundles everything the AssignGate handler needs to build a
// queueing response in one place.
type EnqueueResult struct {
	QueueID    string
	QueueToken string
	Rank       uint32 // 0-based
	Total      uint32
}

// QueueState is the snapshot returned by Lookup, used by QueryQueueStatus.
type QueueState struct {
	Status Status
	Admit  *AdmitToken // populated when Status == StatusAdmitted
	Rank   uint32      // populated when Status == StatusQueueing
	Total  uint32      // populated when Status == StatusQueueing
}

// Queue wraps Redis ZSET operations behind a small typed surface. The zero
// value is unusable; construct via New.
type Queue struct {
	rdb           *redis.Client
	entryTTL      time.Duration
	admitTTL      time.Duration
	hmacSecret    []byte // signs queue tokens; same secret as gate tokens is fine
}

// New constructs a Queue. hmacSecret is used to sign opaque queue tokens —
// reusing the gate token secret is acceptable because the queue token is
// useless to a gate (different message shape) and useless to login without
// a matching queueId entry, so the threat model is just "client tampering",
// which HMAC over (queueId|expire) defeats.
func New(rdb *redis.Client, entryTTL, admitTTL time.Duration, hmacSecret []byte) *Queue {
	return &Queue{
		rdb:        rdb,
		entryTTL:   entryTTL,
		admitTTL:   admitTTL,
		hmacSecret: hmacSecret,
	}
}

// ── Key helpers ────────────────────────────────────────────────────────

func zoneZSetKey(zoneID uint32) string  { return fmt.Sprintf("queue:zone:%d", zoneID) }
func admittedSetKey(zoneID uint32) string { return fmt.Sprintf("admitted:zone:%d", zoneID) }
func admitKey(queueID string) string    { return "admit:" + queueID }
func metaKey(queueID string) string     { return "queue:meta:" + queueID }
func tokenIndexKey(token string) string { return "queue:token:" + token }

// ── Enqueue ────────────────────────────────────────────────────────────

// Enqueue adds the caller to the zone queue and returns rank+token.
//
// NOT idempotent on (zoneId, account, deviceID): every call mints a fresh
// queueId. A client that loses its queueToken (e.g. due to a network blip
// before the response arrives) and re-calls AssignGate without the token
// WILL be appended at the tail again — the previous entry sits orphaned
// until its 1h TTL expires. The robot client mitigates this by saving
// queueToken from the FIRST AssignGate response and threading it through
// every subsequent retry; the server's reentry path (handleReentry) then
// resolves the saved token instead of creating a duplicate.
//
// We DON'T enqueue under a distributed lock; ZADD is atomic. Duplicates
// from missing-token retries are bounded (1 per retry round-trip) and
// the orphan TTLs out cheaply, so the cost of a (deviceID → queueId)
// reverse index would not pay back. If clients start spamming
// AssignGate-without-token under network failure, revisit.
func (q *Queue) Enqueue(ctx context.Context, zoneID uint32, account, deviceID string) (*EnqueueResult, error) {
	queueID := uuid.NewString()
	token, err := signQueueToken(q.hmacSecret, queueID, zoneID, time.Now().Add(q.entryTTL).Unix())
	if err != nil {
		return nil, fmt.Errorf("sign queue token: %w", err)
	}

	scoreMs := time.Now().UnixMilli()

	pipe := q.rdb.TxPipeline()
	pipe.ZAdd(ctx, zoneZSetKey(zoneID), redis.Z{Score: float64(scoreMs), Member: queueID})
	pipe.Expire(ctx, zoneZSetKey(zoneID), q.entryTTL)
	pipe.HSet(ctx, metaKey(queueID), map[string]any{
		"account":   account,
		"device":    deviceID,
		"zone_id":   zoneID,
		"createdAt": scoreMs,
	})
	pipe.Expire(ctx, metaKey(queueID), q.entryTTL)
	pipe.Set(ctx, tokenIndexKey(token), queueID, q.entryTTL)
	if _, err := pipe.Exec(ctx); err != nil {
		return nil, fmt.Errorf("enqueue pipeline: %w", err)
	}

	rank, err := q.rdb.ZRank(ctx, zoneZSetKey(zoneID), queueID).Result()
	if err != nil {
		// Race: ZADD succeeded but member was popped by the dispatcher between
		// the pipeline and ZRANK. Treat as rank 0 — the client will see it as
		// "next" and likely receive an admit on its first poll.
		if errors.Is(err, redis.Nil) {
			rank = 0
		} else {
			return nil, fmt.Errorf("zrank just-added member: %w", err)
		}
	}
	total, _ := q.rdb.ZCard(ctx, zoneZSetKey(zoneID)).Result()

	return &EnqueueResult{
		QueueID:    queueID,
		QueueToken: token,
		Rank:       uint32(rank),
		Total:      uint32(total),
	}, nil
}

// ── Lookup (QueryQueueStatus) ─────────────────────────────────────────

// Lookup translates a queue token to its current state.
func (q *Queue) Lookup(ctx context.Context, token string) (*QueueState, error) {
	queueID, err := q.rdb.Get(ctx, tokenIndexKey(token)).Result()
	if errors.Is(err, redis.Nil) {
		// Token-index entry gone (e.g. evicted under memory pressure, or
		// the entry's 1h TTL fired). Surface as EXPIRED so the client
		// restarts from /assign-gate with a fresh queueId.
		recordExpired(0, ExpireReasonMetaGone)
		return &QueueState{Status: StatusExpired}, nil
	}
	if err != nil {
		return nil, fmt.Errorf("lookup token index: %w", err)
	}

	// Admitted? Take the admit token + clean up so a second poll doesn't
	// double-admit (the dispatcher set status='admitted' atomically; here we
	// turn that into a one-shot consume).
	admit, err := q.consumeAdmit(ctx, queueID)
	if err != nil {
		return nil, err
	}
	if admit != nil {
		// Clean up the meta + token index + admitted set entry — the entry's
		// life ends here. We DO NOT remove the queueId from admitted:{zoneId}
		// because dispatcher SREM is the source of truth for "in-flight
		// admitted but not yet consumed"; consumeAdmit handles that.
		return &QueueState{Status: StatusAdmitted, Admit: admit}, nil
	}

	// Still queueing — fetch zone_id from meta to know which ZSET to query.
	zoneIDStr, err := q.rdb.HGet(ctx, metaKey(queueID), "zone_id").Result()
	if errors.Is(err, redis.Nil) {
		// Meta TTL expired faster than token TTL — treat as expired.
		recordExpired(0, ExpireReasonMetaGone)
		return &QueueState{Status: StatusExpired}, nil
	}
	if err != nil {
		return nil, fmt.Errorf("lookup meta zone: %w", err)
	}
	var zoneID uint32
	if n, _ := fmt.Sscanf(zoneIDStr, "%d", &zoneID); n != 1 || zoneID == 0 {
		// Meta payload is corrupt — should never happen since Enqueue
		// writes zone_id under the same pipeline that creates the entry.
		// Surface as EXPIRED so the client restarts cleanly rather than
		// looking up "queue:zone:0" forever (rank would always be 0,
		// dispatcher would never see it, → silent stuck client).
		recordExpired(0, ExpireReasonMetaGone)
		return &QueueState{Status: StatusExpired}, nil
	}

	rank, err := q.rdb.ZRank(ctx, zoneZSetKey(zoneID), queueID).Result()
	if errors.Is(err, redis.Nil) {
		// Member vanished — likely just popped by dispatcher; advise client to
		// retry rather than mark expired.
		return &QueueState{Status: StatusQueueing, Rank: 0}, nil
	}
	if err != nil {
		return nil, fmt.Errorf("zrank during lookup: %w", err)
	}
	total, _ := q.rdb.ZCard(ctx, zoneZSetKey(zoneID)).Result()
	return &QueueState{Status: StatusQueueing, Rank: uint32(rank), Total: uint32(total)}, nil
}

// consumeAdmit atomically reads-and-deletes admit:{queueId}. Returns nil if
// no admit has been issued yet for this queueId. Also SREMs from the
// admitted set so capacity accounting is honest.
func (q *Queue) consumeAdmit(ctx context.Context, queueID string) (*AdmitToken, error) {
	// GETDEL is single-RTT; if the admit was issued under SET NX it still
	// resolves cleanly. If admit:{queueId} didn't exist, GETDEL returns
	// redis.Nil and we treat it as "still queueing".
	val, err := q.rdb.GetDel(ctx, admitKey(queueID)).Bytes()
	if errors.Is(err, redis.Nil) {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("getdel admit: %w", err)
	}

	var admit AdmitToken
	if err := json.Unmarshal(val, &admit); err != nil {
		return nil, fmt.Errorf("unmarshal admit token: %w", err)
	}

	// Best-effort cleanup of capacity accounting + token index. Failures are
	// non-fatal: SREM no-ops if the entry was already evicted, DEL is
	// idempotent, and the meta TTL guarantees eventual cleanup.
	//
	// We do HMGET (not HGET) to grab createdAt and zone_id in one RTT so the
	// wait-time histogram observation below pays no extra Redis cost. Missing
	// createdAt is non-fatal — the wait observation is skipped.
	fields, _ := q.rdb.HMGet(ctx, metaKey(queueID), "zone_id", "createdAt").Result()
	var (
		zoneID    uint32
		createdAt int64
	)
	if len(fields) >= 1 {
		if s, ok := fields[0].(string); ok {
			_, _ = fmt.Sscanf(s, "%d", &zoneID)
		}
	}
	if len(fields) >= 2 {
		if s, ok := fields[1].(string); ok {
			_, _ = fmt.Sscanf(s, "%d", &createdAt)
		}
	}
	if zoneID != 0 {
		q.rdb.SRem(ctx, admittedSetKey(zoneID), queueID)
		if createdAt > 0 {
			waitMs := time.Now().UnixMilli() - createdAt
			recordWait(zoneID, time.Duration(waitMs)*time.Millisecond)
		}
	}
	// We deliberately keep meta + token index alive for their TTL window so a
	// duplicate poll (client retried mid-network) sees StatusExpired rather
	// than re-enqueueing under a fresh queueId.
	return &admit, nil
}

// ── Dispatcher-facing (admit a batch) ─────────────────────────────────

// PopAdmit pops up to n entries from the front of zoneId's queue and writes
// admit:{queueId} for each. Caller (the dispatcher) supplies the per-entry
// token via signFn so this package stays free of gate-token signing logic.
//
// Returns the number of entries actually admitted (may be < n if the queue
// is shorter than expected, or if some signFn calls fail).
func (q *Queue) PopAdmit(
	ctx context.Context,
	zoneID uint32,
	n int,
	signFn func(zoneID uint32, queueID string) (*AdmitToken, error),
) (int, error) {
	if n <= 0 {
		return 0, nil
	}
	zs, err := q.rdb.ZPopMin(ctx, zoneZSetKey(zoneID), int64(n)).Result()
	if err != nil {
		return 0, fmt.Errorf("zpopmin: %w", err)
	}
	if len(zs) == 0 {
		return 0, nil
	}

	admitted := 0
	for _, z := range zs {
		queueID, ok := z.Member.(string)
		if !ok || queueID == "" {
			continue
		}
		admit, err := signFn(zoneID, queueID)
		if err != nil {
			// Couldn't pick a gate or sign the token — the entry is gone from
			// ZSET. Best-effort: re-add at the front so the next dispatcher
			// tick retries. Score = 0 to put it ahead of newcomers.
			q.rdb.ZAdd(ctx, zoneZSetKey(zoneID), redis.Z{Score: 0, Member: queueID})
			continue
		}
		blob, err := json.Marshal(admit)
		if err != nil {
			continue
		}

		pipe := q.rdb.TxPipeline()
		pipe.Set(ctx, admitKey(queueID), blob, q.admitTTL)
		pipe.SAdd(ctx, admittedSetKey(zoneID), queueID)
		pipe.Expire(ctx, admittedSetKey(zoneID), q.admitTTL)
		if _, err := pipe.Exec(ctx); err != nil {
			continue
		}
		admitted++
	}
	return admitted, nil
}

// AdmittedCount returns the in-flight admitted but not-yet-consumed count for
// a zone. Used by capacity calculation so we don't over-issue gate tokens
// while the previous batch is still walking from the client to the gate.
func (q *Queue) AdmittedCount(ctx context.Context, zoneID uint32) (uint32, error) {
	n, err := q.rdb.SCard(ctx, admittedSetKey(zoneID)).Result()
	if err != nil {
		return 0, err
	}
	return uint32(n), nil
}

// QueueLen is a thin wrapper over ZCARD for capacity decisions.
func (q *Queue) QueueLen(ctx context.Context, zoneID uint32) (uint32, error) {
	n, err := q.rdb.ZCard(ctx, zoneZSetKey(zoneID)).Result()
	if err != nil {
		return 0, err
	}
	return uint32(n), nil
}
