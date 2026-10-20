package main

// 本文件是本工具读「玩家归属映射」的全部读法(D1 第 3 批,docs/handoff/launch-readiness-20261013/spec_D1_batch1b2_2_3.md §5.5;
// 提案 docs/design/player-route-mapping-durability-proposal.md §5 R7 / R8)。
//
// 两种口径:
//   - db(给了 -route-db-dsn):读全局库 player_route —— db_first 下映射的真源。SQL 照抄提案 §11.2 G1a「后续批次的读方模板」:
//     按 zone 枚举走 idx_route_home、键集游标 (route_shard, player_id);按 id 点查走主键 (route_shard, player_id)。
//   - redis(缺省):今天的 SCAN player:zone:* 口径。db_first 下 Redis 只是缓存,缺键 / 旧值都不权威,
//     报告里固定打印 "route source: redis (not authoritative under db_first)"。
//
// 两种口径的共同纪律:读失败一律报错,绝不把「没读到」当成「没有归属」;值形状不合规的条目计数并在报告里列出,不静默跳过。
//
// 本 module 不依赖 go/shared(避免 go.mod / go.sum 引入 shared 的整棵依赖树):行片数与 home_zone 上限镜像在这里,
// 由 route_source_test.go 的 TestMirroredRouteConstantsMatchSources 逐字核对源头。

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strconv"

	"github.com/redis/go-redis/v9"
)

// routeRowShards 镜像 shared/placement.RouteRowShards:player_route.route_shard = player_id % 64(chk_route_shard 钉住)。
const routeRowShards uint64 = 64

// maxHomeZone 镜像 player_route 的 chk_route_home(home_zone BETWEEN 1 AND 999999)。
// CHECK 在 MySQL < 8.0.16 / TiDB 未开 tidb_enable_check_constraint 时不生效,所以读方仍按它判畸形。
const maxHomeZone uint32 = 999999

// routePageSize 是按 zone 枚举时每页的行数。
const routePageSize = 2000

// shapeSampleLimit 是报告里列出的畸形样本上限。
const shapeSampleLimit = 10

// routeSource 是各检查对「玩家归属」的全部读法。
type routeSource interface {
	// Describe 是报告里的 "route source: ..." 一行(不含口令)。
	Describe() string
	// LiveZones 枚举出现过的归属区。值形状不合规的条目不计入活区,计进 zoneShapeReport。
	LiveZones(ctx context.Context) (map[uint32]struct{}, zoneShapeReport, error)
	// CountPlayers 返回有归属记录的玩家数。
	CountPlayers(ctx context.Context) (int64, error)
	// HasHome 报告 playerID 是否有归属记录。error 时调用方整条检查判失败,绝不当成「没有归属」。
	HasHome(ctx context.Context, playerID uint64) (bool, error)
}

// zoneShapeReport 是 LiveZones 顺带做的值形状检查。
type zoneShapeReport struct {
	// Subject 说明计的是什么(redis:player:zone:* 键;db:不同的 home_zone 取值)。
	Subject   string
	Scanned   int64
	Malformed int64
	// Vanished 只在 redis 口径出现:SCAN 列出、MGET 时已被删的键(正常并发),只报不算畸形。
	Vanished int64
	Samples  []string
}

func (r *zoneShapeReport) addMalformed(sample string) {
	r.Malformed++
	if len(r.Samples) < shapeSampleLimit {
		r.Samples = append(r.Samples, sample)
	}
}

// parseHomeZoneValue 在 shared/placement.ParseHomeZone 的口径(十进制、放得进 uint32、不为 0)上多一条上限 maxHomeZone,
// 与 db 口径判畸形的范围一致:库存不下的值出现在缓存里,说明有写者绕过了库。
// 键存在而值为空串也算畸形(ParseHomeZone 把空串当缺席,是因为它的调用方用空串表示「键不存在」)。
func parseHomeZoneValue(s string) (uint32, bool) {
	z, err := strconv.ParseUint(s, 10, 32)
	if err != nil || z == 0 || z > uint64(maxHomeZone) {
		return 0, false
	}
	return uint32(z), true
}

