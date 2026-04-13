# Zone 数据回档方案

## 概述

Zone 数据回档分两个层面：
1. **应用级回档**（已实现）：通过 `RollbackZone` RPC，从快照恢复玩家数据
2. **灾难恢复级**（Ops 操作）：MySQL PITR + Redis flush + Kafka offset reset

---

## 1. 单玩家回档（应用级）

详见 [single_player_rollback.md](single_player_rollback.md)。

- 踢玩家下线 → 找目标时间点之前最近的快照 → 调用各服务 `ImportPlayerData()` 恢复
- 自动校验交易日志防止道具复制（有正常转移记录的物品不恢复）
- 本质是**补偿式**回档，不是时间倒流

---

## 2. 整 Zone 应用级回档（`RollbackZone` RPC）

### 设计决策：Guild/Friend 不回档

**结论：Guild/Friend 数据不需要回档。**

**原因**：
- Player Redis blob（`player:{id}:player_database`）**不包含** guild_id 或 friend 引用
- Guild 通过 `GuildService` gRPC 独立查询，Friend 通过 `FriendService` gRPC 独立查询
- 两个系统的数据完全解耦，回档 player 数据不会与 guild/friend 产生不一致
- 即使存在时间差异（如玩家回档到加入公会前，但 guild_member 表仍有记录），
  这类不一致通过正常的 guild/friend 操作即可自愈（退出重进、重新申请等）

### 孤儿角色处理

> 回档的话新创建角色这种你回档了角色还在吗？

**不在，也不重新创建。**

| 情况 | 处理 |
|------|------|
| T 之前创建的角色 | 从 player_snapshot 恢复数据 ✅ |
| T 之后创建的角色（孤儿） | 无快照 → 删除 zone mapping → 玩家下次登录重新创建角色 |
| 孤儿角色的 Redis 数据 | 通过 `DeletePlayerData` 清理（zone mapping + Redis keys） |
| 孤儿角色的 guild/friend | 留原样，自然自愈；公会踢人/解散时自动清理不存在的成员 |

**不创建孤儿角色的理由**：
- 孤儿角色的数据在回档目标时间点不存在，没有有效数据可恢复
- 自动创建空角色 = 创建一个 0 级空数据角色，没有实际意义
- 让玩家正常走创建角色流程更合理（选职业、名字等）

**不创建可能的问题**：
- 如果 zone mapping 未清理，login 路由到空数据 → C++ LoadPlayerData 可能崩溃
- 解决方案：orphan cleanup 阶段必须清理 zone mapping

### 执行流程（2 阶段）

```
Phase 1: 恢复 player 数据
────────────────────────
遍历 zone 内所有有快照的 player
→ 对每个 player: 找 target_time 之前最近的 player_snapshot
→ 创建 pre-rollback 安全快照
→ 用 snapshot 数据覆盖 Redis

Phase 2: 清理孤儿角色
──────────────────────
SCAN mapping Redis, 找到该 zone 下所有有 zone mapping 的 player
→ 对比 snapshot 中的 player 列表，得到差集（孤儿）
→ 删除孤儿的 Redis 数据 + zone mapping
→ 玩家下次登录时因 zone mapping 不存在而走创建角色流程
```

### 代码位置

| 文件 | 作用 |
|------|------|
| `go/data_service/internal/logic/rollback_logic.go` | RollbackZone（player 回档 + 孤儿清理 + 调用 login 清理账号） |
| `go/data_service/internal/routing/router.go` | GetAllPlayerIDsInZone（SCAN mapping Redis） |
| `go/data_service/internal/store/snapshot_store.go` | player_snapshot + rollback_audit_log CRUD |
| `go/data_service/internal/svc/servicecontext.go` | LoginAdminClient gRPC（通过 etcd 发现 login 服务） |
| `proto/data_service/data_service.proto` | RollbackZone RPC 定义 |
| `go/login/internal/logic/admin/remove_players_from_accounts.go` | 从账号记录中批量移除孤儿 player ID |
| `go/login/internal/server/loginadmin/loginadminserver.go` | LoginAdmin gRPC server handler |
| `go/login/internal/constants/login_constants.go` | player→account 反向索引 key |
| `proto/login/login.proto` | LoginAdmin / RemovePlayersFromAccounts RPC 定义 |

### 跨服务调用链

