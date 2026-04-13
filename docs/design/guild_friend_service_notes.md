# Guild & Friend Service Implementation Notes (2025-03-25)

## Services Created

### Guild Service (`go/guild/`)
- **Port**: 50300
- **Redis**: Global Redis DB:2 (cross-zone)
- **Node type**: GuildNodeService (node.proto)
- **Features**: Create/Get/Join/Leave/Disband/SetAnnouncement + Ranking (UpdateScore/GetRank/GetRankByGuild)
- **Patterns**: cache-aside + singleflight (Redis → MySQL → write-back)
- **Ranking**: Dual ZSET (global + per-zone), see `guild_ranking_architecture.md`
- **ID gen**: bwmarrin/snowflake

### Friend Service (`go/friend/`)
- **Port**: 50400
- **Redis**: Global Redis (cross-zone)
- **Node type**: FriendNodeService=27 (node.proto)
- **Features**: AddFriend/AcceptFriend/RejectFriend/RemoveFriend/GetFriendList/GetPendingRequests
- **Patterns**: cache-aside + singleflight
- **Validation**: self-add check, already-friends check, friend list full check, duplicate pending check

## Proto Integration
- Both added to `tools/proto_generator/protogen/etc/proto_gen.yaml` under `domain_meta`, `rpc.type: grpc`
- Proto-gen generates ~96 .pb.go files per service
- **Key gotcha**: `tip.proto` has NO package declaration — must use `TipInfoMessage` (not `common.base.TipInfoMessage`) in proto files
- **Go import**: `proto/common/base` (not `proto/common`) because generated code lives at `proto/common/base/`

## MySQL Tables (deploy/mysql-init/guild_friend_tables.sql)
- `guild` (PK guild_id, UK name, zone_id)
- `guild_member` (PK guild_id+player_id)
- `friend` (PK player_id+friend_player_id)
- `friend_request` (PK from_player_id+to_player_id, IDX to_player_id+status)

## Architecture Decision: No Separate Ranking Service
- Guild uses global Redis → single `guild_rank` ZSET is already全服
- Per-zone ranking via `guild_rank:zone:{zoneId}` ZSET
- Redis Cluster: single key = single slot, no cross-shard issue
- Separate ranking service only needed if guild data split across per-zone Redis instances
