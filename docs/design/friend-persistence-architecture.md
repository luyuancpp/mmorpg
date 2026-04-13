# Friend Service Persistence Architecture

## Data Characteristics
- **Global cross-zone**: single Friend service (ZoneId=0), single shared MySQL, Redis DB 3
- **Low-frequency writes**: add/remove friend = a few ops per player per day
- **Relationship data**: bidirectional (A<->B), needs transactional consistency
- **Hard upper limit**: MaxFriends=200, must never exceed (no soft tolerance)

## Persistence Pattern: Direct MySQL + Redis Cache-Aside

Friend/Guild 这类低频关系型数据不适合 Kafka write-behind（那是给玩家高频数据用的），直接写 MySQL + cache-aside。

```
写路径: 业务操作 -> MySQL 事务(先写) -> 成功后 -> 删 Redis 缓存
读路径: Redis GET -> miss -> singleflight MySQL load -> 回填 Redis
```

### 与玩家数据持久化的对比

| 特征 | 玩家数据 | Friend / Guild |
|------|----------|----------------|
| 数据归属 | 单实体(一个玩家) | 共享/关系型(多对多) |
| 写入量 | 高频(背包/任务随时变) | 低频(加好友偶尔操作) |
| 一致性要求 | 最终一致即可 | 操作级一致(加好友立即可见) |
| 持久化方式 | Kafka write-behind | 直接写 MySQL |

### 为什么不用 dirty-flag + 定时 flush

dirty-flag 解决的是"写太频繁，需要攒批"的问题。Friend/Guild 每分钟只有几次写入，MySQL 轻松处理，攒批收益为零。反而引入：
- 加好友延迟可见（等 flush 周期）
- 宕机丢关系数据
- 双向写原子性难保证（flush A 成功 B 失败 → 单向好友）

**结论**: dirty-flag 是高频写入的优化手段，不是通用持久化模式。

## 好友上限强制保证 — 方案对比

### 三种方案

| | 容忍软上限 | 预占名额 | 同步硬检查(采用) |
|--|--|--|--|
| 是否超限 | 可能超1-2 | 不超 | **不超** |
| 名额被锁 | 不锁 | 锁(申请未回复=占名额) | **不锁** |
| 用户体验 | 好 | 差(发了申请加不了别人) | **好** |
| 单向好友风险 | 有短暂窗口 | 无 | **无(回滚兜底)** |
| 实现复杂度 | 最低 | 中 | 高 |

### 预占名额的问题
发申请时预占一个好友位 → 对方很久不回复 → 名额被锁 → 影响正常加好友。
设超时释放机制也不理想：发20个申请锁20个名额，体验很差。

### 选择: 同步硬检查
"同意"时用 FOR UPDATE 行锁原子检查 + 写入，代价是多一次事务（低频操作可接受）。

### 实现: MySQL 事务 + FOR UPDATE
```sql
BEGIN;
SELECT COUNT(*) FROM friend WHERE player_id = A FOR UPDATE;  -- 锁 A 的行
-- 检查 A 未满
SELECT COUNT(*) FROM friend WHERE player_id = B FOR UPDATE;  -- 锁 B 的行
-- 检查 B 未满
UPDATE friend_request SET status=2 WHERE ...;
INSERT IGNORE INTO friend ... (双向);
COMMIT;
```

- FOR UPDATE 锁住双方的 friend 行，防止并发 accept 超限
- 单 MySQL 实例，一个事务搞定，不需要分布式事务
- 低频操作(每天几次)，行锁开销可忽略

### 错误处理
- `ErrSenderFriendsFull` -> 告诉 B: "对方好友已满"
- `ErrAcceptorFriendsFull` -> 告诉 B: "你的好友已满"

## 未来分片方案 (1亿+ 玩家)

### 分片策略
- `shard_id = player_id % N`，每个玩家的好友列表固定落在一个分片
- 每人一行存序列化的好友列表，1亿玩家/16分片 = 每片 ~625万行

### 跨分片加好友
```
B 同意:
  1. 同步 RPC -> A 的分片: FOR UPDATE 检查 + 写入(原子)
     A 满了 -> 返回失败，结束
     成功 -> 继续
  2. 本地事务 -> B 的分片: FOR UPDATE 检查 + 写入
     B 满了 -> 同步回滚 A（失败则 Kafka 补偿队列兜底）
     成功 -> 完成
```

### 扩容
翻倍扩容(16->32)，每个分片拆成两半，简单且可预测。

## Online/Offline 状态

### 问题
好友上下线频繁，200好友 x 10K登录/秒 = 2M推送/秒，push 模式开销太大。

### 方案: Redis Key + Pull Model

**存储**:
```
Redis Key: friend:online:{playerID} -> gate_node_id, TTL 60s
```

**生命周期**:
```
登录   -> SET friend:online:{playerID} {gateNodeID} EX 60
心跳   -> EXPIRE friend:online:{playerID} 60 (每30秒)
登出   -> DEL friend:online:{playerID}
崩溃   -> key 60秒后自动过期
```

**查询 (Pull)**:
```
GetFriendList -> 取好友列表 -> Redis Pipeline 批量 EXISTS -> 附带 is_online 返回
```

200个好友的在线状态查询 = 1次 Redis Pipeline roundtrip (~0.1ms)，性能极高。

### 为什么不用 Push
| | Pull | Push |
|--|------|------|
| 登录开销 | 1次 Redis SET | 200次通知(每个好友) |
| 10K并发登录 | 10K ops | 2M ops + 跨zone路由 |
| 实现复杂度 | 极低 | 需要知道每个好友的gate，跨zone推送 |
| 实时性 | 打开好友列表时刷新 | 实时 |

**结论**: 游戏好友不需要毫秒级实时，打开好友面板时刷新即可。如果未来需要登录提示("你的好友XXX上线了")，可以在 NotifyOnline 中额外查好友列表并通过 Kafka 推送到各好友的 gate，但这是可选增强。
