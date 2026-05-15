# Cross-Server Currency Deduction Authorization — Design (todo.md #207)

> **Status:** 2026-05-14 — design only. No code in this slice.
> Implementation is blocked on #48 (cross-server reconnect),
> which itself is a Not-Done P1.

---

## Problem

Currency mutation today is local to the scene node hosting the player.
The clawback system (#59 — `CurrencySystem::AttachDebt` +
`anomaly_detector.cpp` + `recall_logic.go`) catches abuse **after the
fact** by recording a debt and deducting from future income. That's
the right safety net.

What's missing is **prevention** when a player triggers a deduction
that spans servers — e.g. buying an item from a cross-server auction,
sending mail with currency attached to a player in another zone,
joining a cross-zone instance with a currency deposit. In those
flows, today's code would just deduct locally and trust the cross-
server delivery; if the cross-server delivery fails after the local
deduct, the player loses currency with no audit trail beyond the
generic transaction log.

todo #207 calls out the gap: there should be a **two-phase commit
style** authorization step before the actual deduct, so the cross-
server destination acknowledges receipt before the source commits.

---

## Why this is design-only in this slice

Three honest reasons:

1. **#48 cross-server reconnect doesn't exist.** A cross-server
   deduction implicitly assumes both source and destination are
   currently routable for the player. The reconnect path that would
   keep them routable across zone boundaries isn't built. Implementing
   #207's commit chain without #48 means writing code that can't be
   exercised; it would rot before it ran.

2. **Cross-server architecture is partial.** `cross_server_architecture_principle.md`
   defines the principle (storage-only per zone) and `cross_scene_player_messaging.md`
   defines the message routing, but neither has the cpp-side delivery
   acknowledgement primitive that two-phase commit requires.
   See todo #208 (cross-scene-server player messaging, 🟡) — the
   underlying delivery layer is itself partial.

