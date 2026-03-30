# Traffic Statistics Design

## Goal

Provide per-message-type traffic statistics for both C++ nodes and Go services, covering:
- Total bytes per message type
- Message count per type
- Messages/second (frequency)
- Bytes/second (throughput)
- Max single message size (detect outliers)
- Separate inbound/outbound tracking
- Periodic summary logging (not per-message)

The system should be safe to enable temporarily in production and always-on during stress tests.

## Current State (C++)

File: `cpp/libs/engine/core/network/game_channel.cpp`

Existing implementation:
- `gMessageStatistics[kMaxRpcMethodCount]` — per message_id stats
- Feature switch via `gFeatureSwitches[kTestMessageStatistics]`
- `StartMessageStatistics()` / `StopMessageStatistics()` methods

**Problems with current implementation:**
1. **Thread safety**: `gMessageStatistics` and `gTotalFlow` have no synchronization. Multiple event-loop threads can call `LogMessageStatistics` concurrently → data race.
2. **Per-message LOG_INFO**: Every single message triggers a log line. Under load this produces millions of log lines/second, saturating disk I/O. This is the primary reason it's unsafe for production.
3. **Only outgoing tracked**: `LogMessageStatistics` is called in `SendRpcRequestMessage` and `SendRpcResponseMessage`, but not in `HandleRpcMessage` (incoming).
4. **No frequency metric**: `flow_rate_second` is bytes/sec, not messages/sec.
5. **Average-since-start**: `flow_rate_total / total_duration` becomes less meaningful over time. A rolling window or periodic reset is better.
6. **No max message size**: Can't detect sporadic oversized messages.

## Design

### Architecture

```
┌────────────────────────────────────────────────────────────┐
│                    Feature Switch                          │
│  C++: gFeatureSwitches[kTestMessageStatistics]             │
│  Go:  atomic.Bool (toggled via admin RPC or env var)       │
└──────────────────────┬─────────────────────────────────────┘
                       │
        ┌──────────────┼──────────────────┐
        ▼                                 ▼
 ┌──────────────┐                ┌────────────────┐
 │  C++ Stats   │                │   Go Stats     │
 │  (atomic)    │                │   (atomic)      │
 │              │                │                 │
 │ Per msg_id:  │                │ Per fullMethod: │
 │  send_count  │                │  recv_count     │
 │  recv_count  │                │  recv_bytes     │
 │  send_bytes  │                │  resp_bytes     │
 │  recv_bytes  │                │  latency_sum    │
 │  max_size    │                │  max_size       │
 └──────┬───────┘                └───────┬─────────┘
        │                                │
        ▼                                ▼
 ┌──────────────┐                ┌────────────────┐
 │ Periodic     │                │ Periodic       │
 │ Reporter     │                │ Reporter       │
 │ (30s timer)  │                │ (30s ticker)   │
 │ Top-N dump   │                │ Top-N dump     │
 └──────────────┘                └────────────────┘
```

### C++ Changes

**Counter structure** (replace proto-based `MessageStatistics` with atomic struct):

```cpp
struct alignas(64) MessageTrafficStats {  // cache-line aligned
    std::atomic<uint64_t> sendCount{0};
    std::atomic<uint64_t> recvCount{0};
    std::atomic<uint64_t> sendBytes{0};
    std::atomic<uint64_t> recvBytes{0};
    std::atomic<uint32_t> maxMessageSize{0};
};
```

**Recording points:**
- `SendRpcRequestMessage()` / `SendRpcResponseMessage()` → `RecordSend(messageId, byteSize)`
- `HandleRpcMessage()` → `RecordRecv(messageId, byteSize)`

**Periodic reporter:**
- Timer fires every 30 seconds (configurable via env `NODE_TRAFFIC_STATS_INTERVAL_SECONDS`)
- Snapshot + reset all counters (swap to 0, compute delta)
- Log a summary: top-N message types by bytes, total throughput, total frequency
- Only non-zero entries are logged

