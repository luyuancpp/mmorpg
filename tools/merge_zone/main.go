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

// merge_zone performs maintenance-window server merge (合服) from source zone into target zone.
//
// Order (重要): when using separate per-zone data Redis, **copy player:{%d}:* string keys
// to the target cluster BEFORE** remapping `player:zone:*`, otherwise DataService would
// read new zone → wrong empty cluster. Dev single-Redis setups auto-skip copy when
// source and target endpoints are equivalent.
//
// Default steps: guild MySQL → guild per-zone ZSET → player:zone remapping. Optional
// first step: player blob copy (see -migrate-player-blobs).
//
// Key prefixes: player:zone: — data_service Router; player:{id}: — data_logic.

const playerZoneKeyPrefix = "player:zone:"

func main() {
	sourceZone := flag.Uint("source-zone", 0, "Source zone ID to merge FROM (required)")
	targetZone := flag.Uint("target-zone", 0, "Target zone ID to merge INTO (required)")
	mysqlDSN := flag.String("mysql-dsn", "root:@tcp(127.0.0.1:3306)/mmorpg?charset=utf8mb4&parseTime=true&loc=Local", "MySQL DSN (guild table)")

	redisAddr := flag.String("redis-addr", "127.0.0.1:6379", "Default Redis address (guild rank + fallbacks)")
	redisPassword := flag.String("redis-password", "", "Default Redis password")
	redisDB := flag.Int("redis-db", 2, "Redis DB number (guild service ZSET)")

	mappingAddr := flag.String("mapping-redis-addr", "", "Player mapping Redis (player:zone:*). Default: same as -redis-addr")
	mappingPassword := flag.String("mapping-redis-password", "", "Player mapping Redis password. Default: -redis-password")
	mappingDB := flag.Int("mapping-redis-db", 0, "Player mapping Redis DB (see data_service mapping redis; often not 0)")

	dryRun := flag.Bool("dry-run", false, "Print actions without writing")
	apply := flag.Bool("apply", false, "Required for any write (except dry-run preview)")

	skipGuild := flag.Bool("skip-guild-mysql", false, "Skip MySQL guild zone_id update")
	skipRank := flag.Bool("skip-guild-rank", false, "Skip Redis guild per-zone ZSET merge")
	skipMapping := flag.Bool("skip-player-mapping", false, "Skip player:zone remapping in mapping Redis")
	migrateBlobs := flag.Bool("migrate-player-blobs", false, "Copy player:{%d}:* string keys from source-zone data Redis to target-zone data Redis (multi-cluster)")

	sourceData := flag.String("source-data-redis", "", "Comma-separated addr(s) for **source** zone player data (standalone or cluster). Required if -migrate-player-blobs")
	targetData := flag.String("target-data-redis", "", "Comma-separated addr(s) for **target** zone player data. Required if -migrate-player-blobs")
	dataPassword := flag.String("data-redis-password", "", "Password for source/target data Redis (overrides -redis-password if set)")
	sourceDataDB := flag.Int("source-data-redis-db", 0, "DB index (standalone only; cluster ignores)")
	targetDataDB := flag.Int("target-data-redis-db", 0, "DB index (standalone only; cluster ignores)")

	skipBlobs := flag.Bool("skip-player-blob-migration", false, "Skip blob step even if -migrate-player-blobs (emergency)")

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
	if !*dryRun && !*apply {
		fmt.Fprintln(os.Stderr, "ERROR: refuse to modify data: pass -dry-run to preview, or -apply to write")
		os.Exit(1)
	}
	if *migrateBlobs {
		if *sourceData == "" || *targetData == "" {
			fmt.Fprintln(os.Stderr, "ERROR: -migrate-player-blobs requires -source-data-redis and -target-data-redis")
			os.Exit(1)
		}
	}

	src := uint32(*sourceZone)
	dst := uint32(*targetZone)

	ttl := 2 * time.Hour
	if *dryRun {
		ttl = 30 * time.Minute
	}
	ctx, cancel := context.WithTimeout(context.Background(), ttl)
	defer cancel()

	mAddr := *mappingAddr
	if mAddr == "" {
		mAddr = *redisAddr
	}
	mPass := *mappingPassword
	if mPass == "" {
		mPass = *redisPassword
	}
	dataPass := *dataPassword
	if dataPass == "" {
		dataPass = *redisPassword
	}

	log.Printf("=== Merge zone %d → %d (dry-run=%v apply=%v) ===", src, dst, *dryRun, *apply)
	log.Printf("    skip guild MySQL=%v skip rank=%v skip mapping=%v migrate_blobs=%v skip_blobs=%v",
		*skipGuild, *skipRank, *skipMapping, *migrateBlobs, *skipBlobs)

	var (
		guildCount     int64
		rankCount      int
		mapMatch       int
		mapUpdated     int
		blobPlayers    int
		blobKeysCopied int
	)

	mapRdb := redis.NewClient(&redis.Options{Addr: mAddr, Password: mPass, DB: *mappingDB})
	defer mapRdb.Close()
	if err := mapRdb.Ping(ctx).Err(); err != nil {
		log.Fatalf("mapping redis ping: %v", err)
	}

	// 0) list players homed in source (needed for blob migration + useful for logs)
	playerIDs, err := collectPlayerIDsWithHomeZone(ctx, mapRdb, src)
	if err != nil {
		log.Fatalf("list players in source zone: %v", err)
	}
	log.Printf("Mapping: %d players with home_zone=%d", len(playerIDs), src)

	// 1) optional: copy per-player string keys to target data Redis (BEFORE remapping)
	if *migrateBlobs && !*skipBlobs {
		sAddrs := addrsFromCSV(*sourceData)
		tAddrs := addrsFromCSV(*targetData)
		if sameDataBackend(sAddrs, tAddrs, *sourceDataDB, *targetDataDB) {
			log.Println("Data Redis: source and target endpoints identical — skipping player blob copy (single backend)")
		} else {
			srcData := newDataRedisClient(sAddrs, dataPass, *sourceDataDB)
			dstData := newDataRedisClient(tAddrs, dataPass, *targetDataDB)
			defer srcData.Close()
			defer dstData.Close()
			if err := srcData.Ping(ctx).Err(); err != nil {
				log.Fatalf("source data redis ping: %v", err)
			}
			if err := dstData.Ping(ctx).Err(); err != nil {
				log.Fatalf("target data redis ping: %v", err)
			}
			for _, pid := range playerIDs {
				n, err := copyPlayerStringKeys(ctx, srcData, dstData, pid, *dryRun)
				if err != nil {
					log.Fatalf("player %d blob copy: %v", pid, err)
				}
				if n > 0 {
					blobKeysCopied += n
					blobPlayers++
				}
			}
			verb := "would copy"
			if *dryRun {
				verb = "would copy (dry-run; key counts are live scans)"
			} else {
				verb = "copied"
			}
			log.Printf("Player blobs: %s keys for %d players (total string keys: %d)", verb, blobPlayers, blobKeysCopied)
		}
	}

	if !*skipGuild {
		db, err := sql.Open("mysql", *mysqlDSN)
		if err != nil {
			log.Fatalf("mysql open: %v", err)
		}
		defer db.Close()
		if err := db.PingContext(ctx); err != nil {
			log.Fatalf("mysql ping: %v", err)
		}
		guildCount, err = doGuildMySQL(ctx, db, src, dst, *dryRun)
		if err != nil {
			log.Fatalf("guild MySQL: %v", err)
		}
		log.Printf("MySQL: %d guild rows %s for zone %d → %d", guildCount, verbWrite(*dryRun), src, dst)
	}

	if !*skipRank {
		rankRdb := redis.NewClient(&redis.Options{Addr: *redisAddr, Password: *redisPassword, DB: *redisDB})
		defer rankRdb.Close()
		if err := rankRdb.Ping(ctx).Err(); err != nil {
			log.Fatalf("guild redis ping: %v", err)
		}
		var rErr error
		rankCount, rErr = mergeRankingZSET(ctx, rankRdb, src, dst, *dryRun)
		if rErr != nil {
			log.Fatalf("merge ranking: %v", rErr)
		}
		log.Printf("Redis rank: %d ZSET members %s guild_rank:zone %d → %d", rankCount, verbWrite(*dryRun), src, dst)
	}

	if !*skipMapping {
		var mErr error
		mapMatch, mapUpdated, mErr = remapPlayerMapping(ctx, mapRdb, src, dst, *dryRun)
		if mErr != nil {
			log.Fatalf("player mapping: %v", mErr)
		}
		log.Printf("Mapping Redis: %d players matched, %d home_zone values %s  (%s db=%d)",
			mapMatch, mapUpdated, verbWrite(*dryRun), mAddr, *mappingDB)
	}

	log.Printf("=== Done (players_in_source=%d guild_rows=%d rank_entries=%d map_matched=%d map_updated=%d blob_players=%d blob_keys=%d) ===",
		len(playerIDs), guildCount, rankCount, mapMatch, mapUpdated, blobPlayers, blobKeysCopied)
}

