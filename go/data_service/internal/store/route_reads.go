package store

import (
	"context"
	"database/sql"
	"errors"
	"fmt"

	"shared/placement"
)

// 路由表的只读接口:D1 第 3 批读方门禁的回源读,与第 1b-2 批对账 leader 的围栏镜像对账
// (docs/handoff/launch-readiness-20261013/spec_D1_batch1b2_2_3.md §5.2、§13.1 第 3 条、§13.2、§13.3 第 1 条)。
//
// 本文件**只读**:全部是事务外的一致读(不开事务、不加锁),不写任何表 —— 路由表的写入方仍只有 route_store.go
// (TestNoPlayerRouteWritesOutsideRouteStore / TestRouteMetaAndReconcileWritesOnlyInStoreFiles 用 AST 守住;
// 这里的语句也在它们的扫描范围内,写了就会被拦下)。
//
// SQL 照提案 §11.2 G1a「后续批次的读方模板」:
//   - 按 id 的读带行片(placement.RouteRowShard),走聚簇主键点查;
//   - 按 zone / 落点枚举走 idx_route_home / idx_route_storage,键集游标写成
//     `route_shard > ? OR (route_shard = ? AND player_id > ?)`(不用行比较:MySQL 不对行比较做 range 优化),
//     结果按聚簇键 (route_shard, player_id) 排序,**不是** player_id 序;
//   - 时间一律取库时钟(dbNowMsSQL)。
//
// 围栏读 63 号片(routeFenceReadShard):围栏持有者获取 / 续期 / 释放都写满 0..MaxRouteFenceShards-1 全部分片
// (placement.MaxRouteFenceShards 的契约),所以任一片的内容都等价;取最后一片,是因为 W1 只锁 player_id % FenceShards
// 号片,FenceShards < 64 时永远碰不到它,读它与第 2 批 merge_zone 批事务锁的是同一片(规格 §13.3 第 1 条)。
// 读方不加锁;行不在 = 这个 zone 从未被封锁过(持有者获取围栏前先确保 64 片行都在)。

// routeFenceReadShard 是读方(与第 2 批合服工具的批事务)读 zone_merge_fence 用的分片号 = MaxRouteFenceShards − 1。
const routeFenceReadShard = placement.MaxRouteFenceShards - 1

// 本文件的语句(编号 RD1–RD4,与 route_store.go 的 R1–R22、第 1b-2 批预留的 R23–R30 分开)。
// 抽成常量是为了让契约用例 / 真库用例执行**同一段文本**。
const (
	// RD1 home 与合服围栏一条语句、同一快照(router.go 要求「home 与围栏同一快照」,规格 §5.2):
	// player_route LEFT JOIN zone_merge_fence(home 所在 zone 的 63 号片)。fenced 的取值:
	//   1 = 围栏生效;0 = 未封锁 / 已过期 / 片行不在 / 没有 home(LEFT JOIN 没匹配到,run_id 为 NULL);
	//   NULL = run_id 非空而 expires_at_ms 为 NULL(只有 CHECK 不生效的库上才写得进来),按生效处理
	//   (fail-closed,与 route_store.go 的 R2 同一口径)。
	// 实参:围栏片(63)、行片、player_id。
	routeReadHomeFenceSQL = "SELECT r.home_zone, (f.run_id IS NOT NULL AND f.expires_at_ms > " + dbNowMsSQL + ") AS fenced" +
		" FROM player_route AS r LEFT JOIN zone_merge_fence AS f ON f.zone_id = r.home_zone AND f.shard = ?" +
		" WHERE r.route_shard = ? AND r.player_id = ?"

	// RD2 按 home zone 枚举(R7 GetAllPlayerIDsInZone、第 2 批 W10 的读形态):等值前缀 home_zone + 片前缀键集,
	// 走 idx_route_home (home_zone, route_shard, player_id),ORDER BY 与索引序一致、无 filesort。
	// 实参:zone、游标片、游标片、游标 player_id、limit。
	routeScanByHomeSQL = "SELECT route_shard, player_id, " + routeRowColumnsSQL +
		" FROM player_route WHERE home_zone = ? AND (route_shard > ? OR (route_shard = ? AND player_id > ?))" +
		" ORDER BY route_shard, player_id LIMIT ?"

	// RD3 按落点枚举(relocate 清单等):同 RD2,home_zone 换 storage_id,走 idx_route_storage。
	// 实参:storage、游标片、游标片、游标 player_id、limit。
	routeScanByStorageSQL = "SELECT route_shard, player_id, " + routeRowColumnsSQL +
		" FROM player_route WHERE storage_id = ? AND (route_shard > ? OR (route_shard = ? AND player_id > ?))" +
		" ORDER BY route_shard, player_id LIMIT ?"

	// RD4 列出 63 号片上此刻生效的围栏,连同判生效用的那个库时钟(给第 1b-2 批 leader 的围栏镜像对账)。
	// 左表是只有一行的派生表 n(库时钟):没有生效围栏时也回一行(f.* 全 NULL),调用方照样拿得到库时钟;
	// 有生效围栏时每个 zone 一行(主键 (zone_id, shard) 固定 shard 后 zone_id 唯一),不再有全 NULL 的那一行。
	// 生效判据与 RD1 同口径:run_id 非空,且 expires_at_ms 为 NULL(只有 CHECK 不生效的库上可能,按生效处理)或晚于库时钟。
	// NOW() 在一条语句里取语句开始时刻,所以 now_ms 与判生效用的是同一个值。
	// zone_merge_fence 没有以 shard 开头的索引,这是对 (zone 数 × 已建分片行) 的全表扫:行数 = zone 数 × FenceShards
	// 加上被封锁过的 zone × 64,千 zone 量级是万行级的内存扫描;leader 每拍一次(第 1b-2 批定频率)。
	// 实参:围栏片(63)。
	routeListActiveFencesSQL = "SELECT n.now_ms, f.zone_id, f.run_id, f.purpose, f.src_zone, f.dst_zone," +
		" f.expires_at_ms, f.holder_json, f.fence_epoch" +
		" FROM (SELECT " + dbNowMsSQL + " AS now_ms) AS n" +
		" LEFT JOIN zone_merge_fence AS f ON f.shard = ? AND f.run_id IS NOT NULL" +
		" AND (f.expires_at_ms IS NULL OR f.expires_at_ms > n.now_ms)" +
		" ORDER BY f.zone_id"
)

