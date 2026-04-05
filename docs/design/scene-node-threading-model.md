# Scene Node 线程模型决策

**日期**: 2025-04-05

## 现状：单线程 EventLoop

Scene Node 使用单个 muduo `EventLoop` 跑所有事情：
- RPC handler 执行
- Kafka 消息处理
- `World::Update()` 逻辑 tick（33ms 帧间隔）
- 无 I/O 线程池（`setThreadNum(0)`）

入口：`RunSimpleNodeMainWithOwnedContext` → `loop.loop()` 阻塞主线程。

## 决策：现阶段不分离网络线程和逻辑线程

### 理由

1. **无锁 ECS**：所有 gameplay 逻辑和 RPC handler 在同一线程，访问 entt registry 无需加锁，正确性和性能都更好。
2. **连接数少**：Scene Node 对端是内部服务（Gate、SceneManager），不是客户端。连接数几十个，I/O 压力远低于 Gate。
3. **Kafka 消息轻量**：control message（RoutePlayer、KickPlayer），非高吞吐数据流。
4. **水平扩容**：通过多 Scene Node 实例分担不同场景/地图，而非单进程多线程。
5. **调试简单**：所有状态变更可预测，无竞态条件。

### 对比：Gate Node 需要多线程

Gate 面对大量客户端 TCP 连接，使用 `EventLoopThreadPool`（N 个 I/O worker），这是正确的分离。

## 演进路径（未来如遇瓶颈）

```
阶段 0（现在）: 单线程 EventLoop
    ↓  当逻辑帧开始吃紧（网络处理占帧时间 >10%）
阶段 1: setThreadNum(N)，I/O 线程收发，muduo runInLoop 投递回主线程
         主线程每帧 drain 队列 → 处理 → 逻辑 tick
         handler 代码不用改
    ↓  当单场景玩家密度极高
阶段 2: 场景内分 AOI 区域，不同区域分到不同线程（需要 ECS 分片）
```

## 触发分离的信号

- gRPC 序列化/反序列化 + Kafka poll 单帧耗时 >2ms
- `World::Update` 耗时接近 33ms 帧间隔
- AOI 同步量大、跨场景传输量大，序列化本身成为瓶颈