func verbWrite(dry bool) string {
	if dry {
		return "would be migrated"
	}
	return "migrated"
}

func doGuildMySQL(ctx context.Context, db *sql.DB, src, dst uint32, dryRun bool) (int64, error) {
	conflicts, err := checkNameConflicts(ctx, db, src, dst)
	if err != nil {
		return 0, err
	}
	if len(conflicts) > 0 {
		log.Printf("WARNING: %d guild name conflicts (target zone already has that name). Skipping those source guilds:", len(conflicts))
		for _, c := range conflicts {
			log.Printf("  - guild_id=%d name=%q (source) vs guild_id=%d (target)", c.srcGuildID, c.name, c.dstGuildID)
		}
		log.Println("Resolve manually (rename/disband) then re-run.")
	}
	return migrateGuildZone(ctx, db, src, dst, conflicts, dryRun)
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
	var out []nameConflict
	for rows.Next() {
		var c nameConflict
		if err := rows.Scan(&c.srcGuildID, &c.dstGuildID, &c.name); err != nil {
			return nil, err
		}
		out = append(out, c)
	}
	return out, rows.Err()
}

func migrateGuildZone(ctx context.Context, db *sql.DB, src, dst uint32, conflicts []nameConflict, dryRun bool) (int64, error) {
	if dryRun {
		var count int64
		if err := db.QueryRowContext(ctx, "SELECT COUNT(*) FROM guild WHERE zone_id = ?", src).Scan(&count); err != nil {
			return 0, err
		}
		est := count - int64(len(conflicts))
		if est < 0 {
			est = 0
		}
		log.Printf("[DRY-RUN] Would migrate up to %d guild rows (raw count %d, minus %d name conflicts)", est, count, len(conflicts))
		return est, nil
	}
	query := "UPDATE guild SET zone_id = ? WHERE zone_id = ?"
	args := []any{dst, src}
	if len(conflicts) > 0 {
		excludeIDs := make([]any, len(conflicts))
		ph := ""
		for i, c := range conflicts {
			excludeIDs[i] = c.srcGuildID
			if i > 0 {
				ph += ","
			}
			ph += "?"
		}
		query += " AND guild_id NOT IN (" + ph + ")"
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
	members, err := rdb.ZRangeWithScores(ctx, srcKey, 0, -1).Result()
	if err != nil {
		return 0, fmt.Errorf("zrange %s: %w", srcKey, err)
	}
	if len(members) == 0 {
		log.Printf("Redis: source %s is empty, nothing to merge", srcKey)
		return 0, nil
	}
	if dryRun {
		log.Printf("[DRY-RUN] Would merge %d members %s → %s", len(members), srcKey, dstKey)
		return len(members), nil
	}
	pipe := rdb.Pipeline()
	zm := make([]redis.Z, len(members))
	for i, z := range members {
		zm[i] = redis.Z{Score: z.Score, Member: z.Member}
	}
	pipe.ZAdd(ctx, dstKey, zm...)
	pipe.Del(ctx, srcKey)
	if _, err := pipe.Exec(ctx); err != nil {
		return 0, err
	}
	return len(members), nil
}

func remapPlayerMapping(ctx context.Context, rdb *redis.Client, src, dst uint32, dryRun bool) (matched, updated int, err error) {
	sVal := strconv.FormatUint(uint64(src), 10)
	dVal := strconv.FormatUint(uint64(dst), 10)
	var cur uint64
	for {
		var keys []string
		keys, cur, err = rdb.Scan(ctx, cur, playerZoneKeyPrefix+"*", 500).Result()
		if err != nil {
			return 0, 0, err
		}
		for _, key := range keys {
			v, e := rdb.Get(ctx, key).Result()
			if e == redis.Nil {
				continue
			}
			if e != nil {
				return matched, updated, fmt.Errorf("mapping get %q: %w", key, e)
			}
			if v != sVal {
				continue
			}
			matched++
			if dryRun {
				continue
			}
			if e := rdb.Set(ctx, key, dVal, 0).Err(); e != nil {
				return matched, updated, e
			}
			updated++
		}
		if cur == 0 {
			break
		}
	}
	if dryRun {
		updated = 0
	}
	return matched, updated, nil
}
