package main

// 存量玩家 player:zone 映射回填(-backfill-home-zone -zone N)。
//
// 背景(2026-09-08):生产在此之前从未在建号时写 player:zone:{id}(login 的
// CreatePlayer 注册映射另行修复)。data_service Router / login 的跨区重定向 /
// 合服 remap 全都以这个映射为准,存量玩家没有映射就意味着:
//   - login 查 GetPlayerHomeZone 得到 0,只能回退到账号 blob 里的建角 zone;
//   - 合服 RemapHomeZoneForMerge 按「值==source」改写,没有键的玩家**根本
//     不会被合过去**,合服后仍留在已下线的源区 → 登不上。
// 所以第一次合服之前,每个存量 zone 都必须跑一遍回填。
//
// 真源是 zone 自己的 MySQL:`zone_N_db.player_database`(库名规则与
// go/db/internal/config.ZoneDBName 一致,表由 proto2mysql 从
// mysql_database_table.proto 生成,主键 player_id)。有行 = 这个玩家的归属
// zone 就是 N,这是比账号 blob 更硬的证据。
//
// 写入方式与 remapPlayerMapping 同一把键(playerZoneKeyPrefix + id,值为十进制
// zone 字符串),但用 SET NX:**绝不覆盖**已有映射 —— 已存在的值可能是合服
// remap 后的新归属,覆盖回旧 zone 就是把玩家送回坟场。
//
// 幂等 / dry-run / 分批:keyset 分页读 MySQL(player_id > last ORDER BY
// player_id LIMIT batch),每批一条 pipeline;dry-run 用 EXISTS 统计将写入数。
//
// 已被合走的 zone 拒绝回填(2026-09-28,player-storage-placement.md §12 A10):合服改映射之前会写
// merge:merged_into:{src}(merged_into.go)。那之后「有行 = 归属这里」不再成立(copy 模式源库的行是冷副本,
// pin 模式行还是真源、但归属已是目标区);
// SET NX 虽然不覆盖已有映射,但映射一旦丢失(Redis 整体丢数据正是想起回填的典型时刻),回填就会把
// 已合走的玩家整批钉回死区。dry-run 同样拒绝,彩排就能暴露。
//
// -route-source=db(W9,规格 §6.3):真源是全局库的 player_route。定位降级为「只给从未登记过的存量号补 home」,不再是 Redis
// 丢失后的恢复手段(那由 data_service 的对账 leader 按库全量预热)。每批一个库事务(BackfillHomeBatch:该 zone 的 63 号片围栏
// **未生效**(新增,原实现不看围栏)→ 库里的 zone_merged_into 有该 zone 作 src 就拒绝(Redis 丢了也拦得住)→ 无行插入 home /
// home 为 NULL 补 home / 已是本 zone 计 already / 别的 zone 不动)→ 提交 → 本批目标状态的全部行发布 + WAITAOF。读 zone 库也改走
// RC 会话(readCommittedDSN);不再有逐键 GET,也就没有「单键读错误被当成缺席」的口子。

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"log"
	"strconv"
	"time"

	"github.com/redis/go-redis/v9"
)

const backfillBatchSize = 1000

// backfillEntryParams 是 -backfill-home-zone 的纯数据输入(main 管 flag,
// 本文件管语义;与 auditEntryParams 同一分层)。
type backfillEntryParams struct {
	zone        uint32
	mysqlDSN    string
	mappingAddr string
	mappingPwd  string
	mappingDB   int
	dryRun      bool
	// route 是 -route-source 与路由库参数(main 装配,routeEntryOptionsOf)。
	route routeEntryOptions
}