**Thread safety:**
- All counters are `std::atomic<uint64_t>` with `memory_order_relaxed` — sufficient for stats (no ordering requirement)
- `maxMessageSize` uses `compare_exchange_weak` loop
- No mutex needed

**Auto-disable safety:**
- Optional env var `NODE_TRAFFIC_STATS_AUTO_DISABLE_MINUTES` (e.g., 60) — auto-stops after N minutes

### Go Changes

**New shared package: `go/shared/grpcstats/`**

gRPC `UnaryServerInterceptor` that:
1. Checks `enabled` atomic bool
2. Records method name, request size, response size, latency
3. Uses `sync.Map` keyed by full method name → `*MethodStats` (atomic counters)

**Periodic reporter:**
- Background goroutine, 30-second ticker
- Logs top-N methods by total bytes
- Resets counters per window

**Integration:**
- Each Go service adds `grpcstats.UnaryServerInterceptor()` via `server.AddUnaryInterceptors()`
- Toggle: admin RPC, env var `GRPC_TRAFFIC_STATS_ENABLED=1`, or config field

### Metrics Collected

| Metric | C++ | Go | Per-Window | Cumulative |
|--------|-----|----|------------|------------|
| Message count (send) | ✓ | — | ✓ | — |
| Message count (recv) | ✓ | ✓ | ✓ | — |
| Bytes (send) | ✓ | ✓ (resp) | ✓ | — |
| Bytes (recv) | ✓ | ✓ (req) | ✓ | — |
| Messages/sec | ✓ | ✓ | ✓ | — |
| Bytes/sec | ✓ | ✓ | ✓ | — |
| Max message size | ✓ | ✓ | ✓ | — |
| Method latency (p50/p99) | — | ✓ | ✓ | — |

### Production Safety Analysis

| Concern | Risk | Mitigation |
|---------|------|------------|
| CPU overhead | Very low | `atomic::fetch_add` with relaxed ordering ≈ 1 ns per op |
| Memory | Fixed | C++: 118 × 64 bytes ≈ 7.4 KB. Go: proportional to distinct method count |
| Disk I/O (logging) | **Was high** | Now periodic (1 log batch per 30s, not per-message) |
| Lock contention | None | Atomic-only in C++; atomic + sync.Map in Go |
| Auto-disable | Optional | `NODE_TRAFFIC_STATS_AUTO_DISABLE_MINUTES` prevents forgotten-on |

**Verdict**: Safe to enable temporarily in production. The overhead is negligible (a few atomic increments per message). The only real cost is the periodic log output, which is ~1 KB per 30 seconds.

### Toggle Methods

1. **Feature switch** (existing): `gFeatureSwitches[kTestMessageStatistics]` — set programmatically
2. **Environment variable**: `NODE_TRAFFIC_STATS_ENABLED=1` / `GRPC_TRAFFIC_STATS_ENABLED=1`
3. **Admin RPC** (future): Add a `ToggleTrafficStats` RPC to admin service
4. **Auto-timeout**: Set duration limit to auto-disable

### Log Output Format

**C++ (every 30s):**
```
[TrafficStats] window=30s total_send=42000msg/125MB total_recv=38000msg/98MB
  top send: #52 PlayerMove 18000msg 45MB 600msg/s 1.5MB/s max=2.1KB
  top send: #17 ChatMsg    8000msg  12MB 267msg/s 400KB/s max=4.0KB
  top recv: #48 Login      200msg   1MB  7msg/s   33KB/s  max=8.0KB
  ...
```

**Go (every 30s):**
```
[grpcstats] window=30s
  /login.ClientPlayerLogin/Login  recv=200 req_bytes=1.2MB resp_bytes=0.8MB latency_avg=12ms max_req=8KB
  /scene.Scene/EnterScene         recv=150 req_bytes=0.5MB resp_bytes=0.3MB latency_avg=5ms  max_req=2KB
  ...
```
