# Scene Node 线程模型决策

**日期**: 2025-04-05
**最近修订**: 2026-10-16(第 4 批:gRPC 服务端改为 muduo 驱动,跑在 EventLoop 上,见 [grpc-muduo-transport.md](grpc-muduo-transport.md) §15;代码未编译,待 Codex 验证)

## 现状：业务逻辑单线程，进程并非单线程

Scene Node 的线程安全边界是单个 muduo `EventLoop`，不是“整个进程只有一条线程”。

必须在 EventLoop 上执行的内容包括：

- ECS、玩家、场景等 gameplay 状态变更；
- `World::Update()` 逻辑 tick（33ms 帧间隔）；
- muduo RPC handler 的业务部分；
- Kafka 后台 consumer 投递回来的业务回调；
- gRPC 服务端（第 4 批起是 `muduo_grpc::Server`）的全部工作：接入、HTTP/2 协议、派发、截止、排空，
  以及生成代码里的 `grpc.<M>.guard` 守护段和同步的 `Handle*`；
- Agones 异步许可（`AcquireAllocationPermitAsync`）经 `queueInLoop` 投回来的回调。

进程中同时存在非业务线程：

- Kafka/librdkafka、异步日志和 Agones lifecycle worker 等后台线程。

第 3 批（客户端）与第 4 批（服务端）之后，节点不再构造任何官方 gRPC 运行时对象，所以**没有** gRPC
sync server poller、EventEngine 线程池、Lifeguard，也没有关机期间跑 `grpc::Server::Shutdown()` 的
临时 worker（[grpc-muduo-transport.md](grpc-muduo-transport.md) §14、§15）。`GRPC_SERVER_MAX_POLLERS`
已退役：进程忽略它，设了只打一行 WARN。第 5 批再加「运行期不起 gRPC 线程」的守卫。
以上是设计结论，代码尚未编译运行：是否真的没有这些线程，以 grpc-muduo-transport.md §15.14 第 8 步的 thread baseline 与线程名核对为准，
核对通过前不宣称「线程归零」。

历史（第 4 批之前）：官方 sync server 自带 poller（`GRPC_SERVER_MAX_POLLERS` 默认上限 8），gRPC v1.83
默认 EventEngine 会 eager 启动 `Clamp(gpr_cpu_num_cores(), 4, 16)` 条 reserve worker 外加 1 条 Lifeguard，
并能按积压扩展，且没有公开的环境变量能调。这正是第 3–5 批要消除的「线程数不可控」。

入口为 `RunNodeMain`，最终由 `loop.loop()` 驱动业务主线程。

## 决策：现阶段保持业务状态单线程归属

### 理由

1. **无锁 ECS**：所有 gameplay 状态变更回到同一 EventLoop，访问 entt registry 无需跨线程加锁。
2. **连接数少**：Scene Node 对端是内部服务（Gate、SceneManager），不是客户端，业务侧无需为连接数引入并行状态修改。
3. **Kafka 消息轻量**：后台 consumer 只负责收取并投递，实际 control message 处理仍回到 EventLoop。
4. **水平扩容**：通过多个 Scene Node 实例分担不同场景/地图，而不是先把单个 ECS 拆成多线程共享状态。
5. **调试简单**：业务状态变更顺序由 EventLoop 串行化，竞态边界清晰。

第 4 批起 gRPC 的传输和业务都在 EventLoop 上：派发泵每轮最多 64 个调用且累计 ≤2ms（至少 1 个），
剩余的留到下一轮，中间让出给定时器和 I/O。因此 handler **不得阻塞**：`Handle*` 同步执行；
需要等外部结果的（Agones allocate 许可）由守护段发起异步请求，结果经 `queueInLoop` 回到 loop 再
`Proceed()`。不再有 promise/future 跨线程等待，也不存在「gRPC 线程读写 ECS」的问题。

### 对比：Gate Node

Gate 面对大量客户端 TCP 连接，可以使用 `EventLoopThreadPool` 扩展 I/O
处理；这不改变业务状态必须按其明确所有权串行化或同步的原则。