// ── RD1:按 id 读 home 与围栏 ─────────────────────────────────────────────────────

// RouteHomeFence 是 GetHomeAndFence 的结果:home 与该 home 所在 zone 的合服围栏,出自同一条语句、同一快照。
type RouteHomeFence struct {
	// HomeZone:0 = 没有归属(没有这一行,或 home_zone 为 NULL)—— 此时是库的权威缺席。
	HomeZone uint32
	// Fenced:home 所在 zone 的合服围栏此刻生效(按库时钟判)。HomeZone 为 0 时恒为 false。
	Fenced bool
}

// HasHome 报告这名玩家在库里有没有归属。
func (h RouteHomeFence) HasHome() bool { return h.HomeZone != 0 }

// GetHomeAndFence 按 id 读 home 与围栏(RD1,事务外一致读,不加锁)。第 3 批 R2 GetPlayerHomeZoneAndMergeFence 的回源读。
//
//   - 没有这一行、或 home_zone 为 NULL → 零值、nil(权威缺席,调用方回 NotFound);
//   - SQL 故障 → error(调用方回 Unavailable,**绝不**当成缺席);
//   - 库里的 home_zone 不在 [1, RouteHomeZoneMax](只有 CHECK 不生效时才写得进来)→ error(fail-closed,不把坏值交给路由);
//   - playerID 为 0 → 零值、nil,不发语句(与 GetRoutes 跳过 0 同口径:合法的行不可能是 0)。
//
// 按「正确的片 + id」点查,错片行(只有 CHECK 曾经不生效时才写得进来)在这里表现为缺席,同 GetRoutes 的说明。
func (s *RouteStore) GetHomeAndFence(ctx context.Context, playerID uint64) (RouteHomeFence, error) {
	if playerID == 0 {
		return RouteHomeFence{}, nil
	}
	var home sql.Null[uint32]
	var fenced sql.Null[int64]
	err := s.db.QueryRowContext(ctx, routeReadHomeFenceSQL,
		uint64(routeFenceReadShard), routeShardArg(playerID), playerID).Scan(&home, &fenced)
	if errors.Is(err, sql.ErrNoRows) {
		return RouteHomeFence{}, nil
	}
	if err != nil {
		return RouteHomeFence{}, fmt.Errorf("read player_route home and merge fence for player %d: %w", playerID, err)
	}
	if !home.Valid {
		return RouteHomeFence{}, nil
	}
	if home.V == 0 || home.V > RouteHomeZoneMax {
		return RouteHomeFence{}, fmt.Errorf("%w: player_route player_id=%d home_zone=%d not in [1, %d]",
			placement.ErrMalformed, playerID, home.V, RouteHomeZoneMax)
	}
	// fenced 为 NULL 只可能是「run_id 非空而 expires_at_ms 为 NULL」:按生效处理(见 RD1 的注释)。
	return RouteHomeFence{HomeZone: home.V, Fenced: !fenced.Valid || fenced.V != 0}, nil
}