```
RollbackZone (data_service)
  ├── Phase 1: 恢复每个 player 的 Redis 数据（从 snapshot）
  ├── Phase 2: 删除孤儿角色的 Redis 数据 + zone mapping
  └── Phase 2b: gRPC → login.RemovePlayersFromAccounts
                  ├── 查 player_to_account:{pid} 反向索引 → 获取 account name
                  ├── 从 account:{name} 的 SimplePlayers 列表中移除孤儿 pid
                  └── 删除 player_to_account:{pid} 反向索引
```

> **容错设计**：Phase 2b 为 non-fatal。如果 LoginAdminClient 未配置或 RPC 失败，
> 孤儿 ID 仍会在 RollbackZoneResponse.orphan_player_ids 中返回，供外部工具手动重试。

---

## 3. 整 Zone 灾难恢复级回档（Ops 操作）

### 架构前提

每个 zone 在 K8s 中是独立 namespace（`mmorpg-zone-{zoneName}`），拥有独立的：
- MySQL 实例（数据库名 `game`）
- Redis 实例（`--appendonly yes`）
- Kafka broker / topic（`db_task_topic`）

写路径：`C++ node → Kafka(db_task_topic) → Go db service → MySQL`，Redis 仅为缓存加速层。

### 操作步骤

```
步骤 1: 关闭 zone（停止所有业务写入）
─────────────────────────────────────
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName <zone>

步骤 2: MySQL Point-in-Time Recovery (PITR)
────────────────────────────────────────────
# 先恢复到影子库验证
mysqlbinlog --stop-datetime="2026-03-26 10:00:00" binlog.000xxx | mysql -h shadow-host -u root -p game

# 确认无误后替换正式库
# 方式 A: 云 RDS 控制台一键 PITR
# 方式 B: 自建 MySQL 用 binlog 手动恢复

步骤 3: Redis 处理
──────────────────
# Redis 是缓存层，C++ 节点启动时会从 MySQL 重建，直接清空即可
redis-cli -h <redis-host> FLUSHDB

# 或从 AOF/RDB 备份恢复到对应时间点（如果有归档的话）

步骤 4: Kafka 处理
──────────────────
# 清空 db_task_topic，防止回档时间点之后的写入消息被重放
# 方式 A: 删除并重建 topic
kafka-topics.sh --delete --topic db_task_topic --bootstrap-server <broker>
kafka-topics.sh --create --topic db_task_topic --partitions 5 --bootstrap-server <broker>

# 方式 B: 重置 consumer group offset 到指定时间戳
kafka-consumer-groups.sh --reset-offsets --to-datetime 2026-03-26T10:00:00.000 \
  --group db_rpc_consumer_group --topic db_task_topic --bootstrap-server <broker> --execute

步骤 5: 重新拉起 zone
─────────────────────
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up \
  -ZoneName <zone> -ZoneId <id> -NodeImage <image> -WaitReady
```

### 关键安全点

| 要点 | 说明 |
|------|------|
| Redis 可安全清空 | 它是缓存层，节点启动后从 MySQL 重建 |
| Kafka 必须处理 | 不清空 topic/offset 会导致旧消息重放覆盖回档数据 |
| MySQL 必须先恢复 | 它是唯一的持久化真相源（source of truth） |
| 串行保证 | Kafka partition key = `player_id`，同玩家写入串行执行 |

---

## 3. 当前缺失基建

| 缺失项 | 说明 | 优先级 |
|--------|------|--------|
| MySQL 定时备份 + binlog 归档 | K8s MySQL pod 未配置自动备份，需加 CronJob 或用云 RDS | **高** |
| Redis RDB/AOF 定期快照归档 | 当前 `--appendonly yes` 保证持久化，但无历史时间点恢复能力 | 低（可 FLUSHDB） |
| Kafka offset 回档工具 | 需脚本将 consumer group offset 重置到指定时间戳 | 中 |
| 一键 zone 回档脚本 | 将上述步骤封装为 `dev_tools.ps1 -Command k8s-zone-rollback` | 中 |

---

## 4. 生产环境建议

- **云 MySQL（RDS）**：自带自动备份和 PITR，回档操作在控制台点几下即可，**强烈推荐**
- **自建 MySQL**：必须配置 `log-bin`、定期 `mysqldump` CronJob、binlog 归档到对象存储
- **Redis**：zone 回档时直接 FLUSHDB，无需额外备份
- **Kafka**：回档后最安全的做法是清空 `db_task_topic`，避免旧消息重放