## gRPC 关机约束

第 4 批起 gRPC 排空全部在 EventLoop 内完成（[grpc-muduo-transport.md](grpc-muduo-transport.md) §15.10），
没有关机线程、没有 join。排空本身要靠 loop 推进，所以 handler 不得同步等待排空。关机状态机仍是：

```text
Running -> ClosingIngress -> (GrpcDraining || PersistenceDraining) -> Finalizing -> Done
```

1. EventLoop 先停止并 join Kafka consumer，封住 Kafka 入站；producer 继续
   存活，供玩家停机存盘发送 DBTask。随后执行关机前业务 hook（scene 在 hook 里
   `lifecycle.Stop()`，把还在等 Agones 许可的 CreateScene 以「拒绝」投进 loop 队列）。
2. 同一个 loop 任务里调 `grpcServer_->BeginShutdown({2s, 1s, 1s}, onDrained)`：置排空标志（此后
   `ServerDraining()` 为真，CreateScene 守护段回 `UNAVAILABLE "scene_node_shutting_down"`）、
   经 `queueInLoop` 销毁 Acceptor 停止监听、每条连接两段 GOAWAY（收到 PING ACK 或满 1s 发最终
   GOAWAY）。最终 GOAWAY 之前收到的调用照常处理；2s 到期时未开始的回 `REFUSED_STREAM`、已开始的回
   `UNAVAILABLE "server shutting down"`，再写半关、宽限 1s 后 forceClose，总上限约 3s。同时 EventLoop
   每 100ms 现场检查业务 barrier：玩家实体为空、退出存盘均已收到 Redis ACK、Kafka producer 队列已真正
   flush。两条 drain 并行推进。
3. 只有 gRPC 已排空（`onDrained` 经 `queueInLoop` 投递，`Node::OnGrpcServerDrained` 在该排队任务里
   销毁 Server），并且业务 barrier 当前为完成状态，EventLoop 才能 Finalize；业务 barrier 最多等待
   15 秒，超时必须明确记录未完成并继续退出。gRPC 排空完成前不得锁存一次短暂的业务完成结果，因为在途
   调用仍可能追加工作。
4. Finalize 时先有界 flush Kafka producer，再执行 `tlsRedis.Shutdown()`；该调用
   会在 `MessageAsyncClient` 仍存活时触发 Hiredis pending callback 并注销 Channel。
   `loop.loop()` 返回后再执行 `tlsRedisSystem.Shutdown()` 销毁
   `MessageAsyncClient`，最后才允许 `EventLoop` 析构。固定析构顺序为
   Hiredis → MessageAsyncClient → EventLoop。

`muduo_grpc::Server` 与它的 Acceptor 只能在 `queueInLoop` 任务里或 loop 外析构（I/O 事件处理期间
`removeChannel` 有断言）；Finalize 时若 Server 仍在，只能经 `queueInLoop` 防御销毁，不能同步 reset。
（第 4 批之前的约束是「不得在 EventLoop 上同步调用 `grpc::Server::Shutdown()`、不得立即 join 关机
worker」，随官方服务端一起退役。）

## 演进路径（未来如遇瓶颈）

```text
阶段 0（现在）: 单线程业务 EventLoop（RPC0 与 gRPC 传输都在其上）+ 后台 workers
    ↓  当网络投递和业务回调持续挤压逻辑帧
阶段 1: 增加 muduo I/O worker，只负责收发和解码
         所有业务状态修改继续 runInLoop 投递回主线程
    ↓  当单场景玩家密度极高
阶段 2: 场景内按 AOI 区域分片（需要明确 ECS 分片和跨分片消息协议）
```

## 触发演进的信号

- 投递到 EventLoop 的网络/Kafka 回调单帧耗时持续超过 2ms；
- `World::Update` 耗时接近 33ms 帧间隔；
- EventLoop 待执行队列持续积压；
- `[GrpcServer] summary` 的 `max_backlog` / `max_dispatched_per_turn` 持续偏高，或出现 `rejected_overload`；
- AOI 同步或跨场景序列化成为稳定瓶颈。
