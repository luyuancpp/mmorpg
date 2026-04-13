package routing

import (
	"context"
	"fmt"
	"strconv"
	"sync"

	"data_service/internal/config"

	goredis "github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// Router resolves player_id → home_zone_id → Redis client.
// It is the ONLY component that knows about cross-realm topology.
type Router struct {
	mappingRedis *redis.Redis       // global mapping: player_id → home_zone_id
	zoneToClient map[uint32]*goredis.ClusterClient // zone_id → Redis Cluster
	devClient    *goredis.Client    // non-nil in dev mode (single Redis for all)
	mu           sync.RWMutex
	lockTTLSec   int
}

func NewRouter(c config.Config) *Router {
	r := &Router{
		mappingRedis: redis.MustNewRedis(c.MappingRedis),
		zoneToClient: make(map[uint32]*goredis.ClusterClient),
		lockTTLSec:   c.PlayerLockTTLSec,
	}

	// Dev mode: single Redis for all zones
	if c.DevRedis.Host != "" {
		r.devClient = goredis.NewClient(&goredis.Options{
			Addr:     c.DevRedis.Host,
			Password: c.DevRedis.Password,
			DB:       c.DevRedis.DB,
		})
		logx.Infof("[Router] Dev mode: all zones → %s DB=%d", c.DevRedis.Host, c.DevRedis.DB)
		return r
	}

	// Production: build zone → cluster mapping
	for _, region := range c.Regions {
		client := goredis.NewClusterClient(&goredis.ClusterOptions{
			Addrs:    region.Redis.Addrs,
			Password: region.Redis.Password,
		})
		for _, zoneID := range region.Zones {
			r.zoneToClient[zoneID] = client
		}
		logx.Infof("[Router] Region %d: zones %v → cluster %v", region.Id, region.Zones, region.Redis.Addrs)
	}

	return r
}

// ── Player-zone mapping ────────────────────────────────────────

const mappingKeyPrefix = "player:zone:"

func mappingKey(playerID uint64) string {
	return mappingKeyPrefix + strconv.FormatUint(playerID, 10)
}

// RegisterPlayerZone writes the player → home_zone mapping.
func (r *Router) RegisterPlayerZone(ctx context.Context, playerID uint64, homeZoneID uint32) error {
	return r.mappingRedis.SetCtx(ctx, mappingKey(playerID), strconv.FormatUint(uint64(homeZoneID), 10))
}

// GetPlayerHomeZone looks up a player's home zone.
func (r *Router) GetPlayerHomeZone(ctx context.Context, playerID uint64) (uint32, error) {
	val, err := r.mappingRedis.GetCtx(ctx, mappingKey(playerID))
	if err != nil {
		return 0, fmt.Errorf("mapping lookup failed for player %d: %w", playerID, err)
	}
	if val == "" {
		return 0, fmt.Errorf("no home zone mapping for player %d", playerID)
	}
	zoneID, err := strconv.ParseUint(val, 10, 32)
	if err != nil {
		return 0, fmt.Errorf("invalid zone mapping for player %d: %s", playerID, val)
	}
	return uint32(zoneID), nil
}

// DeletePlayerZone removes the player → home_zone mapping.
func (r *Router) DeletePlayerZone(ctx context.Context, playerID uint64) error {
	_, err := r.mappingRedis.DelCtx(ctx, mappingKey(playerID))
	return err
}

// BatchGetPlayerHomeZone returns home zone for multiple players.
func (r *Router) BatchGetPlayerHomeZone(ctx context.Context, playerIDs []uint64) (map[uint64]uint32, error) {
	result := make(map[uint64]uint32, len(playerIDs))
	// Use pipeline for efficiency
	keys := make([]string, len(playerIDs))
	for i, pid := range playerIDs {
		keys[i] = mappingKey(pid)
	}

	vals, err := r.mappingRedis.MgetCtx(ctx, keys...)
	if err != nil {
		return nil, fmt.Errorf("batch mapping lookup failed: %w", err)
	}

	for i, val := range vals {
		if val == "" {
			continue
		}
		zoneID, err := strconv.ParseUint(val, 10, 32)
		if err != nil {
			continue
		}
		result[playerIDs[i]] = uint32(zoneID)
	}
	return result, nil
}

// ── Redis client resolution ────────────────────────────────────

// ClientForPlayer resolves player_id → the correct Redis Cmdable.
// This is the key routing function — all data access goes through here.
func (r *Router) ClientForPlayer(ctx context.Context, playerID uint64) (goredis.Cmdable, error) {
	if r.devClient != nil {
		return r.devClient, nil
	}

	zoneID, err := r.GetPlayerHomeZone(ctx, playerID)
	if err != nil {
		return nil, err
	}

	return r.ClientForZone(zoneID)
}

// ClientForZone returns the Redis client for a specific zone.
func (r *Router) ClientForZone(zoneID uint32) (goredis.Cmdable, error) {
	if r.devClient != nil {
		return r.devClient, nil
	}

	r.mu.RLock()
	client, ok := r.zoneToClient[zoneID]
	r.mu.RUnlock()

	if !ok {
		return nil, fmt.Errorf("no Redis cluster configured for zone %d", zoneID)
	}
	return client, nil
}

// AllZoneIDs returns all configured zone IDs (for server-wide operations).
func (r *Router) AllZoneIDs() []uint32 {
	r.mu.RLock()
	defer r.mu.RUnlock()

	ids := make([]uint32, 0, len(r.zoneToClient))
	for zoneID := range r.zoneToClient {
		ids = append(ids, zoneID)
	}
	return ids
}

// GetAllPlayerIDsInZone scans the global mapping Redis to find all players
// whose home zone matches the given zoneID. This is O(N) over all players
// and should only be used for admin operations (e.g., zone rollback).
func (r *Router) GetAllPlayerIDsInZone(ctx context.Context, zoneID uint32) ([]uint64, error) {
	target := strconv.FormatUint(uint64(zoneID), 10)
	var playerIDs []uint64
	var cursor uint64

	for {
		keys, nextCursor, err := r.mappingRedis.ScanCtx(ctx, cursor, mappingKeyPrefix+"*", 500)
		if err != nil {
			return nil, fmt.Errorf("scan mapping keys: %w", err)
		}

		for _, key := range keys {
			val, err := r.mappingRedis.GetCtx(ctx, key)
			if err != nil || val != target {
				continue
			}
			// Extract player ID from key "player:zone:{ID}"
			idStr := key[len(mappingKeyPrefix):]
			pid, err := strconv.ParseUint(idStr, 10, 64)
			if err != nil {
				continue
			}
			playerIDs = append(playerIDs, pid)
		}

		cursor = nextCursor
		if cursor == 0 {
			break
		}
	}

	return playerIDs, nil
}

// ── Player lock (distributed) ──────────────────────────────────

func playerLockKey(playerID uint64) string {
	return "lock:player:" + strconv.FormatUint(playerID, 10)
}

// AcquirePlayerLock attempts to acquire a per-player distributed lock.
func (r *Router) AcquirePlayerLock(ctx context.Context, playerID uint64) (bool, error) {
	ok, err := r.mappingRedis.SetnxExCtx(ctx, playerLockKey(playerID), "1", r.lockTTLSec)
	return ok, err
}

// ReleasePlayerLock releases the per-player lock.
func (r *Router) ReleasePlayerLock(ctx context.Context, playerID uint64) error {
	_, err := r.mappingRedis.DelCtx(ctx, playerLockKey(playerID))
	return err
}

// Close shuts down all Redis connections.
func (r *Router) Close() {
	if r.devClient != nil {
		r.devClient.Close()
	}
	closed := make(map[*goredis.ClusterClient]bool)
	for _, client := range r.zoneToClient {
		if !closed[client] {
			client.Close()
			closed[client] = true
		}
	}
}