3. **Existing clawback (#59) is sufficient for v1.** Before cross-
   server commerce features actually ship to players, the post-hoc
   recall + alert path catches every problem. We don't need
   prevention until prevention is cheaper than recall, and that
   crossover happens once specific cross-server flows go live.

So this slice records the design and the dependencies; code waits.

---

## Decision (when implemented)

### Two-phase commit, sequenced through Kafka

```
SOURCE NODE                                   DESTINATION NODE
                                              (target zone's data_service)

[1] AttachPendingDeduct(player, type, amount)
    ↓ Redis HSET pending_deducts:{player_id}:{tx_id} = {type, amount, ts}
    ↓ Block currency operations on this {player, type} via lock
[2] Send CurrencyAuthRequest via Kafka
    → key=player_id (one-partition order guarantee)
                                              [3] Receive CurrencyAuthRequest
                                                  ↓ Check destination side can
                                                    accept (e.g. recipient
                                                    inbox not full, target
                                                    instance hasn't been
                                                    deleted)
                                              [4] Send CurrencyAuthResponse
                                                  → key=player_id
[5] Receive CurrencyAuthResponse
    On success:
      ↓ CurrencySystem::Deduct (real, atomic)
      ↓ Redis HDEL pending_deducts:{player_id}:{tx_id}
      ↓ Unlock
    On failure:
      ↓ Redis HDEL pending_deducts:{player_id}:{tx_id}
      ↓ Send refusal to client
      ↓ Unlock
```

### Idempotency via tx_id

Every authorization request carries a `tx_id` (UUID-style, see
`go/shared/snowflake/`). Destination keeps a short-TTL Redis set of
seen tx_ids; a duplicate (e.g. Kafka redeliver) returns the prior
response without re-running side effects. Source uses the same tx_id
to match request and response.

### Recovery on source-node crash mid-flight

If the source crashes between step 1 (pending recorded) and step 5
(commit/rollback), the pending entry stays in Redis. A periodic
sweep on the source node — same eventloop as the
`pending_save_queue_` sweep in `redis_client.h` — finds entries older
than the auth timeout (~5s) and either re-sends the auth or marks
them as orphaned for manual review. The locking on {player, type}
keeps other deducts safe during this window.

### Failure modes worth listing

| Failure | Recovery |
|---|---|
| Source crashes after [1], before [2] | Sweep finds pending entry; re-sends [2] using same tx_id |
| Destination crashes after [3], before [4] | Source times out at ~5s; re-sends [2]; destination's tx_id set deduplicates |
| Destination crashes after [4], before delivering response | Source times out; re-sends [2]; tx_id dedup returns the prior response |
| Source crashes after [5] but before Redis HDEL | Sweep finds entry, sees tx_id is in destination's done-set, completes cleanup |
| Kafka loses the request/response | Source timeout retry; tx_id dedup |
| Player disconnects mid-flight | Tx continues; refund (if any) credits an offline-mail inbox |

### Audit trail integration with #59 clawback

Every step writes to `transaction_log_system.h::TransactionLog`:
- Step 1: `kAttachPendingDeduct`
- Step 5 commit: `kCommitDeduct`
- Step 5 rollback: `kRollbackDeduct`
- Sweep cleanup: `kOrphanCleanup`

The clawback path (`AnomalyDetector::Check`) already reads these
logs; adding the pending lifecycle gives it the data to differentiate
"player attempted a cross-server deduct that failed" from "player
exploited a race."

---

## What landed in this commit (2026-05-14)

| File | Change |
|---|---|
| `docs/design/cross-server-currency-auth.md` (this file) | Decision record + slice plan. |

**No code.** The slice writes the design while the dependency stack
(#48, #208) is still incomplete; locking in an implementation now
would mean refactoring it later when those gaps fill.

---

## What's NOT done (slices to track, in dependency order)

### Predecessor: #48 cross-server reconnect — **L, separate todo**

Until a player can re-attach to a zone after a crash / lease loss,
cross-server transactions can't recover from the "player went offline
mid-flight" case cleanly. Refund-via-offline-mail is a workaround,
not a fix.

### Predecessor: #208 cross-scene-server messaging — **L → M (partial)**

The cpp-side scaffolding for direct cross-scene messaging exists
(`route_message_response_handler.cpp` + `cross_scene_player_messaging.md`)
but the design doc lists many TODOs around delivery ack and ordered
delivery. Both are needed for #207's two-phase commit.

### A. Pending-deduct Redis layout + helper — **S, ~1 day** (once predecessors done)

```cpp
// cpp/libs/modules/currency/system/cross_zone_pending.h
namespace cross_zone_pending {
    // Reserve amount on source; returns tx_id.
    std::optional<TxId> AttachPending(playerId, CurrencyType, Amount);

    // Commit reserved amount (called on auth success).
    bool Commit(playerId, TxId);

    // Release reserved amount (called on auth failure / timeout).
    bool Rollback(playerId, TxId);

    // For the sweep loop.
    std::vector<PendingEntry> ListOlderThan(std::chrono::seconds age);
}
```

### B. Proto messages — **S, ~half day**

```protobuf
// proto/currency/cross_zone_auth.proto
message CrossZoneAuthRequest {
    int64 player_id = 1;
    string tx_id = 2;
    uint32 currency_type = 3;
    uint64 amount = 4;
    uint32 destination_zone = 5;
    string purpose = 6;   // free-form for audit, e.g. "auction_buy"
}

message CrossZoneAuthResponse {
    string tx_id = 1;
    enum Result { OK = 0; REJECT = 1; ERROR = 2; }
    Result result = 2;
    string reject_reason = 3;
}
```

### C. Kafka topic + producer/consumer — **M, 1 day**

Reuse `KeyOrderedKafkaProducer` (per CLAUDE.md §5 — already keyed by
player_id to the same partition, which is exactly what 2PC needs to
keep request and response ordered).

Topic name: `cross-zone-currency-auth-{zone_id}` so each zone has its
own destination topic. Source sends to the destination's topic;
destination sends responses back to the source's topic.

### D. Sweep timer on source — **S, ~half day**

Hook into the existing `eventLoop->runEvery` infrastructure used by
`thread_observability.h::RegisterThreadObservability`. Periodic
(every 1s) check of Redis for `pending_deducts` older than 5s,
re-send or mark orphaned.

### E. Audit log integration — **S, ~half day**

Extend `transaction_log_system.h` enums to include the four new
states. The clawback `AnomalyDetector` reads from the same store,
so no separate wiring needed.

### F. Stress test — **M, 1 day**

Specifically test the failure modes table above. Each row gets a
chaos-injected reproduction (kill -9 the source at step 4, etc.) and
a check that the player's balance + audit log end up consistent.

---

## Open questions

1. **What's the lock granularity?** Per `{player, currency_type}` keeps
   throughput high (different currencies don't block each other) but
   adds complexity. Per `player` is simpler. Pick when slice A
   lands; depends on whether we ever have multiple in-flight cross-
   zone deducts for the same player.

2. **Auth timeout — 5s right?** Optimistic. Could be tighter for
   intra-region cross-zone (~1s) and looser for cross-region (10s+).
   Make it a config knob, default 5s.

3. **What about cross-server item / mail attachments?** Same 2PC
   pattern applies to any cross-server irrevocable mutation, not just
   currency. Generalize the primitive (`CrossZonePending<Resource>`)
   when slice A lands so we don't reinvent it for items.

4. **Idempotency window.** Destination's seen-tx_id set needs a TTL.
   Too short → Kafka redelivery after the window causes a duplicate
   deduct. Too long → memory growth. Recommend 5x the auth timeout
   (25s default).

---

## References

- `cpp/libs/modules/currency/comp/currency_comp.h` — local deduct path
- `cpp/libs/modules/transaction_log/transaction_log_system.h` — audit
  trail that this design hooks into
- `cpp/libs/modules/transaction_log/anomaly_detector.cpp` — #59
  clawback consumer of the audit trail
- `go/data_service/internal/logic/recall_logic.go` — clawback recall
- `cpp/nodes/gate/rpc_replies/route_message_response_handler.cpp` —
  cross-node messaging foundation
- todo.md #207, #59 (post-hoc clawback — pre-existing), #48
  (cross-server reconnect — blocker), #208 (cross-scene messaging —
  blocker)
