# Thread Count Monitoring & Control per Process

## Overview

Every C++ node automatically runs a **ThreadMonitor** that periodically samples the process thread count and logs warnings when sustained growth is detected. Go services rely on goroutine scheduling and per-partition worker pools for concurrency control.

A single muduo EventLoop means **single-threaded ownership of business state**,
not a single-threaded process. Kafka, logging, and lifecycle components have
background threads. gRPC does not: since the muduo-driven gRPC migration
(batches 3 and 4, `docs/design/grpc-muduo-transport.md` §14–§15) the generated
gRPC clients and the scene / battle gRPC servers run on the node's own EventLoop
through `muduo_grpc` and create no threads. The official gRPC library is still
linked, but its runtime must never be initialized in a node process; a runtime
guard checks this (see [gRPC Runtime Guard](#grpc-runtime-guard)).

---

## C++ Nodes — ThreadMonitor (Automatic, All Nodes)

### Implementation
- **File**: `cpp/libs/engine/core/node/system/node/thread_observability.h`
- **Registration**: Node initialization calls `RegisterThreadObservability()` — enabled by default for every node
- **Mechanism**: Timer on main EventLoop samples thread count every N seconds
  - Windows: `CreateToolhelp32Snapshot` + `Thread32First/Next`
  - Linux: `readdir("/proc/self/task")`
- **Behavior**: Passive monitoring only — logs warnings, does **NOT** enforce hard limits or kill threads
- **gRPC runtime guard**: checks at registration and on every sample that the official gRPC runtime is still uninitialized (see below)

### Environment Variables

| Env Var | Default | Description |
|---------|---------|-------------|
| `NODE_THREAD_MONITOR_ENABLED` | `1` (on) | Set `0` / `false` / `off` to disable |
| `NODE_THREAD_MONITOR_SAMPLE_INTERVAL_SECONDS` | `10` | How often to sample (seconds) |
| `NODE_THREAD_MONITOR_STABLE_WINDOW_SECONDS` | `60` | Wait this long before setting baseline |
| `NODE_THREAD_MONITOR_GROWTH_WARN_CONSECUTIVE_SAMPLES` | `6` | Consecutive growth samples before warning |
| `NODE_THREAD_MONITOR_GROWTH_WARN_ABSOLUTE_INCREASE` | `16` | Min absolute increase from baseline to trigger warning |
| `NODE_GRPC_RUNTIME_GUARD_FATAL` | unset (off) | `1` / `true` / `on`: the gRPC runtime guard exits the process (`LOG_FATAL`) on first detection instead of logging. Set it in CI and smoke environments; leave it unset in production (fail-open by design) |

### Log Output Examples

```
[INFO]  Gate thread metrics: total_threads=12, delta_threads=0, peak_threads=14, uptime_sec=120, grpc_runtime_initialized=0
[INFO]  Gate thread baseline (stable window reached): baseline_threads=12, ...
[WARN]  Gate thread growth warning: total_threads=30, increase_from_baseline=+18, consecutive_growth_samples=7, ...
[ERROR] Scene official gRPC runtime is initialized (grpc_is_initialized()=1, detected_at=sample, uptime_sec=3610): its threads (...) are not controlled by the node, ...
```

The `thread metrics` line is logged on the first sample and then at most once
per hour. The `ERROR` line only appears when the guard below fires, and at most
once per process.

### gRPC Runtime Guard

The owner's goal is a controllable thread count: the gRPC library may stay
linked, but no official gRPC thread may start. Once the official runtime is
initialized (an explicit `grpc_init()`, or implicitly by constructing an
official object derived from `grpc::internal::GrpcLibrary`: a channel from
`grpc::CreateChannel`, a server from `grpc::ServerBuilder::BuildAndStart`,
`grpc::CompletionQueue`, channel or server credentials, `grpc::ResourceQuota`,
`grpc::Alarm`; a `*.grpc.pb.h` stub implies it too, since it needs a channel),
it starts EventEngine workers, a Lifeguard, and a timer manager that the node
does not control.

- **Probe**: `cpp/libs/engine/core/node/system/node/grpc_runtime_probe.{h,cpp}`,
  `node::observability::IsGrpcRuntimeInitialized()` wraps
  `grpc_is_initialized()`. The header includes no gRPC header; only the `.cpp`
  includes `<grpc/grpc.h>`. The first call in a process runs gRPC's one-time
  basic setup (an internal mutex, `GRPC_*` environment variables); that setup
  starts no thread and does not count as initialization, so the probe never
  turns its own result true.
- **When it checks**: once in `RegisterThreadObservability()` (independent of
  `NODE_THREAD_MONITOR_ENABLED`) and on every ThreadMonitor sample (only while
  the monitor is enabled).
- **What it reports**: the first time the runtime is found initialized, one
  `ERROR` line `<Node> official gRPC runtime is initialized (grpc_is_initialized()=1, detected_at=registration|sample, uptime_sec=N): ...`
  with troubleshooting hints. The hourly `thread metrics` line always carries
  `grpc_runtime_initialized=0|1`.
- **Fail-open by design** (AGENTS.md §11.3): the guard only logs; it neither
  stops the process nor rejects traffic. An initialized runtime affects thread
  controllability, an operational property, not player assets, data
  consistency, or security; taking a live scene / battle / gate down for it
  would cost far more. The risk is covered by the ERROR line, the
  `grpc_runtime_initialized` field, and the regression test below.
- **Regression test**: `cpp/tests/rpc_controller_test/grpc_runtime_not_initialized_test.cpp`
  drives a generated-client local reject and a real `muduo_grpc` loopback call
  (generated client → `muduo_grpc::Channel` → in-process `muduo_grpc::Server` →
  `GrpcUnaryCall`) and asserts that the runtime is still uninitialized, also
  after teardown. No test in the `rpc_controller_test` process may construct
  official gRPC objects: the state is process-wide and `grpc_shutdown` may
  finish asynchronously on a background thread.

How to check a node:

1. Logs: no `official gRPC runtime is initialized` line; the hourly
   `thread metrics` line shows `grpc_runtime_initialized=0`.
2. Compare `thread baseline (stable window reached): total_threads=` across
   builds and hosts; it must not scale with the CPU core count any more.
3. Linux: `for t in /proc/<pid>/task/*; do cat $t/comm; done | sort | uniq -c`
   must not show official gRPC thread names (`comm` keeps 15 characters):
   `event_engine`, `lifeguard`, `grpc_global_tim`, `grpcpp_sync_ser`,
   `grpc_shutdown`.
4. Code: `rg -n --type cpp "grpcpp|\bgrpc::|grpc_init\(" cpp --glob '!cpp/generated/**'`
   should only hit comments and diagnostic strings (the guard's own ERROR text
   in `thread_observability.h` and the regression test's failure message);
   without `--type cpp` it also hits the `grpcpp_*.lib` link entries in the
   `.vcxproj` files. `rg -n --type cpp "#include <grpc" cpp --glob '!cpp/generated/**'`
   should only hit `grpc_runtime_probe.cpp`, the only handwritten file that
   includes an official gRPC header.

---

## C++ Runtime Thread Composition

A fixed process-wide range such as “5–14 threads” is still not valid:
librdkafka, logging, and lifecycle workers are implementation-dependent, so
production baselines must come from ThreadMonitor measurements. What changed
with batches 3–4 is that gRPC no longer contributes any thread. Before, the
default gRPC EventEngine alone eagerly started `Clamp(gpr_cpu_num_cores(), 4, 16)`
workers plus a Lifeguard (17 threads on a host with 16 or more logical CPUs)
and could grow under backlog, nodes with a gRPC server added up to 8 sync
pollers, and shutdown used a temporary worker thread.

| Thread Category | Count / Limit | Configuration | Notes |
|---|---|---|---|
| muduo EventLoop (business main thread) | 1 | — | Owns ECS and serialized business-state changes; also drives the `muduo_grpc` client channels and server and the muduo_h2 etcd transport |
| gRPC clients and servers (`muduo_grpc`, h2c over nghttp2) | 0 extra | — | Run on the node's EventLoop; server limits (256 streams per connection, 1024 in-flight calls per node, at most 64 dispatches / 2ms per loop turn) are in `grpc-muduo-transport.md` §15 |
| Official gRPC runtime (EventEngine, Lifeguard, timer manager, sync server pollers) | 0 — must stay uninitialized | — | The library is still linked, but nothing may initialize its runtime; enforced by the [gRPC Runtime Guard](#grpc-runtime-guard) and its regression test |
| Kafka consumer / librdkafka | implementation-dependent | librdkafka internal | Background threads; measure rather than assume a fixed count |
| Async logging and lifecycle workers | component-dependent | component-specific | Includes the Agones lifecycle worker on scene / battle where enabled |
| OS / runtime threads | platform-dependent | — | For example Windows loader thread-pool workers, which may exit on their own; measure, do not assume |

Retired or inert settings:

- `GRPC_SERVER_MAX_POLLERS` is retired since batch 4: there are no sync
  pollers. Node ignores it and logs one `WARN` if it is set (deployment-script
  handling: `grpc-muduo-transport.md` §15.11).
- `GRPC_MAX_THREADS` configured the `ResourceQuota` of the official client
  channels in `GrpcChannelCache`, which batch 3 deleted; nothing reads it now.
- `GRPC_THREAD_POOL_RESERVE_THREADS` and `GRPC_THREAD_POOL_MAX_THREADS` were
  never public controls for gRPC v1.83's built-in EventEngine. With the
  official runtime uninitialized there is nothing for them to configure; do not
  use any of these names for capacity planning.

### Gate Node

Gate keeps business-state ownership separate from transport work. Since batch 3
its gRPC client channels (router forwarding and the other generated clients)
are `muduo_grpc::Channel` instances on the gate EventLoop, so creating them no
longer initializes the official gRPC runtime or its EventEngine. Kafka and
logging still add their own background threads.

### Scene and Battle Nodes

Scene owns ECS and `World::Update` on one muduo EventLoop. Scene and battle also
host the control-plane gRPC server, which since batch 4 is a
`muduo_grpc::Server` on that same EventLoop: connections are accepted, HTTP/2
is decoded, and calls are dispatched to the generated handlers directly on the
loop. There are no sync pollers and no `runInLoop` plus promise/future hand-off;
waits such as the Agones allocation permit continue asynchronously on the loop
(`AcquireAllocationPermitAsync`). Still, “Scene logic is single-threaded” must
never be read as “the Scene process has only one thread”: Kafka, logging, and
the Agones lifecycle worker have their own threads.

### gRPC Server Shutdown

Shutdown also runs entirely on the EventLoop (`grpc-muduo-transport.md` §15.10).
There is no shutdown worker thread, and `~Node` no longer joins one.
`BeginShutdown` stops listening, sends a two-phase GOAWAY, and keeps serving the
calls already received. When the 2s drain timeout expires, calls whose business
part has not started are refused (`REFUSED_STREAM`) and started calls complete
with `UNAVAILABLE "server shutting down"`; the remaining connections get a 1s
close grace, so the drain ends after about 3s at most. The log shows
`gRPC server draining` and then `gRPC server drained. drain_ms=`.

Handlers are synchronous on the loop, so a handler that is already running
finishes before any drain timer can fire; the drain bound applies to calls that
are queued or waiting asynchronously (for example on an Agones permit).

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
2. **Business single-threading is an ownership rule** — it does not describe the total process thread count.
3. **gRPC creates no threads** — `muduo_grpc` clients and servers run on the node's EventLoop; the official gRPC library stays linked, but its runtime must never be initialized.
4. **The official runtime is guarded at runtime** — checked at registration and on every sample; the first detection logs one `ERROR`, the hourly metrics line carries `grpc_runtime_initialized`, and the guard is fail-open by design (AGENTS.md §11.3); CI and smoke environments set `NODE_GRPC_RUNTIME_GUARD_FATAL=1` to make it fail-closed.
5. **`GRPC_SERVER_MAX_POLLERS` and `GRPC_MAX_THREADS` are retired** — the sync server pollers and client `ResourceQuota` they configured no longer exist.
6. **Go services have no goroutine hard limit** — they rely on scheduling via `GOMAXPROCS`; `runtime.SetMaxThreads()` concerns OS threads, not goroutine count.
7. **Kafka internal threads are implementation-managed** — production baselines must come from ThreadMonitor measurements.
8. **K8s cgroup CPU/memory limits do not directly cap thread count** — use monitoring and component-specific supported controls rather than assuming an indirect hard limit.

---

## File Reference

| File | Purpose |
|------|---------|
| `cpp/libs/engine/core/node/system/node/thread_observability.h` | ThreadMonitor class, env var parsing, gRPC runtime guard, `RegisterThreadObservability()` |
| `cpp/libs/engine/core/node/system/node/grpc_runtime_probe.{h,cpp}` | `IsGrpcRuntimeInitialized()` (wraps `grpc_is_initialized()`); the header includes no gRPC header |
| `cpp/libs/engine/core/node/system/node/node.cpp` | Registers ThreadMonitor; starts the `muduo_grpc` server and runs its in-loop shutdown drain |
| `cpp/libs/engine/muduo_grpc/` | muduo-driven gRPC client channels and server (no threads) |
| `cpp/tests/rpc_controller_test/grpc_runtime_not_initialized_test.cpp` | Regression test: a generated-client local reject and a real `muduo_grpc` call leave the official runtime uninitialized |
| `go/db/internal/kafka/key_ordered_consumer.go` | Kafka partition worker pool (structured goroutine control) |
