# Guild Ranking Architecture (2025-03-25)

## Problem
Need to support both **global (cross-server) rankings** and **per-zone rankings** simultaneously, working correctly under both zone-locked and zone-unlocked scenarios.

## Solution: Dual Redis ZSET Strategy

### Redis Keys
- **Global ranking**: `guild_rank` — all guilds, single ZSET
- **Per-zone ranking**: `guild_rank:zone:{zoneId}` — one ZSET per zone

### Writes (Pipeline dual-write)
- `UpdateGuildScore`: Pipeline simultaneously `ZADD guild_rank` + `ZADD guild_rank:zone:{zoneId}`
- `RemoveGuildFromRank`: Pipeline simultaneously `ZREM` both keys
- `CreateGuild`: Initialize with score=0, write to both ZSETs
- `DisbandGuild`: Remove from both ZSETs

### Reads (zone_id routing)
- `zone_id=0` → read `guild_rank` (global)
- `zone_id>0` → read `guild_rank:zone:{zoneId}` (per-zone)
- Pagination: `ZCARD` for total, `ZREVRANGE start stop` for current page, client uses `ceil(total/page_size)` to compute total pages

### Zone-lock / Zone-unlock Has No Impact
- Ranking data isolation relies on the `zone_id` field, not on the zone-lock mechanism
- Global ranking is read-only display; no cross-zone operations involved
- When zones are unlocked, a guild's `zone_id` is fixed at creation time, so per-zone rankings remain accurate

### Redis Architecture
- All guild instances connect to the same **global Redis** (DB:2), not per-zone Redis
- Under Redis Cluster, a single ZSET key lands on one slot/node — no cross-shard issues
- No additional aggregation/ranking service needed

## Data Model Changes
- `GuildData` adds `ZoneID uint32`
- MySQL `guild` table adds `zone_id INT UNSIGNED` + `KEY idx_zone`
- Proto: `CreateGuildRequest`, `GuildInfo`, `UpdateGuildScoreRequest`, `GetGuildRankRequest`, `GetGuildRankByGuildRequest` all add `zone_id`

## When Would You Need a Separate Ranking Service?
- Only when guild data is scattered across per-zone independent Redis instances (not shared) would an aggregation service be needed
- Current architecture uses global Redis — **not needed**

## Merge-Zone Tool

### Location
- Go CLI: `tools/merge_zone/main.go` (standalone Go module)
- PowerShell wrapper: `tools/scripts/merge_zone.ps1`
- Registered in `dev_tools.ps1` as `-Command merge-zone`

### What It Does
1. **Name conflict detection**: SQL JOIN to find guilds with same name in source and target zones
2. **MySQL migration**: `UPDATE guild SET zone_id = <target> WHERE zone_id = <source>` (skips conflicts)
3. **Redis ZSET merge**: `ZRANGEWITHSCORES` source → `ZADD` target → `DEL` source key

### Usage
```powershell
# Preview (safe)
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone -MergeSourceZone 102 -MergeTargetZone 101 -DryRun

# Execute (requires typing 'merge' to confirm)
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone -MergeSourceZone 102 -MergeTargetZone 101
```

### Properties
- **Idempotent**: safe to re-run (MySQL UPDATE is no-op for already-migrated rows, Redis ZADD overwrites)
- **Conflict-safe**: name-conflict guilds are logged and skipped (resolve manually)
- **Global ZSET untouched**: only per-zone ZSETs are merged; `guild_rank` (global) already has all guilds