// runBackfillEntry 是 -backfill-home-zone 的入口。
//
// **两个 zone 都要在第一次合服之前各跑一遍**:回填只写自己 zone 库里有行的
// 玩家,目标区玩家没有映射的话,合服后 data_service 一样路由不到他们。
func runBackfillEntry(p backfillEntryParams) {
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Hour)
	defer cancel()

	log.Printf("=== Backfill player:zone for zone %d from %s.player_database (dry-run=%v, mapping=%s db=%d, route-source=%s) ===",
		p.zone, zoneDBName(p.zone), p.dryRun, p.mappingAddr, p.mappingDB, p.route.source)

	var db *sql.DB
	if p.route.source == routeSourceDB {
		// W9:读 zone 库同样走 RC 会话(与合服 / 撤销同一口径,mustOpenMySQL → readCommittedDSN)。
		db = mustOpenMySQL(ctx, p.mysqlDSN)
	} else {
		var err error
		db, err = sql.Open("mysql", p.mysqlDSN)
		if err != nil {
			log.Fatalf("mysql open: %v", err)
		}
		if err := db.PingContext(ctx); err != nil {
			log.Fatalf("mysql ping: %v", err)
		}
	}
	defer db.Close()
	if err := assertSchemaExists(ctx, db, zoneDBName(p.zone)); err != nil {
		log.Fatalf("backfill: %v", err)
	}

	mapRdb := redis.NewClient(&redis.Options{Addr: p.mappingAddr, Password: p.mappingPwd, DB: p.mappingDB})
	defer mapRdb.Close()
	if err := mapRdb.Ping(ctx).Err(); err != nil {
		log.Fatalf("mapping redis ping: %v", err)
	}
	rb, err := openRunRouteBackend(ctx, p.route, mapRdb, newToolRunID("backfill", p.zone, time.Now()))
	if err != nil {
		log.Fatalf("backfill: route source: %v", err)
	}
	defer rb.Close()

	rep, err := stepBackfillHomeZone(ctx, rb, mapRdb, newMySQLPlayerIDSource(db, p.zone), p.zone, p.dryRun)
	if err != nil {
		log.Fatalf("backfill: %v (partial progress: %s)", err, rep)
	}
	log.Printf("=== Backfill done (%s): %s ===", verbWrite(p.dryRun), rep)
	if rep.Mismatched > 0 {
		log.Printf("NOTE: %d players already map to a DIFFERENT zone and were left untouched. "+
			"That is expected for players who have already been merged out of this zone.", rep.Mismatched)
	}
}

// zoneDBName 镜像 go/db/internal/config.ZoneDBName;merge_zone 是独立 module,
// 不引主工程包。改一处必须同步另一处。
func zoneDBName(zone uint32) string {
	return fmt.Sprintf("zone_%d_db", zone)
}

// backfillReport 是回填计数汇总。
type backfillReport struct {
	RowsScanned   int // MySQL 读到的 player 行数
	Created       int // 新写入的映射数(dry-run 恒 0)
	WouldCreate   int // dry-run 下将写入的数;apply 下等于 Created
	AlreadyMapped int // 键已存在且值==zone(幂等跳过)
	Mismatched    int // 键已存在但值!=zone(保留不动,打 WARN)
}

func (r backfillReport) String() string {
	return fmt.Sprintf("rows_scanned=%d would_create=%d created=%d already_mapped=%d mismatched=%d",
		r.RowsScanned, r.WouldCreate, r.Created, r.AlreadyMapped, r.Mismatched)
}

// playerIDBatch 是纯分页函数的输入面:测试用切片替身,生产用 MySQL。
type playerIDSource interface {
	// NextBatch 返回 player_id > after 的下一批(升序),空切片表示结束。
	NextBatch(ctx context.Context, after uint64, limit int) ([]uint64, error)
}

// mysqlPlayerIDSource 从 zone_N_db.player_database 按主键 keyset 分页。
type mysqlPlayerIDSource struct {
	db    *sql.DB
	table string // 已全限定:zone_N_db.player_database
}

func newMySQLPlayerIDSource(db *sql.DB, zone uint32) *mysqlPlayerIDSource {
	return &mysqlPlayerIDSource{db: db, table: zoneDBName(zone) + ".player_database"}
}

func (s *mysqlPlayerIDSource) NextBatch(ctx context.Context, after uint64, limit int) ([]uint64, error) {
	rows, err := s.db.QueryContext(ctx,
		"SELECT player_id FROM "+s.table+" WHERE player_id > ? ORDER BY player_id LIMIT ?", after, limit)
	if err != nil {
		return nil, fmt.Errorf("query %s: %w", s.table, err)
	}
	defer rows.Close()
	out := make([]uint64, 0, limit)
	for rows.Next() {
		var id uint64
		if err := rows.Scan(&id); err != nil {
			return nil, err
		}
		out = append(out, id)
	}
	return out, rows.Err()
}

// sliceSource 是测试用的内存分页源。
type sliceSource []uint64 // 必须升序

