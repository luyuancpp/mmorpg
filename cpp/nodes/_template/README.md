# Node Main Template Guide

This folder provides starter templates for new C++ node `main.cpp` files.

## Choose A Template

- `main.simple.cpp.example`
  - Use when node startup has no long-lived runtime-owned state.
  - Example: only register handlers and Kafka consumers.

- `main.with_context.cpp.example`
  - Use when startup needs runtime-owned state.
  - Example: timers, codecs, session bridges, dispatchers.

## Build Path (Minimal Checklist)

- [ ] Copy one template into your new node folder as `main.cpp`.
- [ ] Replace placeholders:
  - `YourNodeHandler`
  - `YourNodeService`
  - log dir `logs/your_node`
  - `YourNodeHooks` 中的 Kafka command proto 与 `DispatchKafkaCommand`
- [ ] Add or confirm node type in `proto/common/base/node.proto`.
- [ ] Add or confirm node type mapping in `cpp/libs/engine/core/node/system/node/node_util.cpp`.
- [ ] Implement RPC handler(s) in `handler/rpc/*`.
- [ ] Register service handlers in `handler/rpc/register_handler.cpp` when required.
- [ ] Add Kafka route dispatch function in `handler/event/*_kafka_command_router.*`.
- [ ] Rebuild the target node.

## THooks Convention

`YourNodeHooks` 的可选成员与 `node_entry.h` 的实际契约一致：

```cpp
struct YourNodeHooks {
    struct TableLoadHandler {
        static void OnLoaded();
    };
    using KafkaCommandType = contracts::kafka::YourNodeCommand;
    static void DispatchKafkaCommand(const std::string& topic, const KafkaCommandType& command)
    {
        DispatchYourNodeKafkaCommand(topic, command);
    }
    struct ClientEndpointSourceFactory {
        static std::unique_ptr<client_endpoint::ExternalSource> Make();
    };
};
```

省略成员即可跳过对应 hook；声明 `KafkaCommandType` 时须同时提供 `DispatchKafkaCommand`。
路由先校验生成的白名单，再直接调用静态 handler。新节点没有现成入口时，先在
`protogen/internal/generator/cpp/event_ingress.go` 增加入口枚举与文件位置规则，再生成路由。
Kafka topic、group 与寻址字段由节点配置和共享消费者统一确定，见控制面 topic 设计。

## PR Self-Check

- [ ] New node `main.cpp` uses one of the entry helpers from `node/system/node/node_entry.h`.
- [ ] Node startup logic stays thin; business logic remains in systems/services.
- [ ] Thread monitor logs appear at runtime, or `NODE_THREAD_MONITOR_ENABLED=0` is intentionally set.
- [ ] No generated outputs were edited manually.

## Runtime Notes

- `SimpleNode` auto-registers thread observability.
- Thread monitor behavior is controlled by env vars:
  - `NODE_THREAD_MONITOR_ENABLED`
  - `NODE_THREAD_MONITOR_SAMPLE_INTERVAL_SECONDS`
  - `NODE_THREAD_MONITOR_STABLE_WINDOW_SECONDS`
  - `NODE_THREAD_MONITOR_GROWTH_WARN_CONSECUTIVE_SAMPLES`
  - `NODE_THREAD_MONITOR_GROWTH_WARN_ABSOLUTE_INCREASE`

## Entry Helper

- `RunSimpleNodeMainWithOwnedContext<THandler, TContext, THooks>(...)`
  - Single entry point for all nodes.  Use an empty `struct` for TContext
    if no runtime state is needed.

