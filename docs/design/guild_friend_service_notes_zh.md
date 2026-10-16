# 公会与好友服务实现笔记 (2025-03-25)

> 2026-10-15 修订：同步独占库、好友容量门禁退役与帮会号段发号。详细契约见 [friend-persistence-architecture.md](./friend-persistence-architecture.md) 与 [帮会二期交接](./guild-phase2/92-handoff.md)。

## 已创建的服务

### 公会服务 (`go/guild/`)
- **端口**: 50300
- **Redis**: 全局 Redis DB:2（跨区）
- **节点类型**: GuildNodeService (node.proto)
- **功能**: 创建/获取/加入/离开/解散/设置公告 + 排行榜 (UpdateScore/GetRank/GetRankByGuild)
- **模式**: cache-aside + singleflight (Redis → MySQL → 回写)
- **排行榜**: 双 ZSET（全局 + 按区），详见 `guild_ranking_architecture.md`
- **ID 生成**：默认通过 data_service 的 `AllocateIdSegment` 领取 `guild` 号段。当前 `IdSegment.Enabled=true`、`FallbackToSnowflake=false`，号段不可用时拒绝建帮；只有显式禁用号段或允许回退时才走 Snowflake，策略见 `go/guild/internal/svc/guild_id_minter.go`。

### 好友服务 (`go/friend/`)
- **端口**: 50400
- **Redis**：`SharedRedis` 读取跨运行时会话契约；`FriendRedis` 保存好友私有缓存/配额，未配置时回落共享实例。
- **节点类型**: FriendNodeService=27 (node.proto)
- **功能**: 添加好友/接受好友/拒绝好友/删除好友/获取好友列表/获取待处理请求
- **模式**: cache-aside + singleflight
- **校验**: 不能加自己、已是好友检查、好友列表已满检查、重复申请检查

## Proto 集成
- 两个服务均已添加到 `tools/proto_generator/protogen/etc/proto_gen.yaml` 的 `domain_meta` 中，`rpc.type: grpc`
- 协议生成物统一位于 `go/proto/`；通过仓库 proto-gen 管线生成，不按服务维护另一份 `.pb.go`。
- **关键注意点**: `tip.proto` 没有 package 声明——在 proto 文件中必须使用 `TipInfoMessage`（而非 `common.base.TipInfoMessage`）
- **Go 导入路径**: `proto/common/base`（而非 `proto/common`），因为生成的代码位于 `proto/common/base/`

## MySQL 库与表的事实源

- guild 使用独占库 `mmorpg_guild`，schema 源为 `proto/guild/guild_db.proto`，完整表清单由 `go/guild/internal/data/tables.go::Tables` 声明，涵盖关系、管理、资产与活动数据。
- friend 使用独占库 `mmorpg_friend`，schema 源为 `proto/friend/friend_table.proto`，四张业务表为 `friend`、`friend_request`、`friend_capacity`、`friend_block`，清单见 `go/friend/internal/data/tables.go`。
- 两者均经 `go/schemamigrate` 管理结构；旧共享库的 `deploy/mysql-init/guild_friend_tables.sql` 不是当前服务的 schema 事实源。迁移前按各服务的 `-migrate` 入口与配置操作。

## 一致性与授权门禁

- 好友的 `friend_capacity_backfill_v1` / `guild_schema_migration` 启动与写入门禁已于 2026-09-18 退役。friend 只连接 `mmorpg_friend`，不再查询旧共享库的 readiness 标记。写事务仍先补齐容量行，再按 player_id 升序锁定双方容量行；缺行从 `COUNT(friend)` 的权威边数初始化，绝不猜 0。退役原因与当前锁序见 [friend-persistence-architecture.md](./friend-persistence-architecture.md) 的“存量容量回填门禁”与“锁序纪律”。
- Redis 公会快照仅用于展示。`SetAnnouncement` 在一个 MySQL 事务内依次锁
  `guild` 与操作者的 `guild_member`，只允许权威 role 为 officer/leader 时更新；
  降权、退会后的陈旧 Redis 成员缓存不能继续授权。

## 架构决策：不单独拆分排行榜服务
- 公会使用全局 Redis → 单个 `guild_rank` ZSET 天然就是全服的
- 按区排行通过 `guild_rank:zone:{zoneId}` ZSET 实现
- Redis Cluster：单个 key = 单个 slot，不存在跨分片问题
- 只有当公会数据分散在按区 Redis 实例中时，才需要独立的排行榜服务