func (s sliceSource) NextBatch(_ context.Context, after uint64, limit int) ([]uint64, error) {
	var out []uint64
	for _, id := range s {
		if id > after {
			out = append(out, id)
			if len(out) == limit {
				break
			}
		}
	}
	return out, nil
}

// backfillPlanBatch 是纯规划:给定一批 id 与它们当前的映射值(""=不存在),
// 决定哪些要写、哪些跳过、哪些是冲突。抽出来是为了在没有 miniredis 的
// 独立 module 里也能单测规则本身。
func backfillPlanBatch(ids []uint64, existing []string, zoneVal string, rep *backfillReport) (toCreate []uint64) {
	for i, id := range ids {
		switch {
		case existing[i] == "":
			toCreate = append(toCreate, id)
		case existing[i] == zoneVal:
			rep.AlreadyMapped++
		default:
			rep.Mismatched++
			log.Printf("WARN: player %d already mapped to zone %s (backfill zone %s) — kept as-is", id, existing[i], zoneVal)
		}
	}
	rep.WouldCreate += len(toCreate)
	return toCreate
}

// backfillHomeZone 逐批读 id → 读现有映射 → SET NX 写缺席的。zone 已被合走(A10 标记在)时一个键都不写。
func backfillHomeZone(ctx context.Context, mapRdb *redis.Client, src playerIDSource, zone uint32, dryRun bool) (backfillReport, error) {
	var rep backfillReport
	if err := refuseBackfillOfMergedZone(ctx, mapRdb, zone); err != nil {
		return rep, err
	}
	zoneVal := strconv.FormatUint(uint64(zone), 10)
	var after uint64
	for {
		ids, err := src.NextBatch(ctx, after, backfillBatchSize)
		if err != nil {
			return rep, err
		}
		if len(ids) == 0 {
			break
		}
		rep.RowsScanned += len(ids)
		after = ids[len(ids)-1]

		// 读现有映射(MGET 在 cluster 下会跨 slot;mapping Redis 是单实例,
		// 但为了与 remapPlayerMapping 的逐键风格一致、并让 dry-run 与 apply
		// 走同一条读路径,这里用 pipeline GET)。
		pipe := mapRdb.Pipeline()
		gets := make([]*redis.StringCmd, len(ids))
		for i, id := range ids {
			gets[i] = pipe.Get(ctx, playerZoneKeyPrefix+strconv.FormatUint(id, 10))
		}
		if _, err := pipe.Exec(ctx); err != nil && err != redis.Nil {
			return rep, fmt.Errorf("pipeline get mappings: %w", err)
		}
		existing := make([]string, len(ids))
		for i := range ids {
			v, err := gets[i].Result()
			if err == nil {
				existing[i] = v
			}
		}
		toCreate := backfillPlanBatch(ids, existing, zoneVal, &rep)
		if dryRun || len(toCreate) == 0 {
			continue
		}

		wp := mapRdb.Pipeline()
		sets := make([]*redis.BoolCmd, len(toCreate))
		for i, id := range toCreate {
			// NX:读与写之间若 login 刚好注册了这个玩家,以 login 写的为准。
			sets[i] = wp.SetNX(ctx, playerZoneKeyPrefix+strconv.FormatUint(id, 10), zoneVal, 0)
		}
		if _, err := wp.Exec(ctx); err != nil {
			return rep, fmt.Errorf("pipeline setnx mappings: %w", err)
		}
		for _, c := range sets {
			if c.Val() {
				rep.Created++
			} else {
				rep.AlreadyMapped++
				rep.WouldCreate--
			}
		}
	}
	return rep, nil
}

// ── -route-source=db(W9,见文件头)──────────────────────────────────────────────

// stepBackfillHomeZone 按源分派回填:rb == nil → backfillHomeZone(Redis SET NX,逐字不变);否则 backfillHomeZoneDB。
func stepBackfillHomeZone(ctx context.Context, rb *routeBackend, mapRdb *redis.Client, src playerIDSource, zone uint32, dryRun bool) (backfillReport, error) {
	if rb == nil {
		return backfillHomeZone(ctx, mapRdb, src, zone, dryRun)
	}
	return backfillHomeZoneDB(ctx, rb, src, zone, dryRun)
}

