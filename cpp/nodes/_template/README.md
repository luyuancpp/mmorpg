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
  - Kafka command proto and router names in the `YourNodeHooks` struct
- [ ] Add or confirm node type in `proto/common/base/node.proto`.
- [ ] Add or confirm node type mapping in `cpp/libs/engine/core/node/system/node/node_util.cpp`.
- [ ] Implement RPC handler(s) in `handler/rpc/*`.
- [ ] Register service handlers in `handler/rpc/register_handler.cpp` when required.
- [ ] Add Kafka route dispatch function in `handler/event/*_kafka_command_router.*`.
- [ ] Rebuild the target node.

## THooks Convention

Define a `YourNodeHooks` struct with optional nested types:

```cpp
struct YourNodeHooks {
    using EventHandlerType = EventHandler;  // static Register() — called after node init
    struct TableLoadHandler {               // (optional) called after tables load
        static void OnLoaded();
    };
    struct KafkaHandler {                   // auto-configures Kafka consumer
        using CommandType = contracts::kafka::YourNodeCommand;
        static void Dispatch(const std::string& topic, const CommandType& cmd);
    };
};
```

Omit any member to skip that hook.  Kafka topic/group/field names are
auto-derived from the node type (e.g. `GateNodeService` -> topic `gate-{id}`).

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

