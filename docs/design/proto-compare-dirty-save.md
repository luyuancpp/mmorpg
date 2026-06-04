# Proto-Compare-Driven Dirty Save — Design (todo.md #204 + #226)

> **Status:** 2026-06-03 — slices A / B / C all landed. Comparison primitive
> (`proto_dirty_compare.h`), `PlayerLastPersistedSnapshotComp`, the
> `SavePlayerToRedis` fast-path, and the `HandlePlayerAsyncSaved`
> snapshot-replace are all in. Slice D (counter / metric) added 2026-06-03
> via `dirty_save_stats.h` + the 30 s LOG_INFO `[DirtySave] total=X
> skipped=Y skip_pct=Z%` line piggy-backed on RedisSystem's queue
> snapshot timer. Stress-test reverification is the remaining work
> (Round 17+).
>
> Original 2026-05-14 status (kept for context): comparison primitive
> landed; integration into `SavePlayerToRedis` was not yet wired.

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

## What landed in this commit (2026-05-14, then 2026-05-17, then 2026-06-03)

| File | Slice | Date | Change |
|---|---|---|---|
| `cpp/libs/engine/core/utils/proto/proto_dirty_compare.h` | primitive | 2026-05-14 | `dirty_save::IsEqual()` and `dirty_save::ShouldPersist()` — header-only wrapper around `MessageDifferencer::Equals` with safe defaults |
| `cpp/libs/services/scene/player/comp/last_persisted_snapshot_comp.h` | A | ~2026-05-17 | `PlayerLastPersistedSnapshotComp` ECS component holding the last persisted `PlayerAllData` |
| `cpp/libs/services/scene/player/system/player_lifecycle.cpp::SavePlayerToRedis` | B | ~2026-05-17 | Fast-path `IsEqual` check at function entry; early return skips Redis Save + both Kafka DBTasks |
| `cpp/libs/services/scene/player/system/player_lifecycle.cpp::HandlePlayerAsyncSaved` | A | ~2026-05-17 | Calls `snap.Replace(message)` on successful save so the next call has a baseline |
| `cpp/libs/services/scene/player/system/player_lifecycle.cpp` (probe order fix) | R2 | 2026-05-17 | `IsEqual` check moved BEFORE `stresstest_probe::Stamp*` so probe writes don't poison the equality |
| `cpp/libs/services/scene/player/system/dirty_save_stats.h` | C | 2026-06-03 | Two atomic counters (`Total`, `Skipped`) read on the 30 s timer |
| `cpp/libs/services/scene/core/system/redis.cpp` | C | 2026-06-03 | `LOG_INFO "[DirtySave] total=X skipped=Y skip_pct=Z%"` emitted every 30 s alongside `LogQueueSnapshot` |
| `tools/scripts/stress_summarize.ps1` | C | 2026-06-03 | Section 2bb parses `[DirtySave]` lines from `run/logs/cpp_nodes/scene_*.log` and prints the latest sample |
| `docs/design/proto-compare-dirty-save.md` | doc | 2026-06-03 | Status updated to reflect actual integration |

---

## What's NOT done yet (tracked as #204 follow-ups)

### A. ~~Snapshot retention component~~ — **DONE 2026-05-17**

`PlayerLastPersistedSnapshotComp` exists; `HandlePlayerAsyncSaved` calls
`snap.Replace(message)` on success.

### B. ~~SavePlayerToRedis fast-path~~ — **DONE 2026-05-17**

`SavePlayerToRedis` has the `IsEqual` early-return at line ~822. Both the
Redis Save and the two `sendSubTableTask` Kafka emits are skipped on a
match — verified by reading the function body, the early return is at the
start of the function so neither happens.

### C. ~~Counter / metric~~ — **DONE 2026-06-03**

Not Prometheus (cpp scene has no metrics endpoint yet); instead two
atomic counters in `dirty_save_stats.h` and a `LOG_INFO` line emitted
every 30 s on the existing `RedisSystem` snapshot timer. Parsed by
`stress_summarize.ps1` as section 2bb so Round 17+ summaries surface
the skip rate alongside SceneManager / DB tables.

### D. Stress reverification — **PENDING (Round 17)**

The hand-rolled `dirty` flags on individual components are now redundant
for the "is this save necessary?" question. Codebase grep:

```
PlayerCurrencyComp::dirty   — runtime-only marker, kept (drives debt
                               persist-vs-skip; not the same as
                               PlayerAllData-level dirty)
PlayerSkill changed flag    — TODO: audit and possibly remove
```

Round 17 should also verify that the `[DirtySave]` skip rate is non-zero
under realistic stress traffic — anything below ~30 % at idle/AFK density
suggests a bug (stress probe order, snapshot Replace not firing, etc.).

---

## Open questions

1. ~~**What about Kafka write tasks?**~~ — **Resolved by slice B**:
   the `SavePlayerToRedis` early return short-circuits the entire
   function, so neither the Redis Save nor the two `sendSubTableTask`
   Kafka emits run on a skip.

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
