package main

// -mode storage-audit -storage N(docs/design/player-storage-placement.md §11.1 第 5 步):只读地报告
//
//	有效落点为 N 的玩家数  = 落点记录指向 N 的(含冻结中的) + 没有记录、home == N 的(N 是 zone 库时)
//	N 库里的冷副本数       = N 库 player_database 里有行、但有效落点不是 N 的玩家(P-1:在线路径不读不写它们)
//
// 退役一个落点库(下线 zone_N_db / 旧全局库)的判据是前者为 0。另外两类行也必须先处理,本模式单独列出:
//   - 无主行:N 库有行,既没有落点记录也没有 player:zone —— 有效落点定不下来(go/db 按处理任务的 zone 回落),
//     多半是从没回填过映射的存量号;库一删,他们就没了;
//   - 畸形:落点记录或 home 畸形 —— go/db 对他们 fail-closed,工具也判断不了他们指向哪。
//
// 口径与 go/db 相同(有记录看记录,无记录看 home);行只看 player_database(每个玩家必有这一行,
// 与 -backfill-home-zone、合服守卫同一个锚)。扫描不加锁,结果是「扫描那一刻」的数;在线跑时数字会动。
//
// 映射从哪里读(D1 第 2 批,规格 §6.3 R8、§13.3 第 6 条):-route-source=db 时一律读全局库 player_route(真源;
// 「能不能退役」绝不能按一份可能落后 / 丢键的缓存下结论),redis 时读 mapping Redis(今天的口径原样保留)。
// 两种来源经同一个接缝 routeReadSide 交给同一套判定,值串逐字节相同。本文件同时放这个接缝(audit 与搬库 R0 共用)。
//
// 退出码:0 = 审计跑完(无论能不能退役,看最后一行结论);2 = 没查成(连不上 / 库不在 / 库与 Redis 不成对),结论不可信。

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"log"
	"os"
	"time"

	"github.com/redis/go-redis/v9"
)

// storageAuditEntryParams 是 -mode storage-audit 的纯数据输入。
type storageAuditEntryParams struct {
	storage     uint32
	mysqlDSN    string
	mappingAddr string
	mappingPwd  string
	mappingDB   int
	timeout     time.Duration
	// route 是 -route-source 与路由库参数(main 装配,routeEntryOptionsOf;DSN 含口令,不进日志)。
	// source 为空 = 装配层没传:按 fail-closed 报「没查成」(exit 2),绝不默认成某一种来源。
	route routeEntryOptions
}

// ── R8 读方:按 -route-source 读映射(规格 §6.3 R8、§13.3 第 3、6 条)────────────────────────────

// routeReadSide 是只读读方(storage-audit、audit、搬库 R0 的枚举)读映射的接缝:
//   - routeReadSideRedis:读 mapping Redis(-route-source=redis,今天的口径原样保留);
//   - routeReadSideDB:读全局库 player_route(-route-source=db;一致读、不加锁、不发布、不写任何东西)。
//
// 两个实现给出的值串逐字节相同(routePlacementRead 与 Redis 的编码同一口径),调用方的判定函数不必区分来源。
// 搬库 R1 / R5 的读写不走这里,走 relocatePlacements(relocate_run.go 的 sqlRelocatePlacements:读之前要先发布确认)。
type routeReadSide interface {
	// placements 按 ids 顺序返回落点记录与 home(长度与 ids 相同)。单个值畸形记在对应的 *Err 上,不报错;
	// 读不成(连不上、库返回了不该有的行)报错。
	placements(ctx context.Context, ids []uint64) ([]placementRead, error)
	// playersWithHome 返回 home_zone == zone 的全部玩家(升序去重)。
	playersWithHome(ctx context.Context, zone uint32) ([]uint64, error)
	// storagePlayers 返回有效落点为 storage 的玩家(口径见 storagePlayers)。
	storagePlayers(ctx context.Context, storage uint32) (storagePlayers, error)
	// source 是 routeSourceDB / routeSourceRedis(进日志与审计 Notes,低基数)。
	source() string
}

