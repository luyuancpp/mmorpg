# Guild & Friend Service Implementation Notes (2025-03-25)

> 2026-10-15 修订：同步独占库、好友容量门禁退役与帮会号段发号。详细契约见 [friend-persistence-architecture.md](./friend-persistence-architecture.md) 与 [帮会二期交接](./guild-phase2/92-handoff.md)。

## Services Created

### Guild Service (`go/guild/`)
- **Port**: 50300
- **Redis**: Global Redis DB:2 (cross-zone)
- **Node type**: GuildNodeService (node.proto)
- **Features**: Create/Get/Join/Leave/Disband/SetAnnouncement + Ranking (UpdateScore/GetRank/GetRankByGuild)
- **Patterns**: cache-aside + singleflight (Redis → MySQL → write-back)
- **Ranking**: Dual ZSET (global + per-zone), see `guild_ranking_architecture.md`
- **ID 生成**：默认通过 data_service 的 `AllocateIdSegment` 领取 `guild` 号段。当前 `IdSegment.Enabled=true`、`FallbackToSnowflake=false`，号段不可用时拒绝建帮；只有显式禁用号段或允许回退时才走 Snowflake，策略见 `go/guild/internal/svc/guild_id_minter.go`。

### Friend Service (`go/friend/`)
- **Port**: 50400
- **Redis**：`SharedRedis` 读取跨运行时会话契约；`FriendRedis` 保存好友私有缓存/配额，未配置时回落共享实例。
- **Node type**: FriendNodeService=27 (node.proto)
- **Features**: AddFriend/AcceptFriend/RejectFriend/RemoveFriend/GetFriendList/GetPendingRequests
- **Patterns**: cache-aside + singleflight
- **Validation**: self-add check, already-friends check, friend list full check, duplicate pending check

## Proto Integration
- Both added to `tools/proto_generator/protogen/etc/proto_gen.yaml` under `domain_meta`, `rpc.type: grpc`
- 协议生成物统一位于 `go/proto/`；通过仓库 proto-gen 管线生成，不按服务维护另一份 `.pb.go`。
- **Key gotcha**: `tip.proto` has NO package declaration — must use `TipInfoMessage` (not `common.base.TipInfoMessage`) in proto files
- **Go import**: `proto/common/base` (not `proto/common`) because generated code lives at `proto/common/base/`

## MySQL 库与表的事实源

- guild 使用独占库 `mmorpg_guild`，schema 源为 `proto/guild/guild_db.proto`，完整表清单由 `go/guild/internal/data/tables.go::Tables` 声明，涵盖关系、管理、资产与活动数据。
- friend 使用独占库 `mmorpg_friend`，schema 源为 `proto/friend/friend_table.proto`，四张业务表为 `friend`、`friend_request`、`friend_capacity`、`friend_block`，清单见 `go/friend/internal/data/tables.go`。
- 两者均经 `go/schemamigrate` 管理结构；旧共享库的 `deploy/mysql-init/guild_friend_tables.sql` 不是当前服务的 schema 事实源。迁移前按各服务的 `-migrate` 入口与配置操作。

## 一致性与授权门禁

- 好友的 `friend_capacity_backfill_v1` / `guild_schema_migration` 启动与写入门禁已于 2026-09-18 退役。friend 只连接 `mmorpg_friend`，不再查询旧共享库的 readiness 标记。写事务仍先补齐容量行，再按 player_id 升序锁定双方容量行；缺行从 `COUNT(friend)` 的权威边数初始化，绝不猜 0。退役原因与当前锁序见 [friend-persistence-architecture.md](./friend-persistence-architecture.md) 的“存量容量回填门禁”与“锁序纪律”。
- Redis guild snapshots are display caches only. `SetAnnouncement` locks the
  `guild` row and the operator's authoritative `guild_member` row in one MySQL
  transaction, then permits only officer/leader roles before updating.

## Architecture Decision: No Separate Ranking Service
- Guild uses global Redis → single `guild_rank` ZSET is already全服
- Per-zone ranking via `guild_rank:zone:{zoneId}` ZSET
- Redis Cluster: single key = single slot, no cross-shard issue
- Separate ranking service only needed if guild data split across per-zone Redis instances