// ── RD2 / RD3:按 zone / 落点键集翻页 ─────────────────────────────────────────────

// ScanRoutesByHome 按 home zone 键集翻页(RD2,事务外一致读):返回 home_zone = zone 且聚簇键 > after 的至多 limit 行,
// 按 (route_shard, player_id) 升序。下一页从 RouteScanCursorAt(最后一行) 接着翻,起点是零值游标;
// 一页不足 limit 行即翻完。
//
// zone 必须在 [1, RouteHomeZoneMax]、limit 在 [1, RouteBatchLimit]、after.Shard < placement.RouteRowShards,
// 否则 ErrRouteInvalidArgument、不发语句。返回的每一行都校验:库里存的片必须等于 player_id 的片
// (否则 ErrRouteShardMismatch)、home_zone 必须等于 zone、键必须严格大于 after 且严格递增 —— 任一不符整页作废
// (fail-closed:错的一页交给调用方,就是一份静默缺人或重复的名单)。
func (s *RouteStore) ScanRoutesByHome(ctx context.Context, zone uint32, after RouteScanCursor, limit int) ([]RouteRow, error) {
	if zone == 0 || zone > RouteHomeZoneMax {
		return nil, fmt.Errorf("%w: scan by home_zone %d: not in [1, %d]", ErrRouteInvalidArgument, zone, RouteHomeZoneMax)
	}
	return s.scanRoutesByIndex(ctx, "home_zone", routeScanByHomeSQL, uint64(zone), after, limit,
		func(row RouteRow) bool { return row.HomeZone == zone })
}

// ScanRoutesByStorage 按落点键集翻页(RD3):同 ScanRoutesByHome,条件换成 storage_id = storage(storage 必须非 0),
// 每一行校验 storage_id 等于 storage。
func (s *RouteStore) ScanRoutesByStorage(ctx context.Context, storage uint32, after RouteScanCursor, limit int) ([]RouteRow, error) {
	if storage == 0 {
		return nil, fmt.Errorf("%w: scan by storage_id 0", ErrRouteInvalidArgument)
	}
	return s.scanRoutesByIndex(ctx, "storage_id", routeScanByStorageSQL, uint64(storage), after, limit,
		func(row RouteRow) bool { return row.StorageID == storage })
}

// scanRoutesByIndex 是 RD2 / RD3 的公共部分。column 只进错误信息;matches 校验每一行确实满足等值条件。
func (s *RouteStore) scanRoutesByIndex(ctx context.Context, column, query string, key uint64, after RouteScanCursor, limit int,
	matches func(RouteRow) bool) ([]RouteRow, error) {
	if limit < 1 || limit > RouteBatchLimit {
		return nil, fmt.Errorf("%w: scan by %s limit %d not in [1, %d]", ErrRouteInvalidArgument, column, limit, RouteBatchLimit)
	}
	if after.Shard >= placement.RouteRowShards {
		// 片 >= 64 的游标会让查询静默回空页,调用方会当作翻完(同 ScanRoutes)。
		return nil, fmt.Errorf("%w: scan by %s cursor (shard, player_id)=(%d, %d): shard not in [0, %d)",
			ErrRouteInvalidArgument, column, after.Shard, after.PlayerID, placement.RouteRowShards)
	}
	rows, err := s.db.QueryContext(ctx, query, key, uint64(after.Shard), uint64(after.Shard), after.PlayerID, limit)
	if err != nil {
		return nil, fmt.Errorf("scan player_route by %s=%d after (shard, player_id)=(%d, %d): %w",
			column, key, after.Shard, after.PlayerID, err)
	}
	defer rows.Close()

	out := make([]RouteRow, 0, limit)
	prev := after
	for rows.Next() {
		var row RouteRow
		if err := scanRouteRow(rows, &row, true); err != nil {
			return nil, fmt.Errorf("scan player_route row by %s=%d: %w", column, key, err)
		}
		if !matches(row) {
			return nil, fmt.Errorf("scan player_route by %s=%d returned player_id=%d with home_zone=%d storage_id=%d (query contract broken)",
				column, key, row.PlayerID, row.HomeZone, row.StorageID)
		}
		cur := RouteScanCursorAt(row)
		if !prev.Less(cur) {
			return nil, fmt.Errorf("scan player_route by %s=%d: row (shard, player_id)=(%d, %d) not after (%d, %d) (keyset order broken)",
				column, key, cur.Shard, cur.PlayerID, prev.Shard, prev.PlayerID)
		}
		prev = cur
		out = append(out, row)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate player_route scan by %s=%d: %w", column, key, err)
	}
	if len(out) > limit {
		return nil, fmt.Errorf("scan player_route by %s=%d returned %d rows for limit %d", column, key, len(out), limit)
	}
	return out, nil
}

