package main

import (
	"context"
	"errors"
	"fmt"
	"log"
	"sort"
	"strconv"
	"strings"
	"sync"

	"github.com/redis/go-redis/v9"
)

// Key pattern must match data_service/internal/logic/data_logic.go (player:{%d}:field).
func playerDataKeyPattern(playerID uint64) string {
	return fmt.Sprintf("player:{%d}:*", playerID)
}

// collectPlayerIDsWithHomeZone scans mapping Redis for keys player:zone:{id} where value == zoneStr.
//
// 每批 SCAN 用一次 MGET 取值,而不是逐键 GET:mapping 里是**全服所有玩家**的
// 映射(不只源区),百万级 key 逐键 GET 就是百万次 RTT。SCAN COUNT=500 配
// 一次 MGET 把往返压到 1/500。mapping Redis 是单实例(data_service.yaml 的
// MappingRedis Type: node),MGET 跨 slot 的问题不存在。
//
// 返回值有序去重:清单要能 diff,分批边界要稳定。
func collectPlayerIDsWithHomeZone(ctx context.Context, rdb *redis.Client, zone uint32) ([]uint64, error) {
	if rdb == nil {
		return nil, errors.New("nil mapping redis handle")
	}
	want := strconv.FormatUint(uint64(zone), 10)
	var out []uint64
	var cur uint64
	for {
		keys, next, err := rdb.Scan(ctx, cur, playerZoneKeyPrefix+"*", mappingScanCount).Result()
		if err != nil {
			return nil, fmt.Errorf("scan %s*: %w", playerZoneKeyPrefix, err)
		}
		if len(keys) > 0 {
			vals, err := rdb.MGet(ctx, keys...).Result()
			if err != nil {
				return nil, fmt.Errorf("mapping mget (%d keys): %w", len(keys), err)
			}
			for i, key := range keys {
				s, ok := vals[i].(string)
				if !ok || s != want { // nil(扫描后被删)或值不匹配
					continue
				}
				pid, perr := strconv.ParseUint(strings.TrimPrefix(key, playerZoneKeyPrefix), 10, 64)
				if perr != nil {
					log.Printf("WARN: skip bad mapping key %q", key)
					continue
				}
				out = append(out, pid)
			}
		}
		cur = next
		if cur == 0 {
			break
		}
	}
	return sortedUint64(out), nil
}

// collectPlayerIDsWithHomeZoneDB 是 collectPlayerIDsWithHomeZone 的库版(R8,-route-source=db):按 home_zone 键集翻页读完
// player_route(idx_route_home,ScanByHome 逐行校验),返回升序去重的 player_id。翻页游标是聚簇键 (route_shard, player_id),
// 与返回值的 player_id 顺序不同,所以最后统一排序。
func collectPlayerIDsWithHomeZoneDB(ctx context.Context, db routeDBOps, zone uint32) ([]uint64, error) {
	var out []uint64
	err := scanZoneHomeRows(ctx, db, zone, func(page []mergeRouteRow) error {
		for _, row := range page {
			out = append(out, row.PlayerID)
		}
		return nil
	})
	if err != nil {
		return nil, fmt.Errorf("scan player_route by home_zone=%d: %w", zone, err)
	}
	return sortedUint64(out), nil
}

// scanZoneHomeRows 按 home_zone 键集翻页一致读 player_route(idx_route_home),每页交给 visit;一页不足 routeReadBatchLimit 即翻完。
func scanZoneHomeRows(ctx context.Context, db routeDBOps, zone uint32, visit func(page []mergeRouteRow) error) error {
	after := routeScanCursor{}
	for {
		page, err := db.ScanByHome(ctx, zone, after, routeReadBatchLimit)
		if err != nil {
			return err
		}
		if err := visit(page); err != nil {
			return err
		}
		if len(page) < routeReadBatchLimit {
			return nil
		}
		after = routeCursorOf(page[len(page)-1].PlayerID)
	}
}

// mappingScanCount 是 mapping Redis 上 SCAN 的批量,也是按 id 分批 MGET / CAS 的批量
// (readPlayerZones、remapPlayerMapping、restoreMappingForIDs 共用),各处耗时可比。
const mappingScanCount = 500

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

// playerDataLockKey 是 data_service 的 per-player 数据锁
// (go/data_service/internal/routing/router.go::PlayerDataLockKey)。
// 它与 player:{id}:* 同 slot,所以会被 SCAN 命中 —— 但**绝不能拷**:
// 目标端拷进去的是一把 **TTL=0 的锁**(SET 不带 EX),data_service 之后
// 对这名玩家的每一次写都会认为锁被别人持着,而它永远不会过期。
// 这名玩家从此在目标区彻底写不进数据,且没有任何日志说明原因。
func playerDataLockKey(playerID uint64) string {
	return fmt.Sprintf("player:{%d}:__lock", playerID)
}