// routeReadSideRedis 读 mapping Redis。
type routeReadSideRedis struct{ rdb *redis.Client }

func (r routeReadSideRedis) placements(ctx context.Context, ids []uint64) ([]placementRead, error) {
	return readPlacements(ctx, r.rdb, ids)
}

func (r routeReadSideRedis) playersWithHome(ctx context.Context, zone uint32) ([]uint64, error) {
	return collectPlayerIDsWithHomeZone(ctx, r.rdb, zone)
}

func (r routeReadSideRedis) storagePlayers(ctx context.Context, storage uint32) (storagePlayers, error) {
	return collectStoragePlayers(ctx, r.rdb, storage)
}

func (routeReadSideRedis) source() string { return routeSourceRedis }

// routeDBReader 是 -route-source=db 时读方用到的库读原语(生产 *mergeRouteStore,route_store.go;单测注入假实现)。
// 全部是事务外一致读、不加锁。
type routeDBReader interface {
	ReadRoutes(ctx context.Context, ids []uint64) (map[uint64]mergeRouteRow, error)
	ScanByHome(ctx context.Context, zone uint32, after routeScanCursor, limit int) ([]mergeRouteRow, error)
	ScanByStorage(ctx context.Context, storage uint32, after routeScanCursor, limit int) ([]mergeRouteRow, error)
	ReadFence(ctx context.Context, zone uint32) (routeFenceState, error)
}

// routeReadSideDB 读全局库 player_route(真源)。
type routeReadSideDB struct{ db routeDBReader }

// placements:库里没有这一行 = 两键都不存在(权威缺席);落点三列不自洽 = RecordErr(调用方 fail-closed)。
func (r routeReadSideDB) placements(ctx context.Context, ids []uint64) ([]placementRead, error) {
	out := make([]placementRead, len(ids))
	if len(ids) == 0 {
		return out, nil
	}
	rows, err := r.db.ReadRoutes(ctx, ids)
	if err != nil {
		return nil, err
	}
	for i, id := range ids {
		row, ok := rows[id]
		out[i] = routePlacementRead(row, ok)
	}
	return out, nil
}

func (r routeReadSideDB) playersWithHome(ctx context.Context, zone uint32) ([]uint64, error) {
	var out []uint64
	err := scanRouteDBPages(
		func(after routeScanCursor) ([]mergeRouteRow, error) {
			return r.db.ScanByHome(ctx, zone, after, routeReadBatchLimit)
		},
		func(row mergeRouteRow) error {
			out = append(out, row.PlayerID)
			return nil
		})
	if err != nil {
		return nil, err
	}
	return sortedUint64(out), nil
}

// storagePlayers 是 collectStoragePlayers 的库版,口径相同(有记录看记录,无记录看 home),只有 Malformed 的范围不同:
// Redis 里畸形的记录解不出指向哪个库,只能全 mapping 计;库里 storage_id 是一列,畸形的只可能是「storage_id = storage、
// 但版本 / 冻结 run_id 不合法」或「home = storage、没有 storage_id、却带着版本 / 冻结 run_id」的行(CHECK 生效时后者不存在),
// 这里只计这两类 —— 指向别处的行与本库无关。畸形的行不计入 ByRecord / ByHome(与 Redis 版同一口径)。
func (r routeReadSideDB) storagePlayers(ctx context.Context, storage uint32) (storagePlayers, error) {
	var out storagePlayers
	err := scanRouteDBPages(
		func(after routeScanCursor) ([]mergeRouteRow, error) {
			return r.db.ScanByStorage(ctx, storage, after, routeReadBatchLimit)
		},
		func(row mergeRouteRow) error {
			rec, ok, perr := row.placement()
			switch {
			case perr != nil:
				out.Malformed = append(out.Malformed, row.PlayerID)
			case ok && rec.StorageID == storage:
				out.ByRecord = append(out.ByRecord, row.PlayerID)
				if rec.Frozen {
					out.Frozen++
				}
			default:
				return fmt.Errorf("%w: scan by storage_id=%d returned player %d without that placement", errRouteInvariant, storage, row.PlayerID)
			}
			return nil
		})
	if err != nil {
		return out, err
	}
	if isZoneStorage(storage) {
		err = scanRouteDBPages(
			func(after routeScanCursor) ([]mergeRouteRow, error) {
				return r.db.ScanByHome(ctx, storage, after, routeReadBatchLimit)
			},
			func(row mergeRouteRow) error {
				if row.StorageID != 0 {
					return nil // 有落点:按 storage_id 计(上一段),不看 home
				}
				if _, _, perr := row.placement(); perr != nil {
					out.Malformed = append(out.Malformed, row.PlayerID)
					return nil
				}
				out.ByHome = append(out.ByHome, row.PlayerID)
				return nil
			})
		if err != nil {
			return out, err
		}
	}
	out.ByRecord = sortedUint64(out.ByRecord)
	out.ByHome = sortedUint64(out.ByHome)
	out.Malformed = sortedUint64(out.Malformed)
	return out, nil
}

