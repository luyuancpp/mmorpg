# Proto-Compare-Driven Dirty Save — Design (todo.md #204 + #226)

> **Status:** 2026-05-14 — comparison primitive landed (`proto_dirty_compare.h`);
> integration into `SavePlayerToRedis` is **not yet wired**.
> This document captures why the integration is staged separately.

---

## Problem

Persistence in `SavePlayerToRedis` (`cpp/libs/services/scene/player/system/player_lifecycle.cpp`)
is currently unconditional: every logout / cross-zone transfer / scheduled
save serializes the full `PlayerAllData` and pushes it through Redis +
Kafka regardless of whether anything actually changed.

There are hand-rolled `bool dirty{false}` flags on individual components
(`PlayerCurrencyComp::dirty`, `PlayerSkill` has a `changed` flag, etc.),
but:
1. They are not consulted by `SavePlayerToRedis` itself — that function
   has no per-component dirty fast-path; it serializes everything.
2. Each new mutation path has to remember to flip the flag, which is the
   classic "did I set dirty?" bug — the kind that goes unnoticed until
   someone reports lost data after a server restart.

todo #204 ("save only when changed") and #226 ("use proto Compare to drive
dirty-save") together call out the gap. The two items are the same
problem with different framings; #226 is the implementation choice for
#204.

---

## Decision

### Use `google::protobuf::util::MessageDifferencer::Equals`

Protobuf's official `MessageDifferencer::Equals` walks the field
descriptor tree and compares effective content. It already knows how to
handle the awkward bits:

- proto3 default values vs unset (treated as equal)
- nested messages (recursive)
- `map<K, V>` (unordered comparison since proto 3.5)
- repeated scalars / messages (ordered comparison, which is what we
  want — our repeated fields today represent ordered lists)

This is far better than rolling our own comparator (we'd reinvent every
descriptor edge case) or comparing `SerializeAsString()` outputs (proto3
serialization order is deterministic but **not stable across versions**
and unset-vs-default-zero produce different bytes — we'd get false
positives on a protobuf upgrade).

### Compare against a per-player **last-persisted** snapshot, not against
### the serialized body in Redis

Two natural designs for "what to compare against":

| Approach | Cost | Pros | Cons |
|---|---|---|---|
| Keep last `PlayerAllData` in memory | ~few KB per online player | Sub-100µs comparison; fits in `SavePlayerToRedis` happy path | Memory grows with concurrent online players |
| Hash + compare hash | tiny memory | Cheaper to keep around | Hash mismatch ≠ semantic difference; can't use `MessageDifferencer` |
| Re-fetch from Redis before save | ~ms RTT | Memory-free | Defeats the purpose (save-skip latency > save-always latency) |

**We pick option 1.** A 10K-online-player gate at ~5KB per snapshot is
~50MB — meaningful but well within budget for a modern dedicated game
node, and the alternative defeats the latency win entirely.

The snapshot lives next to the `PlayerSessionSnapshotComp` ECS component
(or as a sibling component, TBD when integration lands). It's updated
atomically when `HandlePlayerAsyncSaved` confirms a successful write.

### Stage the integration

The comparison primitive (`proto_dirty_compare.h`) lands now. The actual
hook into `SavePlayerToRedis` is deferred because the **right** place to
wire it depends on decisions we don't have to make today:

1. **Where to keep the snapshot** — sibling ECS component? TLS hash
   keyed by `entt::entity`? Decision can wait until integration sprint.
2. **Per-table vs full-snapshot** — `SavePlayerToRedis` writes
   `player_database_data` and `player_database_1_data` as two
   independent Kafka tasks. We could compare per-sub-table to skip
   one without skipping the other. Probably worth the extra
   bookkeeping for split-load patterns; not worth pre-deciding.
3. **First-save baseline** — on a fresh login the "last persisted"
   snapshot is unknown. Treat first save as always-dirty (the safe
   default our `ShouldPersist` helper already does), or load Redis
   on entry to seed it (one extra GET per session). Pick the simpler
   default; revisit if profiling shows the empty-snapshot case
   matters.

---

## What landed in this commit (2026-05-14)

| File | Change |
|---|---|
| `cpp/libs/engine/core/utils/proto/proto_dirty_compare.h` | `dirty_save::IsEqual()` and `dirty_save::ShouldPersist()` — header-only wrapper around `MessageDifferencer::Equals` with safe defaults |
| `docs/design/proto-compare-dirty-save.md` | this file |

That's it. The integration into `SavePlayerToRedis` itself is **not
done in this slice** — see "What's NOT done" below.

---

## What's NOT done yet (tracked as #204 follow-ups)

### A. Snapshot retention component — **S, ~1 day**

Add `PlayerLastPersistedSnapshotComp` (or whatever name fits the project's
component naming) that holds the last successfully persisted
`PlayerAllData`. Update it inside `HandlePlayerAsyncSaved` after the save
callback fires successfully, **before** any of the existing
post-save side effects (DestroyPlayer / cross-zone transfer).

### B. SavePlayerToRedis fast-path — **S, ~half day**

At the top of `SavePlayerToRedis`:

```cpp
PlayerAllData current;
PlayerAllDataMessageFieldsMarshal(player, current);

const auto* last = tlsEcs.actorRegistry.try_get<PlayerLastPersistedSnapshotComp>(player);
if (last && dirty_save::IsEqual(current, last->snapshot()))
{
    LOG_DEBUG << "[SavePlayerToRedis] no-op for player " << playerId
              << " — proto-compare clean";
    return;
}
```

Cost: one extra full-tree comparison per save. Order of magnitude
~few hundred microseconds for a typical player; far cheaper than the
Redis + Kafka round trip we're skipping.

### C. Counter / metric — **S, ~half day**

`scene_dirty_save_skipped_total` Prometheus counter so we can prove the
optimization is firing in prod. A first-week sanity check should show
this counter rising during idle/AFK periods (no mutation = nothing to
write) and staying near zero during active gameplay.

### D. Stress reverification — **M, 1 day**

The hand-rolled `dirty` flags on individual components become redundant
once `MessageDifferencer` is in charge of "is this save necessary?"; we
should also verify they're not being relied on for **other** semantics
(e.g. "decide whether to recompute a cache") before deleting them. A
codebase grep for `dirty` membership reads should find them all.

---

## Open questions

1. **What about Kafka write tasks?** `SavePlayerToRedis` issues two
   Kafka DBTasks alongside the Redis Save. If we skip the Redis path
   we should skip those too — same comparison, same answer. Plumb
   the skip decision through; don't let the Kafka path become a
   silent bypass of the dirty check.

2. **Reading after skip.** If we skip a save, the Redis blob remains
   the previous successful save. A subsequent same-node `EnterScene`
   will load that blob — fine, that's exactly what we want. A
   cross-node `EnterScene` after a skip works the same way. **No
   correctness issue from the skip itself, only the assumption that
   "skip == idempotent ↔ no observable change to player state".**
   That equality is what `MessageDifferencer` is for.

3. **Per-table comparison** — split decision, see "Stage the
   integration" above.

---

## References

- `cpp/libs/engine/core/utils/proto/proto_dirty_compare.h` — the
  primitive (this slice)
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp::SavePlayerToRedis`
  — integration target (not yet touched)
- `cpp/libs/modules/currency/comp/player_currency_comp.h::dirty` —
  example of the hand-rolled flag pattern this displaces
- `google::protobuf::util::MessageDifferencer` —
  `<google/protobuf/util/message_differencer.h>` already in build
- todo.md #204, #226 — origin
