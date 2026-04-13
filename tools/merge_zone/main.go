package main

import (
	"context"
	"database/sql"
	"flag"
	"fmt"
	"log"
	"os"
	"strconv"
	"time"

	_ "github.com/go-sql-driver/mysql"
	"github.com/redis/go-redis/v9"
)

// merge_zone migrates guild & friend data from a source zone into a target zone.
//
// What it does:
//   1. MySQL: UPDATE guild SET zone_id = <target> WHERE zone_id = <source>
//   2. Redis: ZRANGEBYSCORE on guild_rank:zone:<source> → ZADD into guild_rank:zone:<target> → DEL source key
//   3. Logs name-conflict guilds (same guild name in target zone) for manual resolution
//
// Safe to re-run: idempotent (MySQL UPDATE is a no-op for already-migrated rows,
// Redis ZADD overwrites existing scores, DEL on empty key is a no-op).

func main() {
	sourceZone := flag.Uint("source-zone", 0, "Source zone ID to merge FROM (required)")
	targetZone := flag.Uint("target-zone", 0, "Target zone ID to merge INTO (required)")
	mysqlDSN := flag.String("mysql-dsn", "root:@tcp(127.0.0.1:3306)/mmorpg?charset=utf8mb4&parseTime=true&loc=Local", "MySQL data source name")
	redisAddr := flag.String("redis-addr", "127.0.0.1:6379", "Redis address")
	redisPassword := flag.String("redis-password", "", "Redis password")
	redisDB := flag.Int("redis-db", 2, "Redis DB number (guild service)")
	dryRun := flag.Bool("dry-run", false, "Print what would be done without executing")
	flag.Parse()

	if *sourceZone == 0 || *targetZone == 0 {
		fmt.Fprintln(os.Stderr, "ERROR: --source-zone and --target-zone are required (non-zero)")
		flag.Usage()
		os.Exit(1)
	}
	if *sourceZone == *targetZone {
		fmt.Fprintln(os.Stderr, "ERROR: source and target zone must be different")
		os.Exit(1)
	}

	src := uint32(*sourceZone)
	dst := uint32(*targetZone)

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
	defer cancel()

	// ── Connect MySQL ──
	db, err := sql.Open("mysql", *mysqlDSN)
	if err != nil {
		log.Fatalf("mysql open: %v", err)
	}
	defer db.Close()
	if err := db.PingContext(ctx); err != nil {
		log.Fatalf("mysql ping: %v", err)
	}

	// ── Connect Redis ──
	rdb := redis.NewClient(&redis.Options{
		Addr:     *redisAddr,
		Password: *redisPassword,
		DB:       *redisDB,
	})
	defer rdb.Close()
	if err := rdb.Ping(ctx).Err(); err != nil {
		log.Fatalf("redis ping: %v", err)
	}

	log.Printf("=== Merge Zone %d → Zone %d (dry-run=%v) ===", src, dst, *dryRun)

	// ── Step 1: Check name conflicts ──
	conflicts, err := checkNameConflicts(ctx, db, src, dst)
	if err != nil {
		log.Fatalf("check name conflicts: %v", err)
	}
	if len(conflicts) > 0 {
		log.Printf("WARNING: %d guild name conflicts found (same name exists in target zone):", len(conflicts))
		for _, c := range conflicts {
			log.Printf("  - guild_id=%d name=%q (source zone %d) conflicts with guild_id=%d (target zone %d)",
				c.srcGuildID, c.name, src, c.dstGuildID, dst)
		}
		log.Println("These guilds will NOT be migrated. Resolve manually (rename/disband) before re-running.")
	}

	// ── Step 2: MySQL — migrate guild zone_id ──
	guildCount, err := migrateGuildZone(ctx, db, src, dst, conflicts, *dryRun)
	if err != nil {
		log.Fatalf("migrate guild zone: %v", err)
	}
	log.Printf("MySQL: %d guilds migrated from zone %d → %d", guildCount, src, dst)

	// ── Step 3: Redis — merge ranking ZSET ──
	rankCount, err := mergeRankingZSET(ctx, rdb, src, dst, *dryRun)
	if err != nil {
		log.Fatalf("merge ranking zset: %v", err)
	}
	log.Printf("Redis: %d ranking entries merged from guild_rank:zone:%d → guild_rank:zone:%d", rankCount, src, dst)

	log.Printf("=== Merge complete ===")
}