// ── redis 口径 ────────────────────────────────────────────────────

// mappingRedis 是 redis 口径用到的四个命令,抽出来让单测不必起 Redis(本 module 没有 miniredis 依赖)。
type mappingRedis interface {
	Scan(ctx context.Context, cursor uint64, match string, count int64) ([]string, uint64, error)
	MGet(ctx context.Context, keys ...string) ([]any, error)
	Type(ctx context.Context, key string) (string, error)
	Exists(ctx context.Context, key string) (int64, error)
}

// goRedisMapping 是 mappingRedis 的 go-redis 实现。
type goRedisMapping struct {
	rdb *redis.Client
}

func (m goRedisMapping) Scan(ctx context.Context, cursor uint64, match string, count int64) ([]string, uint64, error) {
	return m.rdb.Scan(ctx, cursor, match, count).Result()
}

func (m goRedisMapping) MGet(ctx context.Context, keys ...string) ([]any, error) {
	return m.rdb.MGet(ctx, keys...).Result()
}

func (m goRedisMapping) Type(ctx context.Context, key string) (string, error) {
	return m.rdb.Type(ctx, key).Result()
}

func (m goRedisMapping) Exists(ctx context.Context, key string) (int64, error) {
	return m.rdb.Exists(ctx, key).Result()
}

const homeZoneKeyPattern = "player:zone:*"

func homeZoneKey(playerID uint64) string {
	return "player:zone:" + strconv.FormatUint(playerID, 10)
}

type redisRouteSource struct {
	r mappingRedis
}

func (s redisRouteSource) Describe() string {
	return "route source: redis (not authoritative under db_first)"
}

// LiveZones 扫 player:zone:* 并 MGET 值。MGET 回 nil 的键再 TYPE 一次:none = SCAN 之后被删(并发,只计 Vanished);
// 其余类型 = 键不是 string(畸形)。
func (s redisRouteSource) LiveZones(ctx context.Context) (map[uint32]struct{}, zoneShapeReport, error) {
	out := map[uint32]struct{}{}
	rep := zoneShapeReport{Subject: "player:zone:* keys"}
	var cur uint64
	for {
		keys, next, err := s.r.Scan(ctx, cur, homeZoneKeyPattern, 500)
		if err != nil {
			return nil, rep, fmt.Errorf("scan %s: %w", homeZoneKeyPattern, err)
		}
		if len(keys) > 0 {
			vals, err := s.r.MGet(ctx, keys...)
			if err != nil {
				return nil, rep, fmt.Errorf("mget %d keys: %w", len(keys), err)
			}
			if len(vals) != len(keys) {
				return nil, rep, fmt.Errorf("mget returned %d values for %d keys", len(vals), len(keys))
			}
			for i, v := range vals {
				rep.Scanned++
				switch val := v.(type) {
				case string:
					z, ok := parseHomeZoneValue(val)
					if !ok {
						rep.addMalformed(fmt.Sprintf("%s=%q", keys[i], truncateSample(val)))
						continue
					}
					out[z] = struct{}{}
				case nil:
					typ, err := s.r.Type(ctx, keys[i])
					if err != nil {
						return nil, rep, fmt.Errorf("type %s: %w", keys[i], err)
					}
					if typ == "none" {
						rep.Vanished++
						continue
					}
					rep.addMalformed(fmt.Sprintf("%s (type=%s)", keys[i], typ))
				default:
					return nil, rep, fmt.Errorf("mget value for %s has unexpected type %T", keys[i], v)
				}
			}
		}
		cur = next
		if cur == 0 {
			break
		}
	}
	return out, rep, nil
}

func (s redisRouteSource) CountPlayers(ctx context.Context) (int64, error) {
	var n int64
	var cur uint64
	for {
		keys, next, err := s.r.Scan(ctx, cur, homeZoneKeyPattern, 500)
		if err != nil {
			return 0, fmt.Errorf("scan %s: %w", homeZoneKeyPattern, err)
		}
		n += int64(len(keys))
		cur = next
		if cur == 0 {
			return n, nil
		}
	}
}

