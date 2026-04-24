package main

import (
	"context"
	"fmt"
	"log"
	"strconv"
	"strings"

	"github.com/redis/go-redis/v9"
)

// Key pattern must match data_service/internal/logic/data_logic.go (player:{%d}:field).
func playerDataKeyPattern(playerID uint64) string {
	return fmt.Sprintf("player:{%d}:*", playerID)
}

// collectPlayerIDsWithHomeZone scans mapping Redis for keys player:zone:{id} where value == zoneStr.
func collectPlayerIDsWithHomeZone(ctx context.Context, rdb *redis.Client, zone uint32) ([]uint64, error) {
	want := strconv.FormatUint(uint64(zone), 10)
	var out []uint64
	var cur uint64
	for {
		keys, next, err := rdb.Scan(ctx, cur, playerZoneKeyPrefix+"*", 500).Result()
		if err != nil {
			return nil, err
		}
		for _, key := range keys {
			v, err := rdb.Get(ctx, key).Result()
			if err == redis.Nil {
				continue
			}
			if err != nil {
				return nil, fmt.Errorf("mapping get %s: %w", key, err)
			}
			if v != want {
				continue
			}
			idStr := strings.TrimPrefix(key, playerZoneKeyPrefix)
			pid, err := strconv.ParseUint(idStr, 10, 64)
			if err != nil {
				log.Printf("WARN: skip bad mapping key %q", key)
				continue
			}
			out = append(out, pid)
		}
		cur = next
		if cur == 0 {
			break
		}
	}
	return out, nil
}

// newDataRedisClient returns a standalone client (one addr) or cluster client (multiple addrs).
// For Redis Cluster, DB is ignored (always 0).
func newDataRedisClient(addrs []string, password string, db int) redis.UniversalClient {
	if len(addrs) == 0 {
		return nil
	}
	if len(addrs) == 1 {
		return redis.NewClient(&redis.Options{
			Addr:     addrs[0],
			Password: password,
			DB:       db,
		})
	}
	return redis.NewClusterClient(&redis.ClusterOptions{
		Addrs:    addrs,
		Password: password,
	})
}

// copyPlayerStringKeys copies all string keys matching player:{id}:* from src to dst (SET with TTL 0).
func copyPlayerStringKeys(ctx context.Context, src, dst redis.UniversalClient, playerID uint64, dryRun bool) (int, error) {
	pat := playerDataKeyPattern(playerID)
	var all []string
	var cur uint64
	for {
		keys, next, err := src.Scan(ctx, cur, pat, 200).Result()
		if err != nil {
			return 0, fmt.Errorf("scan player %d: %w", playerID, err)
		}
		all = append(all, keys...)
		cur = next
		if cur == 0 {
			break
		}
	}
	if len(all) == 0 {
		return 0, nil
	}
	if dryRun {
		return len(all), nil
	}

	// Read values from source
	vals, err := src.MGet(ctx, all...).Result()
	if err != nil {
		return 0, fmt.Errorf("mget player %d: %w", playerID, err)
	}
	pipe := dst.Pipeline()
	written := 0
	for i, k := range all {
		v := vals[i]
		if v == nil {
			continue
		}
		s, ok := v.(string)
		if !ok {
			return 0, fmt.Errorf("player %d key %s: non-string type", playerID, k)
		}
		pipe.Set(ctx, k, s, 0)
		written++
	}
	if _, err := pipe.Exec(ctx); err != nil {
		return 0, fmt.Errorf("pipeline set player %d: %w", playerID, err)
	}
	return written, nil
}

func addrsFromCSV(s string) []string {
	if strings.TrimSpace(s) == "" {
		return nil
	}
	parts := strings.Split(s, ",")
	var out []string
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if p != "" {
			out = append(out, p)
		}
	}
	return out
}

func sameDataBackend(a, b []string, da, db int) bool {
	if len(a) != len(b) || len(a) == 0 {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return da == db
}
