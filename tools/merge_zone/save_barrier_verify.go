package main

// verify:save_barrier —— -verify-merged 的父快照提交屏障逐人核对(2026-10-20 修复,r2-mz#1)。
//
// login 冷加载(go/login/internal/logic/pkg/dataloader/parent_commit_barrier.go 的 beginParentLoad / validateSQLSub)
// 对 owner_epoch≠0 的玩家要求:player:{id}:save_barrier 的 pending 存在且合法、pending 的代次不超过 owner_epoch、
// 两表 ACK 都等于 pending,并且从有效落点库读到的两张 SQL 行的行内围栏 (fence_owner_epoch, fence_save_seq) 都等于 pending。
// 合服后任何一条不成立,这名玩家开服后都登录不进来(fail-closed,要停写恢复)。
//
// 前向 copy 合服从此保留屏障(player_rows.go 的 playerCacheKeysKeepBarrier);这一行逐人核对它赖以成立的前提,把「开服后
// 才发现整批进不来」提前到 Step 5:旧二进制删过屏障、续跑之间有人动过行、P4 被绕过,都会在这里 block。处置是
// docs/ops/merge-zone-runbook.md §10.1 的 save_barrier_restore 流程(保持维护闸,不 zone-up)。
//
// owner_epoch 为 0 或不存在的人(从没进过场景)不要求屏障,只计数。读失败一律 INFRA(没查成,不是通过)。
// 键与版本格式都是镜像字面量(本模块是独立 go.mod,不引 shared),单测钉住。
//
// 屏障 hash 只按 pending 与两表 ACK 三个字段判,不按字段集合判:C++ 严格屏障写的 schema_gen(覆盖世代)/ schema_writer
// (写方令牌,docs/design/mixed-version-persist-field-protection.md §5.2)与 go/db 的 outbox_* 都是合法字段,前向 copy
// 合服原样保留,这里不读也不拒。login 冷加载同样只读这三个字段(schema_gen 只在父快照快路径上比较)。

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"sort"
	"strconv"
	"strings"

	"github.com/redis/go-redis/v9"
)

// saveBarrierAckTables 是屏障 hash 里两张表的 ACK 字段名,也是 zone 库里两张带行内围栏的玩家表名:
// go/db player_cache_publish.go 以 task.MsgType 作 ACK 字段,login 冷加载逐表 HGET 同名字段、逐表核对 SQL 行围栏。
var saveBarrierAckTables = [2]string{"player_database", "player_database_1"}

const saveBarrierPendingField = "pending"

// saveBarrierReadBatch 是一次 MGET / 一条 HMGET pipeline 里的玩家数。
const saveBarrierReadBatch = 500

// playerOwnerEpochKey 与 go/shared/ownerepoch.OwnerEpochKey 逐字一致(旧格式,不带花括号)。
func playerOwnerEpochKey(pid uint64) string {
	return "player:" + strconv.FormatUint(pid, 10) + ":owner_epoch"
}

// saveBarrierVersion 与 go/shared/savebarrier.Version 逐字一致:两段各 20 位十进制、中间一个冒号,共 41 字节。
func saveBarrierVersion(ownerEpoch, saveSeq uint64) string {
	return fmt.Sprintf("%020d:%020d", ownerEpoch, saveSeq)
}

// parseSaveBarrierVersion 与 savebarrier.Parse 同口径:长度、冒号位置、纯数字、uint64 不溢出,缺一不可。
func parseSaveBarrierVersion(v string) (ownerEpoch, saveSeq uint64, ok bool) {
	if len(v) != 41 || v[20] != ':' {
		return 0, 0, false
	}
	for i := 0; i < len(v); i++ {
		if i != 20 && (v[i] < '0' || v[i] > '9') {
			return 0, 0, false
		}
	}
	e, eerr := strconv.ParseUint(v[:20], 10, 64)
	s, serr := strconv.ParseUint(v[21:], 10, 64)
	if eerr != nil || serr != nil {
		return 0, 0, false
	}
	return e, s, true
}

