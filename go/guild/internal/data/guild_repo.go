package data

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"strconv"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"golang.org/x/sync/singleflight"
)

// GuildData is the persistence-layer representation of a guild (stored in Redis + MySQL).
type GuildData struct {
	GuildID      uint64       `json:"guild_id"`
	Name         string       `json:"name"`
	LeaderID     uint64       `json:"leader_id"`
	Level        uint32       `json:"level"`
	Announcement string       `json:"announcement"`
	CreateTimeMs int64        `json:"create_time_ms"`
	MaxMembers   uint32       `json:"max_members"`
	ZoneID       uint32       `json:"zone_id"`
	Members      []MemberData `json:"members"`
}

type MemberData struct {
	PlayerID     uint64 `json:"player_id"`
	Role         uint32 `json:"role"`
	JoinTimeMs   int64  `json:"join_time_ms"`
	LastActiveMs int64  `json:"last_active_ms"`
	Contribution uint64 `json:"contribution"`
}

// GuildRepo provides cache-aside access to guild data.
// Read path:  Redis → (miss) → singleflight → MySQL → write-back Redis
// Write path: Redis + MySQL (sync)
type GuildRepo struct {
	rdb        *redis.Client
	db         *sql.DB
	sfGroup    singleflight.Group
	defaultTTL time.Duration
}

func NewGuildRepo(rdb *redis.Client, db *sql.DB, defaultTTL time.Duration) *GuildRepo {
	return &GuildRepo{
		rdb:        rdb,
		db:         db,
		defaultTTL: defaultTTL,
	}
}

// ── Redis key helpers ──────────────────────────────────────────

func guildKey(guildID uint64) string {
	return fmt.Sprintf("guild:%d", guildID)
}

func playerGuildKey(playerID uint64) string {
	return fmt.Sprintf("player_guild:%d", playerID)
}

const guildRankKey = "guild_rank" // Redis ZSET: member=guildID, score=rankScore (global)

func zoneRankKey(zoneID uint32) string {
	return fmt.Sprintf("guild_rank:zone:%d", zoneID)
}

// ── Read (cache-aside + singleflight) ──────────────────────────

// GetGuild loads a guild by ID. On Redis miss, uses singleflight to coalesce
// concurrent MySQL queries for the same guild.
func (r *GuildRepo) GetGuild(ctx context.Context, guildID uint64) (*GuildData, error) {
	// 1. Try Redis
	data, err := r.rdb.Get(ctx, guildKey(guildID)).Bytes()
	if err == nil {
		var guild GuildData
		if err := json.Unmarshal(data, &guild); err != nil {
			return nil, fmt.Errorf("unmarshal guild %d from cache: %w", guildID, err)
		}
		return &guild, nil
	}
	if err != redis.Nil {
		return nil, fmt.Errorf("redis get guild %d: %w", guildID, err)
	}

	// 2. Cache miss → singleflight coalesced MySQL load
	result, err, _ := r.sfGroup.Do(fmt.Sprintf("guild:%d", guildID), func() (any, error) {
		guild, err := r.loadGuildFromMySQL(ctx, guildID)
		if err != nil {
			return nil, err
		}
		if guild == nil {
			return nil, nil // guild does not exist
		}
		// Write back to Redis
		if err := r.cacheGuild(ctx, guild); err != nil {
			logx.Errorf("cache guild %d after MySQL load: %v", guildID, err)
			// non-fatal: serve from memory
		}
		return guild, nil
	})

	if err != nil {
		return nil, err
	}
	if result == nil {
		return nil, nil
	}
	return result.(*GuildData), nil
}

// GetPlayerGuildID returns the guild ID a player belongs to (0 if none).
func (r *GuildRepo) GetPlayerGuildID(ctx context.Context, playerID uint64) (uint64, error) {
	val, err := r.rdb.Get(ctx, playerGuildKey(playerID)).Uint64()
	if err == redis.Nil {
		// Cache miss → load from MySQL
		result, err, _ := r.sfGroup.Do(fmt.Sprintf("pguild:%d", playerID), func() (any, error) {
			guildID, err := r.loadPlayerGuildFromMySQL(ctx, playerID)
			if err != nil {
				return nil, err
			}
			if guildID > 0 {
				r.rdb.Set(ctx, playerGuildKey(playerID), guildID, r.defaultTTL)
			}
			return guildID, nil
		})
		if err != nil {
			return 0, err
		}
		return result.(uint64), nil
	}
	if err != nil {
		return 0, fmt.Errorf("redis get player guild %d: %w", playerID, err)
	}
	return val, nil
}

// ── Write (dual-write: MySQL + Redis) ──────────────────────────

