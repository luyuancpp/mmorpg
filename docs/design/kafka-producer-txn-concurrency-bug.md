# Kafka Producer Transactional Concurrency Bug

## Date: 2026-04-12

## Location
`go/login/internal/kafka/key_ordered_producer.go` — `SendTasks()` and `SendTask()`

## Bug
`SendMessages → CommitTxn → BeginTxn` 三步事务操作没有加锁。多 goroutine 并发调用时，sarama 事务状态机被破坏：
- 一个 goroutine 的 CommitTxn 提交了其他 goroutine 堆积的消息
- 后续 goroutine 再 CommitTxn 时事务状态已变，导致失败
- AbortTxn + BeginTxn 恢复操作进一步干扰其他 goroutine

## 症状
- BatchConcurrentLogin 50 并发：前 ~35 个成功，后 15 个全部返回 `kLoginUnknownError` (id:35)
- 失败极快（~302ms），不是超时，是事务操作直接失败
- `ensurePlayerDataInRedis` 在 Kafka send 阶段就失败了，根本没到 BLPop 等待

## Fix
给 `SendMessages/SendMessage → CommitTxn → BeginTxn` 整个事务周期加 `p.mu.Lock()/Unlock()`。

```go
// Before (broken under concurrency):
if err := p.producer.SendMessages(msgs); err != nil { ... }
if err := p.producer.CommitTxn(); err != nil { ... }
if err := p.producer.BeginTxn(); err != nil { ... }

// After (safe):
p.mu.Lock()
err := p.producer.SendMessages(msgs)
if err != nil {
    // handle error...
    p.mu.Unlock()
    return err
}
if err := p.producer.CommitTxn(); err != nil {
    // handle error...
    p.mu.Unlock()
    return err
}
if err := p.producer.BeginTxn(); err != nil {
    p.mu.Unlock()
    return err
}
p.mu.Unlock()
```

## Related Config
| Setting | Value | Impact |
|---------|-------|--------|
| `Idempotent` | `true` | Forces transactional mode |
| `MaxOpenRequests` | `1` | Required by idempotency, serializes at network level |
| `Transaction.ID` | auto-generated | Per-send commit/begin cycle |
| DB partitions | 5 workers | Not the bottleneck |
| `TaskWaitTimeout` | 5s | Dead config — never wired into context |
| `LoginTotalTimeout` | 10s | Dead config — never wired into context |

## Other Findings (same session)

### AccountDisplacement (顶号) — 未实现
- Go login 正确判断 `ReplaceLogin`，通过 Kafka 发 `KickPlayerEvent` 到 Gate
- Gate 的 Kafka 路由收到事件并分发到 `KickPlayerEventHandler`
- **但 handler 是空函数**（`cpp/nodes/gate/handler/event/gate_event_handler.cpp`）
- 需要实现：根据 `event.session_id()` 查找连接 → 发 KickPlayer 消息(msg_id=34) → 关闭连接

### Login Test Suite 修复
- `robot/main.go` 原来 `login-test` 模式调用 `runLoginTestsLocal`（单次登录），改为 `runLoginTests`（完整 17 场景套件）
- BatchConcurrentLogin 上限改为 50（login-test 是功能验证，非压测）
- 修复 `condStr(err != nil, err.Error(), "")` nil panic（Go 严格求值）

## Login Error Codes (login_error_tip.proto)
| Code | Name | Description |
|------|------|-------------|
| 0 | kLogin_errorOK | Success |
| 20 | kLoginAccountNotFound | Account not found |
| 25 | kLoginInProgress | Login already in progress |
| 35 | kLoginUnknownError | Generic error (used when ensurePlayerDataInRedis fails) |
| 37 | kLoginBeKickByAnOtherAccount | Kicked by another login |
| 48 | kLoginSessionNotFound | Session not found |
| 51 | kLoginTimeout | Login timeout |