// saveBarrierObservation 是一名清单玩家被观测到的状态(纯数据,judgeSaveBarrier 只看它)。
type saveBarrierObservation struct {
	OwnerRaw     string // player:<id>:owner_epoch 原值
	OwnerPresent bool
	Pending      string    // 屏障 hash 的 pending;字段或 hash 不存在为 ""
	Acks         [2]string // 两表 ACK(顺序同 saveBarrierAckTables);缺失为 ""
	// PlacementBad:落点记录畸形,go/db 对他 fail-closed,算不出该去哪个库读行。
	PlacementBad bool
	RowFence     [2]string // 有效落点库两张表的行内围栏 saveBarrierVersion(epoch, seq)
	RowPresent   [2]bool
}

// judgeSaveBarrier 的结论。字面量进 Notes,低基数;barrierVerdictOK 表示通过或不要求。
const (
	barrierVerdictOK               = ""
	barrierVerdictOwnerMalformed   = "owner_epoch_malformed"
	barrierVerdictMissing          = "barrier_missing"
	barrierVerdictPendingMalformed = "pending_malformed"
	barrierVerdictOwnerBehind      = "owner_epoch_behind_pending"
	barrierVerdictUncommitted      = "ack_not_equal_pending"
	barrierVerdictPlacementBad     = "placement_record_malformed"
	barrierVerdictRowMissing       = "sql_row_missing"
	barrierVerdictFenceMismatch    = "sql_row_fence_not_equal_pending"
)

// saveBarrierRequired:owner_epoch 存在且不是 "0"(进过场景)才要求屏障;畸形值也算要求,交给 judgeSaveBarrier 报出来。
func saveBarrierRequired(o saveBarrierObservation) bool {
	return o.OwnerPresent && o.OwnerRaw != "0"
}

// judgeSaveBarrier 按 login 冷加载的顺序复刻它的拒绝条件(纯函数)。
func judgeSaveBarrier(o saveBarrierObservation) string {
	if !saveBarrierRequired(o) {
		return barrierVerdictOK
	}
	owner, err := strconv.ParseUint(o.OwnerRaw, 10, 64)
	if err != nil || strconv.FormatUint(owner, 10) != o.OwnerRaw {
		return barrierVerdictOwnerMalformed
	}
	if o.Pending == "" {
		return barrierVerdictMissing
	}
	epoch, seq, ok := parseSaveBarrierVersion(o.Pending)
	if !ok || epoch == 0 || seq == 0 {
		return barrierVerdictPendingMalformed
	}
	if epoch > owner {
		return barrierVerdictOwnerBehind
	}
	for _, ack := range o.Acks {
		if ack != o.Pending {
			return barrierVerdictUncommitted
		}
	}
	if o.PlacementBad {
		return barrierVerdictPlacementBad
	}
	for i := range o.RowFence {
		if !o.RowPresent[i] {
			return barrierVerdictRowMissing
		}
		if o.RowFence[i] != o.Pending {
			return barrierVerdictFenceMismatch
		}
	}
	return barrierVerdictOK
}

// rowFenceReader 读 schema.table 里 ids 的行内围栏:player_id → saveBarrierVersion(fence_owner_epoch, fence_save_seq)。
// 没有行的 id 不出现在返回值里。单测经它注入替身(本 module 没有 sqlmock)。
type rowFenceReader func(ctx context.Context, schema, table string, ids []uint64) (map[uint64]string, error)

// sqlRowFenceReader 是 rowFenceReader 的 MySQL 实现。非锁定读、分批 IN;schema 只来自 storeDBName,table 只来自
// saveBarrierAckTables,都不是用户输入。缺 fence 列(还没跑围栏迁移)时查询报错,调用方报 INFRA。
func sqlRowFenceReader(db *sql.DB) rowFenceReader {
	return func(ctx context.Context, schema, table string, ids []uint64) (map[uint64]string, error) {
		if db == nil {
			return nil, errors.New("no MySQL handle")
		}
		out := make(map[uint64]string, len(ids))
		for _, batch := range chunkUint64(sortedUint64(ids), playerRowsBatchSize) {
			rows, err := db.QueryContext(ctx, fmt.Sprintf(
				"SELECT %s, fence_owner_epoch, fence_save_seq FROM %s.%s WHERE %s IN (%s)",
				playerIDColumn, schema, table, playerIDColumn, inListLiteral(batch)))
			if err != nil {
				return nil, fmt.Errorf("read row fences of %s.%s: %w", schema, table, err)
			}
			for rows.Next() {
				var id, epoch, seq uint64
				if err := rows.Scan(&id, &epoch, &seq); err != nil {
					rows.Close()
					return nil, fmt.Errorf("scan row fence of %s.%s: %w", schema, table, err)
				}
				out[id] = saveBarrierVersion(epoch, seq)
			}
			err = rows.Err()
			rows.Close()
			if err != nil {
				return nil, err
			}
		}
		return out, nil
	}
}