func (r *GuildRepo) SaveGuild(ctx context.Context, guild *GuildData) error {
	if err := r.saveGuildToMySQL(ctx, guild); err != nil {
		return err
	}
	if err := r.cacheGuild(ctx, guild); err != nil {
		logx.Errorf("cache guild %d after save: %v", guild.GuildID, err)
	}
	return nil
}

func (r *GuildRepo) SetPlayerGuild(ctx context.Context, playerID, guildID uint64) error {
	if err := r.setPlayerGuildInMySQL(ctx, playerID, guildID); err != nil {
		return err
	}
	r.rdb.Set(ctx, playerGuildKey(playerID), guildID, r.defaultTTL)
	return nil
}

func (r *GuildRepo) RemovePlayerGuild(ctx context.Context, playerID uint64) error {
	if err := r.removePlayerGuildInMySQL(ctx, playerID); err != nil {
		return err
	}
	r.rdb.Del(ctx, playerGuildKey(playerID))
	return nil
}

func (r *GuildRepo) DeleteGuild(ctx context.Context, guildID uint64) error {
	if err := r.deleteGuildFromMySQL(ctx, guildID); err != nil {
		return err
	}
	r.rdb.Del(ctx, guildKey(guildID))
	return nil
}

// ── Cache helper ───────────────────────────────────────────────

func (r *GuildRepo) cacheGuild(ctx context.Context, guild *GuildData) error {
	data, err := json.Marshal(guild)
	if err != nil {
		return err
	}
	return r.rdb.Set(ctx, guildKey(guild.GuildID), data, r.defaultTTL).Err()
}

// ── MySQL queries ──────────────────────────────────────────────

func (r *GuildRepo) loadGuildFromMySQL(ctx context.Context, guildID uint64) (*GuildData, error) {
	row := r.db.QueryRowContext(ctx,
		"SELECT guild_id, name, leader_id, level, announcement, create_time_ms, max_members, zone_id FROM guild WHERE guild_id = ?",
		guildID)

	var guild GuildData
	err := row.Scan(&guild.GuildID, &guild.Name, &guild.LeaderID, &guild.Level,
		&guild.Announcement, &guild.CreateTimeMs, &guild.MaxMembers, &guild.ZoneID)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, fmt.Errorf("query guild %d: %w", guildID, err)
	}

	// Load members
	rows, err := r.db.QueryContext(ctx,
		"SELECT player_id, role, join_time_ms, last_active_ms, contribution FROM guild_member WHERE guild_id = ?",
		guildID)
	if err != nil {
		return nil, fmt.Errorf("query guild members %d: %w", guildID, err)
	}
	defer rows.Close()

	for rows.Next() {
		var m MemberData
		if err := rows.Scan(&m.PlayerID, &m.Role, &m.JoinTimeMs, &m.LastActiveMs, &m.Contribution); err != nil {
			return nil, fmt.Errorf("scan guild member: %w", err)
		}
		guild.Members = append(guild.Members, m)
	}

	return &guild, nil
}

func (r *GuildRepo) loadPlayerGuildFromMySQL(ctx context.Context, playerID uint64) (uint64, error) {
	var guildID uint64
	err := r.db.QueryRowContext(ctx,
		"SELECT guild_id FROM guild_member WHERE player_id = ?",
		playerID).Scan(&guildID)
	if err == sql.ErrNoRows {
		return 0, nil
	}
	if err != nil {
		return 0, fmt.Errorf("query player guild %d: %w", playerID, err)
	}
	return guildID, nil
}

func (r *GuildRepo) saveGuildToMySQL(ctx context.Context, guild *GuildData) error {
	_, err := r.db.ExecContext(ctx,
		`INSERT INTO guild (guild_id, name, leader_id, level, announcement, create_time_ms, max_members, zone_id)
		 VALUES (?, ?, ?, ?, ?, ?, ?, ?)
		 ON DUPLICATE KEY UPDATE name=VALUES(name), leader_id=VALUES(leader_id), level=VALUES(level),
		   announcement=VALUES(announcement), max_members=VALUES(max_members), zone_id=VALUES(zone_id)`,
		guild.GuildID, guild.Name, guild.LeaderID, guild.Level,
		guild.Announcement, guild.CreateTimeMs, guild.MaxMembers, guild.ZoneID)
	return err
}

func (r *GuildRepo) setPlayerGuildInMySQL(ctx context.Context, playerID, guildID uint64) error {
	_, err := r.db.ExecContext(ctx,
		`INSERT INTO guild_member (guild_id, player_id, role, join_time_ms, last_active_ms, contribution)
		 VALUES (?, ?, 0, ?, ?, 0)
		 ON DUPLICATE KEY UPDATE guild_id=VALUES(guild_id)`,
		guildID, playerID, time.Now().UnixMilli(), time.Now().UnixMilli())
	return err
}