func (routeReadSideDB) source() string { return routeSourceDB }

// scanRouteDBPages 按键集游标翻完一个索引范围:每页 routeReadBatchLimit 行,不足一页即翻完,下一页从上一页最后一行的
// 聚簇键接着翻(页内的校验由 ScanByHome / ScanByStorage 做);逐行交给 visit,visit 报错即停。
func scanRouteDBPages(page func(after routeScanCursor) ([]mergeRouteRow, error), visit func(mergeRouteRow) error) error {
	var after routeScanCursor
	for {
		rows, err := page(after)
		if err != nil {
			return err
		}
		for _, row := range rows {
			if err := visit(row); err != nil {
				return err
			}
		}
		if len(rows) < routeReadBatchLimit {
			return nil
		}
		after = routeCursorOf(rows[len(rows)-1].PlayerID)
	}
}

// openRouteReadSide 按 -route-source 装出只读读方,并先核对全局库与 mapping Redis 是否成对(openRouteBackend:redis 时
// 见到 mapping:route:watermark 即拒;db 时要求它存在、世代等于库的 route_meta.generation,规格 §13.3 第 3 条)。
// db 时返回的 store 由调用方 Close。读方从不写:开库时的探针 P4 / P5 在必然回滚的事务里插行(与 data_service 启动时
// 同一探针),不留任何数据;发布器只用来读水位键,WAITAOF 参数用不到。purpose 只进写者标识(只读用不到,但开库要求合法)。
// 不复用 openRunRouteBackend(merge_run.go):那是写入方的入口(日志按「先写库再发布」说),且它的库接口不含 ScanByStorage。
func openRouteReadSide(ctx context.Context, ro routeEntryOptions, mapping *redis.Client, purpose string) (routeReadSide, *mergeRouteStore, error) {
	if ro.source == "" {
		return nil, nil, errors.New("internal: -route-source was not passed to this mode (the main.go wiring is missing); " +
			"refusing to guess which copy of the route mapping is authoritative")
	}
	if mapping == nil {
		return nil, nil, errors.New("no mapping Redis handle: cannot check whether the route DB and the mapping Redis are paired " +
			"(" + routeWatermarkKey + ")")
	}
	host, _ := os.Hostname()
	st, err := openRouteBackend(ctx, ro.source, ro.dbDSN, newMergeRouteWriter(host, purpose), newRoutePublisher(mapping, routeDurability{}))
	if err != nil {
		return nil, nil, err
	}
	if st == nil {
		return routeReadSideRedis{rdb: mapping}, nil, nil
	}
	return routeReadSideDB{db: st}, st, nil
}

// storageAuditReport 是审计结果。
type storageAuditReport struct {
	Storage        uint32
	ByRecord       int // 落点记录指向 N
	FrozenOnRecord int // 其中冻结中(搬库进行中:数据还在 N)
	ByHome         int // 没有记录、home == N
	// MalformedRecords:全 mapping 里畸形的落点记录(指向哪里不知道,可能就是 N)。
	MalformedRecords int
	MalformedSample  []uint64
	// 以下是 N 库 player_database 的逐行判定。
	Rows             int
	LiveRows         int // 有效落点就是 N
	ColdCopies       int // 有效落点不是 N:冷副本
	ColdSample       []uint64
	UnmappedRows     int // 既无记录也无 home
	UnmappedSample   []uint64
	UnreadableRows   int // 行主人的记录或 home 畸形
	UnreadableSample []uint64
}