// verifySaveBarriers 是 -verify-merged 的 verify:save_barrier 一行(见文件头)。
func verifySaveBarriers(ctx context.Context, cfg auditConfig) ResourceAudit {
	return verifySaveBarriersWith(ctx, cfg, sqlRowFenceReader(cfg.db))
}

func verifySaveBarriersWith(ctx context.Context, cfg auditConfig, readFences rowFenceReader) ResourceAudit {
	r := ResourceAudit{Name: "verify:save_barrier", UniqueScope: "global"}
	m, err := cfg.verifyManifest()
	if err != nil {
		return infraAudit(r.Name, "%v", err)
	}
	r.SourceCount = int64(len(m.PlayerIDs))
	// 与 verify:manifest_rows 同一道闸:玩家行这一步没做完,行还不在 login 要读的库里,拿屏障去比它只会得出错误结论。
	mode := m.rowsMode()
	if gate := rowsStepFor(mode); !m.stepDone(gate) {
		r.Severity = "warn"
		r.Notes = fmt.Sprintf("NOT VERIFIED: step %s (-player-rows-mode %s) is not recorded as done in the manifest — "+
			"save barriers were not checked against the players' rows", gate, mode)
		return r
	}
	if cfg.sharedRDB == nil {
		return infraAudit(r.Name, "no shared (DB 0) Redis handle — owner_epoch and save_barrier live there")
	}
	obs, required, err := readSaveBarrierState(ctx, cfg.sharedRDB, m.PlayerIDs)
	if err != nil {
		return infraAudit(r.Name, "%v", err)
	}
	if len(required) == 0 {
		r.Severity = "info"
		r.Notes = fmt.Sprintf("none of the %d manifest players has a non-zero owner_epoch (never entered a scene): "+
			"no save barrier is required", len(m.PlayerIDs))
		return r
	}
	if err := readRequiredRowFences(ctx, cfg, required, obs, readFences); err != nil {
		return infraAudit(r.Name, "%v", err)
	}

	byVerdict := map[string][]uint64{}
	for _, id := range required {
		if v := judgeSaveBarrier(*obs[id]); v != barrierVerdictOK {
			byVerdict[v] = append(byVerdict[v], id)
		}
	}
	bad := 0
	for _, ids := range byVerdict {
		bad += len(ids)
	}
	r.TargetCount = int64(len(required) - bad)
	r.ConflictCount = int64(bad)
	if bad > 0 {
		verdicts := make([]string, 0, len(byVerdict))
		for v := range byVerdict {
			verdicts = append(verdicts, v)
		}
		sort.Strings(verdicts)
		parts := make([]string, 0, len(verdicts))
		for _, v := range verdicts {
			parts = append(parts, fmt.Sprintf("%s=%d (first ids: %v)", v, len(byVerdict[v]), sampleUint64(byVerdict[v])))
		}
		r.Severity = "block"
		r.Notes = fmt.Sprintf("%d of the %d manifest players with a non-zero owner_epoch would be refused by the login cold load: %s. "+
			"Keep the maintenance gate and rebuild their barriers with save_barrier_restore (docs/ops/merge-zone-runbook.md §10.1) "+
			"before zone-up", bad, len(required), strings.Join(parts, "; "))
		return r
	}
	r.Severity = "info"
	r.Notes = fmt.Sprintf("all %d manifest players with a non-zero owner_epoch have a committed save_barrier equal to both "+
		"row fences in their placement store; %d never entered a scene (no barrier required)",
		len(required), len(m.PlayerIDs)-len(required))
	return r
}

