# DB Write-Behind Dirty Flag Race Condition Analysis

**Created:** 2026-03-26

## Scheme Description (Write-Behind)

```
External write → Update Redis + mark key as dirty
db service    → Periodically scan dirty key set → Read latest data from Redis → Batch write to MySQL
```

## Core Problem: Race Condition on Dirty Flag Clearing

```
T=0: flusher reads data V2 for key K from Redis
T=1: New write updates K in Redis to V3, marks dirty again
T=2: flusher writes V2 to MySQL, then clears dirty flag
Result: MySQL=V2, Redis=V3, dirty cleared → V3 will never be flushed to MySQL!
```

New data is overwritten by old data, and since the dirty flag has been cleared, V3 is permanently lost.

## Possible Fix: Version Number + Lua Atomic Operation

- Increment a version number on every Redis write
- During flush, use a Lua script to atomically: read data + clear dirty flag only if version matches
- High complexity, introduces distributed race condition management

## Recommended Scheme Comparison

| Scheme | Correctness | Performance | Crash Recovery | Complexity |
|--------|-------------|-------------|----------------|------------|
| Immediate per-row MySQL write (current) | Safest | One IO per row | Kafka replay idempotent | Low |
| External dirty flag + periodic flush | Has race | Batch high throughput | Redis dirty data may be lost | High |
| **Worker-internal batch flush** | Safe | Batch high throughput | Kafka replay idempotent | Medium |

## Best Scheme: Worker-Internal Batch Flush

Leverages the single-goroutine serial guarantee of the existing Kafka partition worker:

```
Kafka partition worker (same key serial, single goroutine)
├─ Receive write msg → Update Redis cache → Add to local dirtyMap
├─ ...
├─ Every N messages or every T seconds → Batch flush dirtyMap to MySQL
└─ Flush success → Clear dirtyMap → MarkMessage
```

**Why it's safe**: Reading Redis, writing MySQL, and clearing the dirty flag all execute serially in the same goroutine — no race condition.

## Scheme A / Scheme B Usage Guidelines

### Scheme A (Recommended Default)

A = **The write requester directly sends a Kafka `Op:"write"` to the db service** (may also update Redis cache simultaneously).

**When to use A:**
- Highest correctness is required; priority is ensuring "new data is never overwritten by old data"
- Current write QPS has not saturated MySQL
- Want to reuse existing Kafka same-key serial and retry capabilities
- Want to rely on Kafka replay after crash to guarantee eventual persistence

**Benefits of A:**
- Simple pipeline: the write source has the latest data, no need for a second read-back from Redis
- Stronger consistency: same-key ordering is naturally guaranteed by Kafka partition + worker
- Clear failure recovery: un-acked messages can be replayed

### Scheme B (Enable Only When Throughput Becomes a Bottleneck)

B = **db worker-internal batch flush** (maintain a dirtyMap within the same worker, batch write to MySQL every N messages or T seconds).

**When to use B:**
- Monitoring confirms MySQL has become a write bottleneck (e.g., p95/p99 write latency consistently exceeds thresholds)
- Need to significantly reduce SQL round-trips and improve throughput
- Can tolerate brief persistence delay (T seconds)

**Prerequisites and Boundaries for B:**
- Must complete "receive write request → update dirtyMap → flush → ack" within the same partition worker
- Must not be changed to an external standalone dirty scanner (would introduce race conditions)
- Requires observability: flush success rate, queue backlog, longest un-persisted time per key

## Discouraged Schemes

- External dirty flag + scheduled scan flush (without version control)
- Reason: Has a race condition where "old value persisted then dirty cleared, causing new value to never be persisted"

## Scheme A Implementation Status (Completed 2026-03-26)

**C++ side (`cpp/libs/services/scene/player/system/player_lifecycle.cpp`):**
- `SavePlayerToRedis()` after saving to Redis, splits `PlayerAllData` into two sub-tables: `player_database` and `player_database_1`
- Each sub-table sends an independent `DBTask(op="write")` to Kafka topic `db_task_topic`
- `msg_type` uses the sub-table proto full_name (`player_database` / `player_database_1`), consistent with the login read path
- `task_id` format: `{playerId}:{tableName}:{millisTimestamp}`, used for log tracing
- Must call `set_player_id()` before serialization (generated marshal does not set the primary key)

**Go side (`go/db/internal/kafka/key_ordered_consumer.go`):**
- `handleDBWriteOp()` is implemented, executes `REPLACE INTO` via `proto_sql.DB.SqlModel.Save(msg)`
- Synchronously updates Redis cache after writing to MySQL
- No changes needed — directly receives write messages sent from C++

**Key Constraints:**
- Cannot send the entire `PlayerAllData` as a DBTask (`Save()` only handles single-table mapping messages)
- Kafka key = playerId string, ensures writes for the same player land on the same partition for serial execution

## Final Conclusion

- **Default to A**: Prioritize correctness and recoverability
- **Switch to B only with clear performance evidence**: and only in the worker-internal batch flush form
- Regardless of A/B, must maintain same-key serial execution and idempotent writes (currently `REPLACE INTO`)
