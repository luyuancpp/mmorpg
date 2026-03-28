# 每进程线程数监控与控制

## 概述

每个 C++ 节点自动运行一个 **ThreadMonitor**，定期采样进程线程数，并在检测到持续增长时输出警告日志。Go 服务依赖 goroutine 调度和按分区的 worker 池进行并发控制。

---

## C++ 节点 — ThreadMonitor（自动启用，所有节点）

### 实现
- **文件**: `cpp/libs/engine/core/node/system/node/thread_observability.h`
- **注册**: `SimpleNode` 构造函数调用 `RegisterThreadObservability()` — 所有节点默认启用
- **机制**: 在主 EventLoop 上设置定时器，每 N 秒采样线程数
  - Windows: `CreateToolhelp32Snapshot` + `Thread32First/Next`
  - Linux: `readdir("/proc/self/task")`
- **行为**: 仅被动监控 — 输出警告日志，**不会**强制限制或终止线程

### 环境变量

| 环境变量 | 默认值 | 说明 |
|---------|---------|-------------|
| `NODE_THREAD_MONITOR_ENABLED` | `1`（开启） | 设为 `0` / `false` / `off` 以禁用 |
| `NODE_THREAD_MONITOR_SAMPLE_INTERVAL_SECONDS` | `10` | 采样间隔（秒） |
| `NODE_THREAD_MONITOR_STABLE_WINDOW_SECONDS` | `60` | 等待此时长后设定基线值 |
| `NODE_THREAD_MONITOR_GROWTH_WARN_CONSECUTIVE_SAMPLES` | `6` | 连续增长采样次数达到此值后触发警告 |
| `NODE_THREAD_MONITOR_GROWTH_WARN_ABSOLUTE_INCREASE` | `16` | 基线以上的最小绝对增量才触发警告 |

### 日志输出示例

```
[INFO]  Gate thread metrics: total_threads=12, delta_threads=0, peak_threads=14, uptime_sec=120
[INFO]  Gate thread baseline (stable window reached): baseline_threads=12, ...
[WARN]  Gate thread growth warning: total_threads=30, increase_from_baseline=+18, consecutive_growth_samples=7, ...
```

---

## C++ 各节点类型的线程池

### Gate 节点 — 预期 ~5–14 个线程

| 线程类别 | 数量 | 配置环境变量 | 备注 |
|---|---|---|---|
| muduo EventLoop（主线程） | 1 | — | 单 reactor 循环 |
| gRPC ResourceQuota 最大线程数 | 2（默认） | `GRPC_MAX_THREADS` | gRPC 服务端线程池硬上限 |
| gRPC EventEngine 预留线程 | 2（debug 配置） | `GRPC_THREAD_POOL_RESERVE_THREADS` | 预分配线程 |
| gRPC EventEngine 最大线程 | 8（debug 配置） | `GRPC_THREAD_POOL_MAX_THREADS` | 事件引擎线程硬上限 |
| Kafka consumer (librdkafka) | 2–4 | librdkafka 内部管理 | 不透明；不可直接配置 |

- gRPC 环境变量配置工具: `cpp/libs/engine/core/node/system/grpc_channel_cache.h`
- Debug 配置环境变量设置于: `cpp/nodes/gate/gate.vcxproj` `<LocalDebuggerEnvironment>`

### Scene 节点 — 预期 ~3–5 个线程

| 线程类别 | 数量 | 配置环境变量 | 备注 |
|---|---|---|---|
| muduo EventLoop（主线程，运行 `World::Update`） | 1 | — | 游戏模拟主循环 |
| Kafka consumer (librdkafka) | 2–4 | librdkafka 内部管理 | 不透明 |

Scene 节点不托管 gRPC 服务；线程数比 Gate 少。

---

## Go 服务 — 基于 Goroutine

### 通用模式

- 所有 Go 服务使用 go-zero 框架；每个 gRPC/HTTP 请求在独立的 goroutine 中运行
- Goroutine 调度受 `GOMAXPROCS` 限制（默认为 `runtime.NumCPU()`）
- 当前未配置显式的 goroutine 池上限

### db 服务 — Kafka 分区 Worker

- **文件**: `go/db/internal/kafka/key_ordered_consumer.go`
- 每个 Kafka 分区 1 个 worker goroutine（`Config.Kafka.PartitionCnt`）
- 每个 worker 的任务通道缓冲区大小为 1000
- 这是唯一具有结构化并发控制的 Go 服务

### login 服务

- etcd KeepAlive goroutine（每个注册节点 1 个）
- NodeWatcher goroutine（1 个）用于基于 etcd 的服务发现
- TaskManager goroutine（1 个）用于过期批次清理

### 其他 Go 服务（scene_manager、data_service、player_locator）

- 标准的 go-zero 每请求一个 goroutine 模型
- Kafka 生产者使用 fire-and-forget（segmentio/kafka-go）；额外 goroutine 极少

---

## 关键设计要点

1. **C++ 监控是被动的** — 仅记录日志和发出警告，不会终止或限制线程。
2. **gRPC 线程上限是唯一的硬限制** — `GRPC_MAX_THREADS`（默认 2）通过 `ResourceQuota` 限制服务端线程池。
3. **Go 服务没有 goroutine 硬上限** — 依赖操作系统通过 `GOMAXPROCS` 进行线程调度。
4. **Kafka 内部线程不透明** — librdkafka 自行管理线程（每个 consumer 实例通常 2–4 个）。
5. **如需在生产环境中强制硬限制**，可选方案包括：
   - gRPC `ResourceQuota`（已在使用）
   - Go `runtime.SetMaxThreads()` 设置操作系统级线程上限
   - K8s cgroup CPU/内存限制（间接约束线程创建）

---

## 文件索引

| 文件 | 用途 |
|------|---------|
| `cpp/libs/engine/core/node/system/node/thread_observability.h` | ThreadMonitor 类、环境变量解析、`RegisterThreadObservability()` |
| `cpp/libs/engine/core/node/system/node/simple_node.h` | 为所有 SimpleNode 实例自动注册 ThreadMonitor |
| `cpp/libs/engine/core/node/system/grpc_channel_cache.h` | gRPC 线程配置读取器（`GRPC_MAX_THREADS` 等） |
| `cpp/nodes/gate/gate.vcxproj` | gRPC 线程池的 Debug 环境变量 |
| `go/db/internal/kafka/key_ordered_consumer.go` | Kafka 分区 worker 池（结构化 goroutine 控制） |