// readSaveBarrierState 读每名清单玩家的 owner_epoch(MGET),再对 owner_epoch≠0 的人(required,按 ids 顺序)读屏障
// 三字段(HMGET pipeline)。两类键都在共享核心 Redis 上(与父快照同实例,login 的 Lua 要求如此);任一读失败整体报错,
// 不能把读失败当成「没有屏障」。
func readSaveBarrierState(ctx context.Context, rdb redis.Cmdable, ids []uint64) (
	map[uint64]*saveBarrierObservation, []uint64, error) {
	obs := make(map[uint64]*saveBarrierObservation, len(ids))
	var required []uint64
	for _, batch := range chunkUint64(ids, saveBarrierReadBatch) {
		keys := make([]string, len(batch))
		for i, id := range batch {
			keys[i] = playerOwnerEpochKey(id)
		}
		vals, err := rdb.MGet(ctx, keys...).Result()
		if err != nil {
			return nil, nil, fmt.Errorf("owner_epoch mget (%d keys): %w", len(keys), err)
		}
		if len(vals) != len(batch) {
			return nil, nil, fmt.Errorf("owner_epoch mget returned %d values for %d keys", len(vals), len(batch))
		}
		for i, id := range batch {
			o := &saveBarrierObservation{}
			if s, ok := vals[i].(string); ok {
				o.OwnerRaw, o.OwnerPresent = s, true
			}
			obs[id] = o
			if saveBarrierRequired(*o) {
				required = append(required, id)
			}
		}
	}
	fields := []string{saveBarrierPendingField, saveBarrierAckTables[0], saveBarrierAckTables[1]}
	for _, batch := range chunkUint64(required, saveBarrierReadBatch) {
		pipe := rdb.Pipeline()
		cmds := make([]*redis.SliceCmd, len(batch))
		for i, id := range batch {
			cmds[i] = pipe.HMGet(ctx, playerSaveBarrierKey(strconv.FormatUint(id, 10)), fields...)
		}
		if _, err := pipe.Exec(ctx); err != nil {
			return nil, nil, fmt.Errorf("save_barrier hmget (%d players): %w", len(batch), err)
		}
		for i, id := range batch {
			vals := cmds[i].Val()
			if len(vals) != len(fields) {
				return nil, nil, fmt.Errorf("save_barrier hmget for player %d returned %d values", id, len(vals))
			}
			o := obs[id]
			o.Pending, _ = vals[0].(string)
			o.Acks[0], _ = vals[1].(string)
			o.Acks[1], _ = vals[2].(string)
		}
	}
	return obs, required, nil
}

// readRequiredRowFences 按有效落点(expectedMainRowStore:有记录用记录,没有用 dst)分库读 required 两张表的行内围栏。
func readRequiredRowFences(ctx context.Context, cfg auditConfig, required []uint64,
	obs map[uint64]*saveBarrierObservation, readFences rowFenceReader) error {
	routes, err := cfg.routeReads()
	if err != nil {
		return fmt.Errorf("%v (placement records decide which database login reads each player's rows from)", err)
	}
	reads, err := routes.placements(ctx, required)
	if err != nil {
		return err
	}
	if len(reads) != len(required) {
		return fmt.Errorf("placement read returned %d answers for %d players", len(reads), len(required))
	}
	byStore := map[uint32][]uint64{}
	for i, id := range required {
		store, ok := expectedMainRowStore(reads[i], cfg.dst)
		if !ok {
			obs[id].PlacementBad = true
			continue
		}
		byStore[store] = append(byStore[store], id)
	}
	for _, store := range sortedZones(storeSet(byStore)) {
		name, ok := storeDBName(store)
		if !ok {
			return fmt.Errorf("placement store %d has no database name", store)
		}
		ids := byStore[store]
		for t, table := range saveBarrierAckTables {
			fences, err := readFences(ctx, name, table, ids)
			if err != nil {
				return err
			}
			for _, id := range ids {
				if v, found := fences[id]; found {
					obs[id].RowFence[t], obs[id].RowPresent[t] = v, true
				}
			}
		}
	}
	return nil
}