func (s redisRouteSource) HasHome(ctx context.Context, playerID uint64) (bool, error) {
	n, err := s.r.Exists(ctx, homeZoneKey(playerID))
	if err != nil {
		return false, fmt.Errorf("exists %s: %w", homeZoneKey(playerID), err)
	}
	return n > 0, nil
}

// truncateSample 截短畸形值,报告里不铺开整段二进制。
func truncateSample(s string) string {
	const n = 32
	if len(s) > n {
		return s[:n] + "..."
	}
	return s
}

// ── db 口径 ───────────────────────────────────────────────────────

// routeKey 是 player_route 的聚簇主键,也是按 zone 枚举的键集游标。
type routeKey struct {
	Shard    uint64
	PlayerID uint64
}

func (k routeKey) less(o routeKey) bool {
	return k.Shard < o.Shard || (k.Shard == o.Shard && k.PlayerID < o.PlayerID)
}

// routeTable 是 db 口径用到的三条只读查询;sqlRouteTable 是生产实现,单测用假实现驱动 dbRouteSource 的循环与判定。
type routeTable interface {
	// NextHomeZone 返回不小于 from 的最小 home_zone(from 取 0 表示从头);没有了回 ok=false。
	// 参数取「不小于」而不是「大于 -1」:不让负数常量去比 int unsigned 列,两种引擎的有无符号比较口径不必再核对。
	NextHomeZone(ctx context.Context, from uint64) (zone uint32, ok bool, err error)
	// PageByHome 返回 home_zone = zone 且主键大于 after 的至多 limit 行的主键,按 (route_shard, player_id) 升序。
	PageByHome(ctx context.Context, zone uint32, after routeKey, limit int) ([]routeKey, error)
	// HomeOf 点查 playerID 的 home_zone;无行或 home_zone 为 NULL 回 present=false。
	HomeOf(ctx context.Context, playerID uint64) (zone uint32, present bool, err error)
}

// SQL 照抄提案 §11.2 G1a「后续批次的读方模板」。
//
//   - sqlNextHomeZone:在 idx_route_home 上跳着取下一个 home_zone(每次一个范围起点 + LIMIT 1,次数 = zone 数),
//     不用 SELECT DISTINCT:TiDB 没有 loose index scan,DISTINCT 会扫完整条索引;
//   - sqlPageByHome:按 zone 枚举的模板原文(键集游标 (route_shard, player_id),起点 {0, 0},走 idx_route_home、无 filesort);
//     游标谓词写成 OR 展开而不是行比较 —— MySQL 不对行比较做 range 优化;
//   - sqlHomeOf:「按 id 回源」的不加锁形态,主键点查。
const (
	sqlNextHomeZone = "SELECT home_zone FROM player_route WHERE home_zone >= ? ORDER BY home_zone LIMIT 1"
	sqlPageByHome   = "SELECT route_shard, player_id FROM player_route WHERE home_zone = ? AND (route_shard > ? OR (route_shard = ? AND player_id > ?)) ORDER BY route_shard, player_id LIMIT ?"
	sqlHomeOf       = "SELECT home_zone FROM player_route WHERE route_shard = ? AND player_id = ?"
)

type sqlRouteTable struct {
	db *sql.DB
}

func (t sqlRouteTable) NextHomeZone(ctx context.Context, from uint64) (uint32, bool, error) {
	var zone uint32
	err := t.db.QueryRowContext(ctx, sqlNextHomeZone, from).Scan(&zone)
	if errors.Is(err, sql.ErrNoRows) {
		return 0, false, nil
	}
	if err != nil {
		return 0, false, fmt.Errorf("next home_zone from %d: %w", from, err)
	}
	return zone, true, nil
}