type nameConflict struct {
	srcGuildID uint64
	dstGuildID uint64
	name       string
}

func checkNameConflicts(ctx context.Context, db *sql.DB, src, dst uint32) ([]nameConflict, error) {
	rows, err := db.QueryContext(ctx,
		`SELECT s.guild_id, d.guild_id, s.name
		 FROM guild s
		 JOIN guild d ON s.name = d.name AND d.zone_id = ?
		 WHERE s.zone_id = ?`,
		dst, src)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var conflicts []nameConflict
	for rows.Next() {
		var c nameConflict
		if err := rows.Scan(&c.srcGuildID, &c.dstGuildID, &c.name); err != nil {
			return nil, err
		}
		conflicts = append(conflicts, c)
	}
	return conflicts, rows.Err()
}

func migrateGuildZone(ctx context.Context, db *sql.DB, src, dst uint32, conflicts []nameConflict, dryRun bool) (int64, error) {
	if dryRun {
		var count int64
		err := db.QueryRowContext(ctx,
			"SELECT COUNT(*) FROM guild WHERE zone_id = ?", src).Scan(&count)
		if err != nil {
			return 0, err
		}
		log.Printf("[DRY-RUN] Would migrate %d guilds (minus %d conflicts)", count, len(conflicts))
		return count - int64(len(conflicts)), nil
	}

	// Build exclusion list
	query := "UPDATE guild SET zone_id = ? WHERE zone_id = ?"
	args := []any{dst, src}

	if len(conflicts) > 0 {
		excludeIDs := make([]any, len(conflicts))
		placeholders := ""
		for i, c := range conflicts {
			excludeIDs[i] = c.srcGuildID
			if i > 0 {
				placeholders += ","
			}
			placeholders += "?"
		}
		query += " AND guild_id NOT IN (" + placeholders + ")"
		args = append(args, excludeIDs...)
	}

	result, err := db.ExecContext(ctx, query, args...)
	if err != nil {
		return 0, err
	}
	return result.RowsAffected()
}

func mergeRankingZSET(ctx context.Context, rdb *redis.Client, src, dst uint32, dryRun bool) (int, error) {
	srcKey := fmt.Sprintf("guild_rank:zone:%d", src)
	dstKey := fmt.Sprintf("guild_rank:zone:%d", dst)

	// Get all entries from source zone ranking
	members, err := rdb.ZRangeWithScores(ctx, srcKey, 0, -1).Result()
	if err != nil {
		return 0, fmt.Errorf("zrange %s: %w", srcKey, err)
	}

	if len(members) == 0 {
		log.Printf("Redis: source ZSET %s is empty, nothing to merge", srcKey)
		return 0, nil
	}

	if dryRun {
		log.Printf("[DRY-RUN] Would merge %d entries from %s → %s", len(members), srcKey, dstKey)
		for _, z := range members {
			guildID, _ := strconv.ParseUint(fmt.Sprintf("%v", z.Member), 10, 64)
			log.Printf("  guild_id=%d score=%.0f", guildID, z.Score)
		}
		return len(members), nil
	}

	// Pipeline: ZADD all to target, then DEL source
	pipe := rdb.Pipeline()
	zMembers := make([]redis.Z, len(members))
	for i, z := range members {
		zMembers[i] = redis.Z{Score: z.Score, Member: z.Member}
	}
	pipe.ZAdd(ctx, dstKey, zMembers...)
	pipe.Del(ctx, srcKey)

	if _, err := pipe.Exec(ctx); err != nil {
		return 0, fmt.Errorf("pipeline exec: %w", err)
	}

	return len(members), nil
}
