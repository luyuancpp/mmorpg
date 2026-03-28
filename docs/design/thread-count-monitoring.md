# Thread Count Monitoring & Control per Process

## Overview

Every C++ node automatically runs a **ThreadMonitor** that periodically samples the process thread count and logs warnings when sustained growth is detected. Go services rely on goroutine scheduling and per-partition worker pools for concurrency control.

---

## C++ Nodes — ThreadMonitor (Automatic, All Nodes)

### Implementation
- **File**: `cpp/libs/engine/core/node/system/node/thread_observability.h`
- **Registration**: `SimpleNode` constructor calls `RegisterThreadObservability()` — enabled by default for every node
- **Mechanism**: Timer on main EventLoop samples thread count every N seconds
  - Windows: `CreateToolhelp32Snapshot` + `Thread32First/Next`
  - Linux: `readdir("/proc/self/task")`
- **Behavior**: Passive monitoring only — logs warnings, does **NOT** enforce hard limits or kill threads

### Environment Variables

| Env Var | Default | Description |
|---------|---------|-------------|
| `NODE_THREAD_MONITOR_ENABLED` | `1` (on) | Set `0` / `false` / `off` to disable |
| `NODE_THREAD_MONITOR_SAMPLE_INTERVAL_SECONDS` | `10` | How often to sample (seconds) |
| `NODE_THREAD_MONITOR_STABLE_WINDOW_SECONDS` | `60` | Wait this long before setting baseline |
| `NODE_THREAD_MONITOR_GROWTH_WARN_CONSECUTIVE_SAMPLES` | `6` | Consecutive growth samples before warning |
| `NODE_THREAD_MONITOR_GROWTH_WARN_ABSOLUTE_INCREASE` | `16` | Min absolute increase from baseline to trigger warning |

### Log Output Examples

```
[INFO]  Gate thread metrics: total_threads=12, delta_threads=0, peak_threads=14, uptime_sec=120
[INFO]  Gate thread baseline (stable window reached): baseline_threads=12, ...
[WARN]  Gate thread growth warning: total_threads=30, increase_from_baseline=+18, consecutive_growth_samples=7, ...
```

---

## C++ Thread Pools by Node Type

### Gate Node — Expected ~5–14 threads

| Thread Category | Count | Config Env Var | Notes |
|---|---|---|---|
| muduo EventLoop (main) | 1 | — | Single reactor loop |
| gRPC ResourceQuota max threads | 2 (default) | `GRPC_MAX_THREADS` | Hard limit on gRPC server thread pool |
| gRPC EventEngine reserve | 2 (debug profile) | `GRPC_THREAD_POOL_RESERVE_THREADS` | Pre-allocated threads |
| gRPC EventEngine max | 8 (debug profile) | `GRPC_THREAD_POOL_MAX_THREADS` | Hard cap on event engine threads |
| Kafka consumer (librdkafka) | 2–4 | librdkafka internal | Opaque; not directly configurable |

- gRPC env config utility: `cpp/libs/engine/core/node/system/grpc_channel_cache.h`
- Debug profile env vars set in: `cpp/nodes/gate/gate.vcxproj` `<LocalDebuggerEnvironment>`

### Scene Node — Expected ~3–5 threads

| Thread Category | Count | Config Env Var | Notes |
|---|---|---|---|
| muduo EventLoop (main, runs `World::Update`) | 1 | — | Game simulation loop |
| Kafka consumer (librdkafka) | 2–4 | librdkafka internal | Opaque |

Scene node does not host a gRPC server; fewer threads than Gate.

---

## Go Services — Goroutine-Based

### General Pattern

- All Go services use go-zero framework; each gRPC/HTTP request runs in its own goroutine
- Goroutine scheduling bound by `GOMAXPROCS` (defaults to `runtime.NumCPU()`)
- No explicit goroutine pool limits configured currently

### db Service — Kafka Partition Workers

- **File**: `go/db/internal/kafka/key_ordered_consumer.go`
- 1 worker goroutine per Kafka partition (`Config.Kafka.PartitionCnt`)
- Each worker has task channel buffer of 1000
- This is the only Go service with structured concurrency control

### login Service

- etcd KeepAlive goroutine (1 per registered node)
- NodeWatcher goroutine (1) for etcd-based service discovery
- TaskManager goroutine (1) for expired batch cleanup

### Other Go Services (scene_manager, data_service, player_locator)

- Standard go-zero request-per-goroutine model
- Kafka producers use fire-and-forget (segmentio/kafka-go); minimal extra goroutines

---

## Key Design Points

1. **C++ monitoring is passive** — logs and warns but does not kill or cap threads.
2. **gRPC thread cap is the only hard limit** — `GRPC_MAX_THREADS` (default 2) caps the server-side thread pool via `ResourceQuota`.
3. **Go services have no goroutine hard limit** — rely on OS thread scheduling via `GOMAXPROCS`.
4. **Kafka internal threads are opaque** — librdkafka manages its own threads (typically 2–4 per consumer instance).
5. **To enforce hard limits in production**, options include:
   - gRPC `ResourceQuota` (already in use)
   - Go `runtime.SetMaxThreads()` for OS-level thread cap
   - K8s cgroup CPU/memory limits (indirectly constrains thread creation)

---

## File Reference

| File | Purpose |
|------|---------|
| `cpp/libs/engine/core/node/system/node/thread_observability.h` | ThreadMonitor class, env var parsing, `RegisterThreadObservability()` |
| `cpp/libs/engine/core/node/system/node/simple_node.h` | Auto-registers ThreadMonitor for all SimpleNode instances |
| `cpp/libs/engine/core/node/system/grpc_channel_cache.h` | gRPC thread config readers (`GRPC_MAX_THREADS`, etc.) |
| `cpp/nodes/gate/gate.vcxproj` | Debug env vars for gRPC thread pool |
| `go/db/internal/kafka/key_ordered_consumer.go` | Kafka partition worker pool (structured goroutine control) |