// ── RD4:生效的合服围栏 ───────────────────────────────────────────────────────────

// ActiveMergeFence 是 zone_merge_fence 63 号片上一条此刻生效的围栏。可空列「NULL = 零值」。
type ActiveMergeFence struct {
	ZoneID  uint32
	RunID   string
	Purpose string // merge / unmerge;NULL = ""
	SrcZone uint32 // NULL = 0
	DstZone uint32 // NULL = 0
	// ExpiresAtMs 是库时钟的过期时刻。0 = 库里 expires_at_ms 为 NULL 而 run_id 非空(只有 CHECK 不生效的库上可能):
	// 本函数按生效列出(fail-closed);调用方不得据此删除镜像,也拼不出镜像的过期时刻(placement.MergeFenceMirrorArgs 拒绝 0)。
	ExpiresAtMs uint64
	HolderJSON  string // 持有者 JSON(第 2 批起带 fence_epoch);NULL = ""
	FenceEpoch  uint64
}

// ActiveMergeFences 是 ListActiveMergeFences 的结果。
type ActiveMergeFences struct {
	// DBNowMs 是同一条语句里判生效用的库时钟(epoch 毫秒)。
	DBNowMs uint64
	// Fences 按 zone_id 升序;没有生效围栏时为空。
	Fences []ActiveMergeFence
}

// ListActiveMergeFences 列出 63 号片上此刻生效的围栏与读时的库时钟(RD4,事务外一致读)。给第 1b-2 批对账 leader 的
// 围栏镜像对账用(规格 §13.1 第 3 条):库里生效的补 / 续镜像,Redis 里有、这里没有的复读确认后按完整值删。
//
// 回复形状不对(没有任何一行、全 NULL 行与真实行同时出现、生效行缺 run_id / fence_epoch、库时钟为 0)一律报错:
// 这份清单决定 leader 删不删镜像,形状坏了宁可这一拍不对账。
func (s *RouteStore) ListActiveMergeFences(ctx context.Context) (ActiveMergeFences, error) {
	rows, err := s.db.QueryContext(ctx, routeListActiveFencesSQL, uint64(routeFenceReadShard))
	if err != nil {
		return ActiveMergeFences{}, fmt.Errorf("list active zone_merge_fence (shard %d): %w", routeFenceReadShard, err)
	}
	defer rows.Close()

	var out ActiveMergeFences
	var total, empty int
	for rows.Next() {
		total++
		var (
			nowMs          uint64
			zone, src, dst sql.Null[uint32]
			runID, purpose sql.NullString
			holder         sql.NullString
			expires, epoch sql.Null[uint64]
		)
		if err := rows.Scan(&nowMs, &zone, &runID, &purpose, &src, &dst, &expires, &holder, &epoch); err != nil {
			return ActiveMergeFences{}, fmt.Errorf("scan active zone_merge_fence row: %w", err)
		}
		if total == 1 {
			out.DBNowMs = nowMs
		}
		if !zone.Valid {
			// 派生表那一行没有匹配到任何围栏(LEFT JOIN 的 f.* 全 NULL)。
			empty++
			continue
		}
		if !runID.Valid || runID.String == "" || !epoch.Valid {
			return ActiveMergeFences{}, fmt.Errorf("active zone_merge_fence row zone_id=%d shard=%d has run_id=%q (valid=%t) fence_epoch valid=%t (reply shape broken)",
				zone.V, routeFenceReadShard, runID.String, runID.Valid, epoch.Valid)
		}
		f := ActiveMergeFence{ZoneID: zone.V, RunID: runID.String, FenceEpoch: epoch.V}
		if purpose.Valid {
			f.Purpose = purpose.String
		}
		if src.Valid {
			f.SrcZone = src.V
		}
		if dst.Valid {
			f.DstZone = dst.V
		}
		if expires.Valid {
			f.ExpiresAtMs = expires.V
		}
		if holder.Valid {
			f.HolderJSON = holder.String
		}
		out.Fences = append(out.Fences, f)
	}
	if err := rows.Err(); err != nil {
		return ActiveMergeFences{}, fmt.Errorf("iterate active zone_merge_fence: %w", err)
	}
	switch {
	case total == 0:
		return ActiveMergeFences{}, errors.New("list active zone_merge_fence: the statement returned no row (the clock row is missing; reply shape broken)")
	case empty > 0 && total > 1:
		return ActiveMergeFences{}, fmt.Errorf("list active zone_merge_fence: %d all-NULL rows among %d rows (reply shape broken)", empty, total)
	case out.DBNowMs == 0:
		return ActiveMergeFences{}, errors.New("list active zone_merge_fence: database clock read as 0")
	}
	return out, nil
}