// Effective 是有效落点为 N 的玩家数。
func (r storageAuditReport) Effective() int { return r.ByRecord + r.ByHome }

// RetireBlockers 返回退役 N 之前必须处理的事(空 = 可以退役)。
func (r storageAuditReport) RetireBlockers() []string {
	var out []string
	if r.Effective() > 0 {
		out = append(out, fmt.Sprintf("%d players are still placed on storage %d", r.Effective(), r.Storage))
	}
	if r.UnmappedRows > 0 {
		out = append(out, fmt.Sprintf("%d rows have neither a placement record nor a home_zone (first: %v)", r.UnmappedRows, r.UnmappedSample))
	}
	if r.UnreadableRows > 0 {
		out = append(out, fmt.Sprintf("%d rows belong to players whose placement / home_zone is malformed (first: %v)", r.UnreadableRows, r.UnreadableSample))
	}
	if r.MalformedRecords > 0 {
		out = append(out, fmt.Sprintf("%d malformed placement records exist (first: %v) — any of them may point at storage %d",
			r.MalformedRecords, r.MalformedSample, r.Storage))
	}
	return out
}

func (r storageAuditReport) String() string {
	return fmt.Sprintf("storage %d: effective=%d (by_record=%d incl. frozen=%d, by_home=%d) | player_database rows=%d "+
		"(live=%d cold_copies=%d unmapped=%d unreadable=%d) | malformed_records=%d",
		r.Storage, r.Effective(), r.ByRecord, r.FrozenOnRecord, r.ByHome, r.Rows, r.LiveRows, r.ColdCopies,
		r.UnmappedRows, r.UnreadableRows, r.MalformedRecords)
}

// storageRowVerdict 是 N 库里一行的归属判定。
type storageRowVerdict int

const (
	storageRowLive       storageRowVerdict = iota // 有效落点 == N
	storageRowCold                                // 有效落点是别的库:冷副本
	storageRowUnmapped                            // 既无记录也无 home
	storageRowUnreadable                          // 记录或 home 畸形
)

// classifyStorageRow 是纯判定:N 库里有这名玩家的行时,它是不是他的真源。口径同 go/db:记录或 home 任一畸形,
// go/db 对这名玩家的选库整体 fail-closed(parsePlacementSnapshot 报错),他的行在哪都没人读得到 → unreadable;
// 其余按 effectiveStorage(有记录看记录,无记录看 home),回落值 0 表示两者都没有 → 无主行。
func classifyStorageRow(rd placementRead, storage uint32) storageRowVerdict {
	if rd.RecordErr != nil || rd.HomeErr != nil {
		return storageRowUnreadable
	}
	switch effectiveStorage(rd.Record, rd.RecordPresent, rd.Home, rd.HomePresent, 0) {
	case 0:
		return storageRowUnmapped
	case storage:
		return storageRowLive
	default:
		return storageRowCold
	}
}

func appendSample(sample []uint64, id uint64) []uint64 {
	if len(sample) < unmappedSampleSize {
		return append(sample, id)
	}
	return sample
}

// auditStorage 是 -route-source=redis 口径的本体(读 mapping Redis),保留给既有调用方。
func auditStorage(ctx context.Context, db *sql.DB, rdb *redis.Client, storage uint32) (storageAuditReport, error) {
	return auditStorageWith(ctx, db, routeReadSideRedis{rdb: rdb}, storage)
}

