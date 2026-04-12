# Kafka Topic Retention Strategy

**Date**: 2026-04-12

## Principle

Most Kafka messages in this project are fire-and-forget control commands (player routing, session binding, kick, disconnect notifications). They are consumed immediately and have no value after consumption. Only DB write-behind tasks (and future payment-like messages) need disk persistence and longer retention.

## Broker Default

| Setting | Value | Purpose |
|---|---|---|
| `log.retention.ms` | `60000` (60s) | All auto-created topics expire in 1 minute |
| `log.retention.check.interval.ms` | `30000` (30s) | Faster cleanup cycle |
| `log.segment.bytes` | `16777216` (16MB) | Smaller segments → faster expiry |
| `log.retention.bytes` | `134217728` (128MB) | Hard cap per partition |

Dev environment additionally uses **tmpfs** for `/tmp/kafka-logs` to eliminate disk I/O entirely.

## Per-Topic Retention Matrix

| Topic Pattern | Retention | Persistent? | Rationale |
|---|---|---|---|
| `gate-{id}` | 60s (broker default) | No | Routing/bind/kick commands — consume immediately |
| `scene-{id}` | 60s (broker default) | No | Scene commands — consume immediately |
| `player-events` | 60s (broker default) | No | Disconnect/lease events — consume immediately |
| `db_task_zone_{id}` | **5 min** (configurable) | **Yes** | DB write-behind pipeline — survive consumer restart; config `Kafka.RetentionMs` |
| Future: payment | **longer** (explicit) | **Yes** | Critical financial messages — set per-topic |

## Topic Initialization

`go/shared/kafkautil/topic_init.go` provides `EnsureTopics(brokers, specs)`:
- Creates topics if they don't exist (with specified partition count and retention)
- Updates `retention.ms` for existing topics to match spec
- Uses sarama `ClusterAdmin` API

### Wiring

- **Login service** (`go/login/login.go`): calls `EnsureTopics` at startup, retention read from `Kafka.RetentionMs` config (default 5 min).
- **All other topics**: auto-created by Kafka broker on first produce, inherit 60s default retention.

### Config

```yaml
# login.yaml
Kafka:
  RetentionMs: 300000 # 5 minutes, increase for production if needed
```

## Adding a New Persistent Topic

1. Define a `TopicSpec` in the appropriate service's startup code:
   ```go
   kafkautil.EnsureTopics(brokers, []kafkautil.TopicSpec{
       {Name: "payment-orders", Partitions: 3, RetentionMs: 86400000}, // 24 hours
   })
   ```
2. Call it before any producer/consumer uses the topic.
3. Update this document.

## JVM Resource Limits (Dev)

| Container | Heap | Container Limit |
|---|---|---|
| kafka | `-Xms256m -Xmx512m` | 768MB / 1 CPU |
| kafka-ui | `-Xms128m -Xmx256m` | 384MB / 0.5 CPU |