// refuseBackfillDB 是库版回填的只读前置(dry-run 与 apply 同一口径,先于任何写):该 zone 的围栏生效、或库里记着它已被合走,
// 即拒绝;读失败同样拒绝(fail-closed)。apply 的每个批事务在库里再复核一次,那才是权威判据。
func refuseBackfillDB(ctx context.Context, db routeDBOps, zone uint32) error {
	fs, err := db.ReadFence(ctx, zone)
	if err != nil {
		return fmt.Errorf("cannot prove zone %d is not being merged (zone_merge_fence): %w — refusing to backfill", zone, err)
	}
	if fs.Active {
		return fmt.Errorf("zone %d is being merged (zone_merge_fence run_id=%s holder=%s): backfill after that run finishes",
			zone, fs.RunID, fs.HolderJSON)
	}
	dst, run, found, err := db.ReadMergedInto(ctx, zone)
	if err != nil {
		return fmt.Errorf("cannot prove zone %d was not merged away (zone_merged_into): %w — refusing to backfill", zone, err)
	}
	if found {
		return fmt.Errorf("%w: zone %d was merged into zone %d by run %s (zone_merged_into): its players were re-homed by that merge, "+
			"and backfilling would pin them back to a dead zone. Restore lost mappings by replaying the merge manifest (runbook), "+
			"not by backfill", errRouteZoneMergedAway, zone, dst, run)
	}
	return nil
}

// backfillHomeZoneDB 是 backfillHomeZone 的库版(W9):逐批读 zone 库的 player_id(keyset 分页,口径与 Redis 版相同)→ 每
// routeWriteBatchLimit 人一个库事务(BackfillHomeBatch)→ 本批目标状态的全部行(插入的、补上的、早已是本 zone 的)发布 +
// WAITAOF → 下一批。计数与 Redis 版同口径:插入与补 home 计 Created,已是本 zone 计 AlreadyMapped,别的 zone 计 Mismatched(不动)。
// 任何一批发布失败即停(重跑时已写的走「已是本 zone」分支、重新发布)。dry-run 只读库分类。
func backfillHomeZoneDB(ctx context.Context, rb *routeBackend, src playerIDSource, zone uint32, dryRun bool) (backfillReport, error) {
	var rep backfillReport
	if err := refuseBackfillDB(ctx, rb.db, zone); err != nil {
		return rep, err
	}
	warnConflict := func(id uint64) {
		log.Printf("WARN: player %d already has a home_zone other than %d in player_route — kept as-is", id, zone)
	}
	var after uint64
	for {
		ids, err := src.NextBatch(ctx, after, backfillBatchSize)
		if err != nil {
			return rep, err
		}
		if len(ids) == 0 {
			return rep, nil
		}
		rep.RowsScanned += len(ids)
		after = ids[len(ids)-1]
		if dryRun {
			rows, err := rb.db.ReadRoutes(ctx, ids)
			if err != nil {
				return rep, err
			}
			for _, id := range ids {
				switch row, ok := rows[id]; {
				case !ok || row.HomeZone == 0:
					rep.WouldCreate++
				case row.HomeZone == zone:
					rep.AlreadyMapped++
				default:
					rep.Mismatched++
					warnConflict(id)
				}
			}
			continue
		}
		for _, chunk := range chunkUint64(ids, routeWriteBatchLimit) {
			res, err := rb.db.BackfillHomeBatch(ctx, zone, chunk)
			if err != nil {
				if errors.Is(err, errRouteZoneFenced) || errors.Is(err, errRouteZoneMergedAway) {
					return rep, fmt.Errorf("zone %d became fenced or merged away while backfilling — stopped: %w", zone, err)
				}
				return rep, fmt.Errorf("backfill a batch of %d players (nothing of this batch committed): %w", len(chunk), err)
			}
			for _, id := range chunk {
				switch v := res.Verdicts[id]; v {
				case routeBackfillInserted, routeBackfillFilled:
					rep.Created++
					rep.WouldCreate++
				case routeBackfillAlready:
					rep.AlreadyMapped++
				case routeBackfillConflict:
					rep.Mismatched++
					warnConflict(id)
				default:
					return rep, fmt.Errorf("%w: the backfill batch returned verdict %d for player %d", errRouteInvariant, v, id)
				}
			}
			if _, err := rb.pub.publishRoutes(ctx, res.Target); err != nil {
				return rep, fmt.Errorf("a backfill batch of %d players committed in player_route (%d written) but publishing its %d "+
					"target rows failed — re-run (rows already on zone %d are republished): %w",
					len(chunk), res.Changed, len(res.Target), zone, err)
			}
		}
	}
}