// auditStorageWith 是本模式的本体:映射经 routes 读(db 源读库、redis 源读 mapping Redis),N 库的行经 db 读。
func auditStorageWith(ctx context.Context, db *sql.DB, routes routeReadSide, storage uint32) (storageAuditReport, error) {
	rep := storageAuditReport{Storage: storage}
	name, ok := storeDBName(storage)
	if !ok {
		return rep, fmt.Errorf("-storage %d is not a storage id", storage)
	}
	if routes == nil {
		return rep, errors.New("no route read side")
	}
	if err := assertSchemaExists(ctx, db, name); err != nil {
		return rep, err
	}
	sp, err := routes.storagePlayers(ctx, storage)
	if err != nil {
		return rep, err
	}
	rep.ByRecord, rep.FrozenOnRecord, rep.ByHome = len(sp.ByRecord), sp.Frozen, len(sp.ByHome)
	rep.MalformedRecords = len(sp.Malformed)
	rep.MalformedSample = sampleUint64(sp.Malformed)

	rows := &mysqlPlayerIDSource{db: db, table: name + ".player_database"}
	var after uint64
	for {
		ids, err := rows.NextBatch(ctx, after, backfillBatchSize)
		if err != nil {
			return rep, err
		}
		if len(ids) == 0 {
			return rep, nil
		}
		after = ids[len(ids)-1]
		reads, err := routes.placements(ctx, ids)
		if err != nil {
			return rep, err
		}
		if len(reads) != len(ids) {
			return rep, fmt.Errorf("placement read returned %d answers for %d players", len(reads), len(ids))
		}
		for i, id := range ids {
			rep.Rows++
			switch classifyStorageRow(reads[i], storage) {
			case storageRowLive:
				rep.LiveRows++
			case storageRowCold:
				rep.ColdCopies++
				rep.ColdSample = appendSample(rep.ColdSample, id)
			case storageRowUnmapped:
				rep.UnmappedRows++
				rep.UnmappedSample = appendSample(rep.UnmappedSample, id)
			default:
				rep.UnreadableRows++
				rep.UnreadableSample = appendSample(rep.UnreadableSample, id)
			}
		}
	}
}

func runStorageAuditEntry(p storageAuditEntryParams) {
	ctx, cancel := context.WithTimeout(context.Background(), p.timeout)
	defer cancel()
	log.Printf("=== Storage audit for storage %d (route-source=%s mapping=%s db=%d) — read-only ===", p.storage, p.route.source,
		p.mappingAddr, p.mappingDB)
	infra := func(format string, args ...any) {
		log.Printf("ERROR: "+format, args...)
		log.Printf("Storage audit could not complete. The result is NOT a clean bill of health.")
		os.Exit(2)
	}
	db, err := sql.Open("mysql", p.mysqlDSN)
	if err != nil {
		infra("mysql open: %v", err)
	}
	defer db.Close()
	if err := db.PingContext(ctx); err != nil {
		infra("mysql ping: %v", err)
	}
	rdb := redis.NewClient(&redis.Options{Addr: p.mappingAddr, Password: p.mappingPwd, DB: p.mappingDB})
	defer rdb.Close()
	if err := rdb.Ping(ctx).Err(); err != nil {
		infra("mapping redis ping (%s db=%d): %v", p.mappingAddr, p.mappingDB, err)
	}
	// db 源:映射一律读全局库(决定库能否退役,不能按缓存下结论);开库前先核对库与 mapping Redis 成对。
	routes, routeStore, err := openRouteReadSide(ctx, p.route, rdb, "storage-audit")
	if err != nil {
		infra("route source %q: %v", p.route.source, err)
	}
	if routeStore != nil {
		defer routeStore.Close()
	}
	log.Printf("route mapping is read from %s", routes.source())
	rep, err := auditStorageWith(ctx, db, routes, p.storage)
	if err != nil {
		infra("%v (partial: %s)", err, rep)
	}
	log.Printf("%s", rep)
	if rep.ColdCopies > 0 {
		log.Printf("cold copies (rows whose player now lives elsewhere; online paths never read them), first: %v", rep.ColdSample)
	}
	if blockers := rep.RetireBlockers(); len(blockers) > 0 {
		log.Printf("RETIREMENT CHECK: NOT SAFE to retire storage %d:", p.storage)
		for _, b := range blockers {
			log.Printf("  - %s", b)
		}
		return
	}
	log.Printf("RETIREMENT CHECK: nothing routes to storage %d any more; its %d rows are cold copies.", p.storage, rep.Rows)
}