func (r *GuildRepo) removePlayerGuildInMySQL(ctx context.Context, playerID uint64) error {
	_, err := r.db.ExecContext(ctx, "DELETE FROM guild_member WHERE player_id = ?", playerID)
	return err
}

func (r *GuildRepo) deleteGuildFromMySQL(ctx context.Context, guildID uint64) error {
	tx, err := r.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	if _, err := tx.ExecContext(ctx, "DELETE FROM guild_member WHERE guild_id = ?", guildID); err != nil {
		return err
	}
	if _, err := tx.ExecContext(ctx, "DELETE FROM guild WHERE guild_id = ?", guildID); err != nil {
		return err
	}
	return tx.Commit()
}

// ── Ranking (Redis ZSET) ───────────────────────────────────────

// RankEntry is a guild's position in the leaderboard.
type RankEntry struct {
	GuildID uint64
	Score   int64
	Rank    uint32 // 1-based
}

// UpdateGuildScore sets or updates a guild's score in both global and per-zone ranking ZSETs.
func (r *GuildRepo) UpdateGuildScore(ctx context.Context, guildID uint64, zoneID uint32, score int64) error {
	z := redis.Z{Score: float64(score), Member: guildID}
	pipe := r.rdb.Pipeline()
	pipe.ZAdd(ctx, guildRankKey, z)
	if zoneID > 0 {
		pipe.ZAdd(ctx, zoneRankKey(zoneID), z)
	}
	_, err := pipe.Exec(ctx)
	return err
}

// RemoveGuildFromRank removes a guild from both global and per-zone ranking ZSETs.
func (r *GuildRepo) RemoveGuildFromRank(ctx context.Context, guildID uint64, zoneID uint32) error {
	pipe := r.rdb.Pipeline()
	pipe.ZRem(ctx, guildRankKey, guildID)
	if zoneID > 0 {
		pipe.ZRem(ctx, zoneRankKey(zoneID), guildID)
	}
	_, err := pipe.Exec(ctx)
	return err
}

// GetGuildRankPage returns a page of guilds sorted by score descending.
// page is 1-based; zoneID=0 means global ranking.
func (r *GuildRepo) GetGuildRankPage(ctx context.Context, zoneID, page, pageSize uint32) ([]RankEntry, uint32, error) {
	key := guildRankKey
	if zoneID > 0 {
		key = zoneRankKey(zoneID)
	}

	total, err := r.rdb.ZCard(ctx, key).Result()
	if err != nil {
		return nil, 0, fmt.Errorf("zcard %s: %w", key, err)
	}
	if total == 0 || page == 0 || pageSize == 0 {
		return nil, uint32(total), nil
	}

	start := int64((page - 1) * pageSize)
	stop := start + int64(pageSize) - 1

	members, err := r.rdb.ZRevRangeWithScores(ctx, key, start, stop).Result()
	if err != nil {
		return nil, 0, fmt.Errorf("zrevrange %s: %w", key, err)
	}

	entries := make([]RankEntry, 0, len(members))
	for i, z := range members {
		guildID, _ := strconv.ParseUint(fmt.Sprintf("%v", z.Member), 10, 64)
		entries = append(entries, RankEntry{
			GuildID: guildID,
			Score:   int64(z.Score),
			Rank:    uint32(start) + uint32(i) + 1,
		})
	}
	return entries, uint32(total), nil
}

// GetGuildRank returns a single guild's rank and score. zoneID=0 means global. Rank is 1-based (0 = not ranked).
func (r *GuildRepo) GetGuildRank(ctx context.Context, guildID uint64, zoneID uint32) (RankEntry, error) {
	key := guildRankKey
	if zoneID > 0 {
		key = zoneRankKey(zoneID)
	}

	member := fmt.Sprintf("%d", guildID)

	// ZREVRANK: 0-based index in descending order
	rank, err := r.rdb.ZRevRank(ctx, key, member).Result()
	if err == redis.Nil {
		return RankEntry{}, nil // not in ranking
	}
	if err != nil {
		return RankEntry{}, fmt.Errorf("zrevrank guild %d: %w", guildID, err)
	}

	score, err := r.rdb.ZScore(ctx, key, member).Result()
	if err != nil {
		return RankEntry{}, fmt.Errorf("zscore guild %d: %w", guildID, err)
	}

	return RankEntry{
		GuildID: guildID,
		Score:   int64(score),
		Rank:    uint32(rank) + 1, // 1-based
	}, nil
}