// playerFieldIndexKey 是 data_service 的字段索引(go/data_service/internal/logic/data_logic.go::fieldIndexKey),HASH。
// MGET 对 HASH 回 nil,所以它本来不会被下面的 string 拷贝带走;但目标端若残留同名索引且戳恰好等于拷过去的 __version,
// data_service 会把它当成完整索引,拷过去的字段从全量读 / 删除里消失。拷贝时先删目标端索引,该玩家在目标端走
// data_service 的 SCAN 回退(Cluster 上按 slot 扫单节点,结果正确)。不重建索引:Regions 从未部署(CZ-2)。
func playerFieldIndexKey(playerID uint64) string {
	return fmt.Sprintf("player:{%d}:__fields", playerID)
}

// scanPlayerKeys 列出一名玩家在 src 上的全部 player:{id}:* 键。
//
// Cluster 修正:本工具依赖的 go-redis(见 go.mod)里 ClusterClient.Scan 不带 key 时只会打到**一个**
// 随机节点,漏掉其它分片上的键。player:{id}:* 用了 hash tag,同一名玩家的
// 键确实都在同一个 slot / 节点上 —— 但那是**哪个**节点,客户端在无 key 的
// SCAN 里并不知道。必须 ForEachMaster 扫全部主节点再合并。(go-redis v9.18.0 起 Scan 对带 hash tag 的
// pattern 会按 slot 路由,data_service 用的是那一版;本工具升级前仍以 ForEachMaster 为准。)
//
// ForEachMaster 为每个主节点各起一个 goroutine 并发调用 collect,seen / all 由 mu 保护;迁槽时同一名玩家的键
// 可能同时出现在源、目标两个节点上,两个回调会并发写这两份共享状态。
func scanPlayerKeys(ctx context.Context, src redis.UniversalClient, playerID uint64) ([]string, error) {
	pat := playerDataKeyPattern(playerID)
	var mu sync.Mutex
	seen := map[string]struct{}{}
	var all []string
	collect := func(ctx context.Context, c *redis.Client) error {
		var cur uint64
		for {
			keys, next, err := c.Scan(ctx, cur, pat, 200).Result()
			if err != nil {
				return err
			}
			mu.Lock()
			for _, k := range keys {
				if _, dup := seen[k]; dup {
					continue
				}
				seen[k] = struct{}{}
				all = append(all, k)
			}
			mu.Unlock()
			cur = next
			if cur == 0 {
				return nil
			}
		}
	}
	switch c := src.(type) {
	case *redis.ClusterClient:
		if err := c.ForEachMaster(ctx, collect); err != nil {
			return nil, fmt.Errorf("cluster scan player %d: %w", playerID, err)
		}
	case *redis.Client:
		if err := collect(ctx, c); err != nil {
			return nil, fmt.Errorf("scan player %d: %w", playerID, err)
		}
	default:
		return nil, fmt.Errorf("unsupported redis client type %T", src)
	}
	sort.Strings(all) // 让 dry-run 与 apply 的日志顺序稳定
	return all, nil
}

// copyPlayerStringKeys copies all string keys matching player:{id}:* from src to dst (SET with TTL 0).
// 锁键被显式排除,见 playerDataLockKey;目标端的字段索引先删,见 playerFieldIndexKey。
//
// apply 模式下拷贝之前先查 data_service 的数据锁,锁在就报错、一个键都不拷(调用方走 abortKeepingFence,围栏保留)。
// 前提是 data_service 写门禁(routing.AcquirePlayerWriteLock):写者在数据锁到手之后复核合服围栏,提交脚本校验数据锁 token。
// 于是围栏立起之后某一刻 t 查到锁不在,就说明 t 之前拿到锁的写者要么已提交(下面的 SCAN / MGET 看得到),要么锁已过期、
// 之后的提交会被 token 校验拒绝;t 之后拿到锁的写者复核必然看见围栏而放弃 —— t 之后源端不会再有 player:{id}:* 的提交。
// 检查必须在 SCAN 之前:放在 SCAN 与 MGET 之间,SCAN 之后新建的字段键会漏拷。dry-run 只告警。
// 同一后端时 copyPlayerBlobs 整步跳过,不会走到这里。
func copyPlayerStringKeys(ctx context.Context, src, dst redis.UniversalClient, playerID uint64, dryRun bool) (int, error) {
	lockKey := playerDataLockKey(playerID)
	held, err := src.Exists(ctx, lockKey).Result()
	if err != nil {
		return 0, fmt.Errorf("player %d: check data_service write lock %s: %w", playerID, lockKey, err)
	}
	if held > 0 {
		if !dryRun {
			return 0, fmt.Errorf("player %d: data_service write lock %s is held on the source data Redis: a write that "+
				"passed its merge-fence recheck before the fence went up may still commit after this copy and be lost; "+
				"wait at least PlayerLockTTLSec (3s in etc/data_service.yaml and the K8s ConfigMap) and rerun",
				playerID, lockKey)
		}
		log.Printf("[DRY-RUN] WARN player %d: %s is held on the source data Redis; -apply would stop here", playerID, lockKey)
	}
	scanned, err := scanPlayerKeys(ctx, src, playerID)
	if err != nil {
		return 0, err
	}
	all := make([]string, 0, len(scanned))
	for _, k := range scanned {
		if k == lockKey {
			log.Printf("player %d: skipping %s (copying a lock with no TTL would deadlock the player in the target zone)", playerID, k)
			continue
		}
		all = append(all, k)
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
	pipe.Del(ctx, playerFieldIndexKey(playerID))
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