func (t sqlRouteTable) PageByHome(ctx context.Context, zone uint32, after routeKey, limit int) ([]routeKey, error) {
	rows, err := t.db.QueryContext(ctx, sqlPageByHome, zone, after.Shard, after.Shard, after.PlayerID, limit)
	if err != nil {
		return nil, fmt.Errorf("page player_route home_zone=%d after (%d, %d): %w", zone, after.Shard, after.PlayerID, err)
	}
	defer rows.Close()
	page := make([]routeKey, 0, limit)
	for rows.Next() {
		var k routeKey
		if err := rows.Scan(&k.Shard, &k.PlayerID); err != nil {
			return nil, fmt.Errorf("scan player_route row home_zone=%d: %w", zone, err)
		}
		page = append(page, k)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate player_route home_zone=%d: %w", zone, err)
	}
	return page, nil
}

func (t sqlRouteTable) HomeOf(ctx context.Context, playerID uint64) (uint32, bool, error) {
	var home sql.NullInt64
	err := t.db.QueryRowContext(ctx, sqlHomeOf, playerID%routeRowShards, playerID).Scan(&home)
	if errors.Is(err, sql.ErrNoRows) {
		return 0, false, nil
	}
	if err != nil {
		return 0, false, fmt.Errorf("point read player_route player_id=%d: %w", playerID, err)
	}
	if !home.Valid {
		return 0, false, nil
	}
	return uint32(home.Int64), true, nil
}

type dbRouteSource struct {
	t      routeTable
	dbName string
}

func (s dbRouteSource) Describe() string {
	return fmt.Sprintf("route source: db (player_route in database %q, authoritative under db_first)", s.dbName)
}

// homeZones 依次取出全部不同的 home_zone(含越界值,由调用方判)。返回值严格递增,否则报错(防排序规则 / 驱动异常时死循环)。
func (s dbRouteSource) homeZones(ctx context.Context) ([]uint32, error) {
	var zones []uint32
	var from uint64
	for {
		zone, ok, err := s.t.NextHomeZone(ctx, from)
		if err != nil {
			return nil, err
		}
		if !ok {
			return zones, nil
		}
		if uint64(zone) < from {
			return nil, fmt.Errorf("next home_zone from %d returned %d (not increasing)", from, zone)
		}
		zones = append(zones, zone)
		from = uint64(zone) + 1
	}
}

// LiveZones:home_zone 为 0 或大于 maxHomeZone 的取值是畸形(CHECK 未生效的库才可能出现),不计入活区。
func (s dbRouteSource) LiveZones(ctx context.Context) (map[uint32]struct{}, zoneShapeReport, error) {
	rep := zoneShapeReport{Subject: "distinct player_route.home_zone values"}
	zones, err := s.homeZones(ctx)
	if err != nil {
		return nil, rep, err
	}
	out := map[uint32]struct{}{}
	for _, z := range zones {
		rep.Scanned++
		if z == 0 || z > maxHomeZone {
			rep.addMalformed(fmt.Sprintf("home_zone=%d", z))
			continue
		}
		out[z] = struct{}{}
	}
	return out, rep, nil
}

// CountPlayers 按 zone 逐页数 home_zone 非 NULL 的行(含越界 zone,与 redis 口径数全部 player:zone:* 键对应)。
func (s dbRouteSource) CountPlayers(ctx context.Context) (int64, error) {
	zones, err := s.homeZones(ctx)
	if err != nil {
		return 0, err
	}
	var n int64
	for _, z := range zones {
		after := routeKey{}
		for {
			page, err := s.t.PageByHome(ctx, z, after, routePageSize)
			if err != nil {
				return 0, err
			}
			if len(page) > routePageSize {
				return 0, fmt.Errorf("page for home_zone=%d returned %d rows, limit %d", z, len(page), routePageSize)
			}
			for _, k := range page {
				if !after.less(k) {
					return 0, fmt.Errorf("page for home_zone=%d is not strictly after cursor (%d, %d)", z, after.Shard, after.PlayerID)
				}
				after = k
			}
			n += int64(len(page))
			if len(page) < routePageSize {
				break
			}
		}
	}
	return n, nil
}

func (s dbRouteSource) HasHome(ctx context.Context, playerID uint64) (bool, error) {
	_, present, err := s.t.HomeOf(ctx, playerID)
	return present, err
}
