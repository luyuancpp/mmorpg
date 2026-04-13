# Cross-Scene Player Messaging

## Problem
Scene A wants to send a message to a player, but the player may not be on Scene A. A unified message delivery mechanism is needed.

## Two Scenarios

### Case 1: Player is not on the local Scene (cross-scene delivery)

**Old approach**: Forward through Centre → complex, Centre becomes bottleneck/single point of failure
**New approach**: Deliver via Kafka to the topic of the Scene where the player currently resides

**Key edge case — player switches scene during routing**:
- Message has entered Kafka (target: Scene B's topic)
- During routing, the player switches from Scene B to Scene C
- When Scene B receives the message, the player is already gone → message lost or delivered to wrong scene

**Solution — message importance classification (via protobuf option)**:
- Annotate message importance level on RPC service method definitions via `option`
- **Important messages** (e.g., trade results, reward grants, state changes):
  - Require delivery confirmation / failure retry
  - Scene B finds player has left → re-query PlayerLocator → forward to Scene C
  - Or fall back to persistent queue (DB/Redis), redeliver when player enters a scene next time
- **Non-important messages** (e.g., chat bubbles, effect notifications, non-critical UI hints):
  - Lost is acceptable, no retry needed
  - Scene B finds player absent → discard

### Case 2: Player is on the local Scene (local delivery)

**Pending decision**: Direct synchronous processing? Or also put into a unified player message queue?

**Direct processing (synchronous path)**:
- Pros: Lowest latency, simple and direct
- Cons: Inconsistent logic with the cross-scene path, cannot guarantee unified message ordering

**Unified message queue (asynchronous path)**:
- Pros: Local and cross-scene go through the same path, unified logic, ordered messages
- Cons: Local messages incur one extra queue hop
- Consistent with the "location transparency" principle

**Conclusion**: Default to the unified message queue, consistent with the cross-server "location transparency" principle. For ultra-low-latency scenarios such as movement sync, allow marking messages to take the direct path as an exception.

## Design Points

1. **Protobuf option for importance**: Add option on service/rpc method definitions; code generation produces different delivery strategies accordingly
2. **Failure handling varies by importance**: Important messages guarantee at-least-once delivery; non-important messages are best-effort
3. **Player message queue**: Each player maintains an ordered message queue on their current Scene, with a unified processing entry point
4. **Integration with existing Kafka architecture**: Reuse `gate-{gate_id}` topic pattern, add `scene-{scene_id}` topics or `player-msg-{partition}` topics

## To Be Refined
- [ ] Specific protobuf option definition (e.g., `option (message_priority) = IMPORTANT;`)
- [ ] Retry/persist-and-redeliver mechanism details for important messages after failure
- [ ] Player message queue implementation location (Scene ECS comp? Independent system?)
- [ ] Final decision on whether local vs cross-scene should use a unified path
- [ ] Kafka topic partitioning strategy (player hash vs. scene ID)
