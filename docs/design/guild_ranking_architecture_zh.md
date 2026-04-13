# 公会排行榜架构 (2025-03-25)

## 问题
需要同时支持**全服排行榜**和**本服（per-zone）排行榜**，且在锁区/不锁区场景下都正确工作。

## 方案：双 Redis ZSET 策略

### Redis 键
- **全服排行**: `guild_rank` — 所有公会，单个 ZSET
- **本服排行**: `guild_rank:zone:{zoneId}` — 每个区一个 ZSET

### 写入（Pipeline 双写）
- `UpdateGuildScore`：Pipeline 同时 `ZADD guild_rank` + `ZADD guild_rank:zone:{zoneId}`
- `RemoveGuildFromRank`：Pipeline 同时 `ZREM` 两个 key
- `CreateGuild`：初始化 score=0，写入两个 ZSET
- `DisbandGuild`：从两个 ZSET 移除

### 读取（zone_id 路由）
- `zone_id=0` → 读 `guild_rank`（全服）
- `zone_id>0` → 读 `guild_rank:zone:{zoneId}`（本服）
- 分页：`ZCARD` 获取总数，`ZREVRANGE start stop` 取当前页，客户端用 `ceil(total/page_size)` 计算总页数

### 锁区/不锁区无影响
- 排行榜数据隔离靠 `zone_id` 字段，不依赖锁区机制
- 全服排行是只读展示，不涉及跨区操作
- 不锁区时公会 `zone_id` 在创建时固定，本服排行依然准确

### Redis 架构
- 所有公会实例连同一个 **全局 Redis**（DB:2），不是 per-zone Redis
- Redis Cluster 下单个 ZSET key 落在一个 slot/节点上，不存在跨分片问题
- 不需要额外的聚合/排行服务

## 数据模型变更
- `GuildData` 新增 `ZoneID uint32`
- MySQL `guild` 表新增 `zone_id INT UNSIGNED` + `KEY idx_zone`
- Proto：`CreateGuildRequest`、`GuildInfo`、`UpdateGuildScoreRequest`、`GetGuildRankRequest`、`GetGuildRankByGuildRequest` 均添加 `zone_id`

## 什么时候需要独立的排行服务？
- 只有当公会数据分散在各区独立的 Redis 实例（不共享）时才需要聚合服务
- 当前架构使用全局 Redis，**不需要**

## 合服工具

### 位置
- Go CLI：`tools/merge_zone/main.go`（独立 Go 模块）
- PowerShell 封装：`tools/scripts/merge_zone.ps1`
- 在 `dev_tools.ps1` 中注册为 `-Command merge-zone`

### 功能
1. **名称冲突检测**：SQL JOIN 查找源区和目标区中同名的公会
2. **MySQL 迁移**：`UPDATE guild SET zone_id = <target> WHERE zone_id = <source>`（跳过冲突项）
3. **Redis ZSET 合并**：`ZRANGEWITHSCORES` 源区 → `ZADD` 目标区 → `DEL` 源区 key

### 用法
```powershell
# 预览（安全）
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone -MergeSourceZone 102 -MergeTargetZone 101 -DryRun

# 执行（需要输入 'merge' 确认）
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone -MergeSourceZone 102 -MergeTargetZone 101
```

### 特性
- **幂等**：可安全重复执行（MySQL UPDATE 对已迁移行无操作，Redis ZADD 会覆盖）
- **冲突安全**：同名冲突公会会被记录并跳过（需手动处理）
- **全服 ZSET 不受影响**：只合并本服 ZSET；`guild_rank`（全服）已包含所有公会
