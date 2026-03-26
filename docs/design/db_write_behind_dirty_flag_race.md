# DB Write-Behind 脏标记方案竞态分析

**Created:** 2026-03-26

## 方案描述（Write-Behind / 写回）

```
外部写操作 → 更新 Redis + 标记 key 为脏
db 服务   → 定期扫描脏 key 集合 → 从 Redis 读最新数据 → 批量写 MySQL
```

## 核心问题：脏标记清除的竞态条件

```
T=0: flusher 从 Redis 读到 key K 的数据 V2
T=1: 新写入把 Redis 里 K 更新成 V3，再次标记 dirty
T=2: flusher 把 V2 写入 MySQL，然后清除 dirty 标记
结果: MySQL=V2, Redis=V3, dirty 已清除 → V3 永远不会被刷回 MySQL！
```

新数据被旧数据覆盖，且由于 dirty 已被清除，V3 会永久丢失。

## 可选修复：版本号 + Lua 原子操作

- 每次写入 Redis 时递增版本号
- flush 时用 Lua 脚本原子执行：读数据 + 只在版本号匹配时清除脏标记
- 复杂度高，引入分布式竞态管理

## 推荐方案对比

| 方案 | 正确性 | 性能 | 崩溃恢复 | 复杂度 |
|------|--------|------|----------|--------|
| 每条立即写 MySQL（当前） | 最安全 | 每条一次 IO | Kafka 重投幂等 | 低 |
| 外部脏标记 + 定期 flush | 有竞态 | 批量高吞吐 | Redis 脏数据可能丢 | 高 |
| **Worker 内 batch flush** | 安全 | 批量高吞吐 | Kafka 重投幂等 | 中 |

## 最佳方案：Worker 内 batch flush

利用已有 Kafka partition worker 的单 goroutine 串行保证：

```
Kafka partition worker (同 key 串行，单 goroutine)
├─ 收到 write msg → 更新 Redis 缓存 → 加入本地 dirtyMap
├─ ...
├─ 每 N 条或每 T 秒 → 批量 flush dirtyMap 到 MySQL
└─ flush 成功 → 清空 dirtyMap → MarkMessage
```

**为什么安全**：读 Redis、写 MySQL、清脏标记全在同一个 goroutine 里串行执行，没有竞态。

## 方案A / 方案B 使用准则

### 方案A（推荐默认）

A = **写请求方直接发 Kafka `Op:"write"` 到 db 服务**（可同时更新 Redis 缓存）。

**什么时候用 A：**
- 需要最高正确性，优先保证"新数据不被旧数据覆盖"
- 当前写入 QPS 未把 MySQL 打满
- 希望复用现有 Kafka 同 key 串行与重试能力
- 希望崩溃后依赖 Kafka 重投保证最终落库

**A 的收益：**
- 链路简单：写源头就是最新数据，不需要二次从 Redis 回读
- 一致性更稳：同 key 顺序天然受 Kafka partition + worker 保证
- 故障恢复清晰：消息未 ack 就可重投

### 方案B（仅在吞吐瓶颈时启用）

B = **db worker 内 batch flush**（同 worker 内维护 dirtyMap，按 N 条或 T 秒批量写 MySQL）。

**什么时候用 B：**
- 监控确认 MySQL 已成为写入瓶颈（例如 p95/p99 写延迟持续超标）
- 需要显著减少 SQL 次数、提升吞吐
- 能接受短暂的落库延迟（T 秒级）

**B 的前提与边界：**
- 必须在同一个 partition worker 内完成"收写请求→更新 dirtyMap→flush→ack"
- 不能改成外部独立脏扫描器（会引入竞态）
- 需要可观测性：flush 成功率、队列积压、单 key 最长未落库时间

## 不建议方案

- 外部脏标记 + 定时扫描 flush（无版本控制）
- 原因：存在"旧值落库后清脏，导致新值永不落库"的竞态

## 方案A 实现状态（2026-03-26 完成）

**C++ 端（`cpp/libs/services/scene/player/system/player_lifecycle.cpp`）：**
- `SavePlayerToRedis()` 在保存 Redis 后，将 `PlayerAllData` 拆分为 `player_database` 和 `player_database_1` 两个子表
- 每个子表发送一条独立的 `DBTask(op="write")` 到 Kafka topic `db_task_topic`
- `msg_type` 使用子表 proto full_name（`player_database` / `player_database_1`），与 login 读路径一致
- `task_id` 格式：`{playerId}:{tableName}:{millisTimestamp}`，用于日志追踪
- 必须在序列化前 `set_player_id()`（generated marshal 不设主键）

**Go 端（`go/db/internal/kafka/key_ordered_consumer.go`）：**
- `handleDBWriteOp()` 已实现，通过 `proto_sql.DB.SqlModel.Save(msg)` 执行 `REPLACE INTO`
- 写完 MySQL 后同步更新 Redis 缓存
- 无需改动，直接接收 C++ 发来的 write 消息

**关键约束：**
- 不能发送 `PlayerAllData` 整体作为 DBTask（`Save()` 只处理单表映射消息）
- Kafka key = playerId 字符串，保证同一玩家的写入落到同一 partition，串行执行

## 最终结论

- **默认选 A**：先保证正确性与可恢复性
- **有明确性能证据再切 B**：并且只允许 worker 内 batch flush 形态
- 无论 A/B，必须坚持同 key 串行与幂等写（当前 `REPLACE INTO`）
