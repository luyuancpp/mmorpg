// Package barrierrestore 在停写维护期间从权威 SQL 恢复历史玩家的提交屏障。
// 它不恢复归属计数器、不清缓存，也不能替代停旧写者与审定 Kafka/retry 的运维门禁。
package barrierrestore

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"net"
	"strconv"
	"strings"
	"time"

	databasepb "proto/common/database"
	"shared/ownerepoch"
	"shared/placement"
	"shared/savebarrier"

	"github.com/redis/go-redis/v9"
)

const Format = "player-save-barrier-restore-v1"
const FormatV2 = "player-save-barrier-restore-v2"
const RoutingModeRedis = "redis-same-instance"
const RoutingModeSplit = "redis-split"
const RoutingModeDBFirst = "db_first"

// Plan 只包含身份与摘要。它不是停写证明，执行时仍须独立维持维护窗口。
type Plan struct {
	Format                 string   `json:"format"`
	Schema                 string   `json:"schema"`
	Zone                   uint32   `json:"home_zone"`
	MaintenanceID          string   `json:"maintenance_id"`
	AllowLegacy            bool     `json:"allow_legacy"`
	RedisAddr              string   `json:"redis_addr"`
	RedisDB                int      `json:"redis_db"`
	RoutingMode            string   `json:"routing_mode"`
	LocalMySQL             bool     `json:"local_mysql"`
	MappingAddr            string   `json:"mapping_addr,omitempty"`
	MappingDB              int      `json:"mapping_db,omitempty"`
	RouteSchema            string   `json:"route_schema,omitempty"`
	AuditedOwnerUpperBound string   `json:"audited_owner_upper_bound,omitempty"`
	Players                []Player `json:"players"`
}

type Player struct {
	ID              string    `json:"player_id"`
	Owner           string    `json:"owner_epoch"`
	Original        string    `json:"original_version"`
	Target          string    `json:"target_version"`
	Rows            [2]string `json:"asset_row_sha256"`
	HomeZone        string    `json:"home_zone_value"`
	Placement       string    `json:"placement_value"`
	RouteGeneration string    `json:"route_generation,omitempty"`
	RouteWatermark  string    `json:"route_watermark,omitempty"`
	RouteRevision   string    `json:"route_revision,omitempty"`
}

// Restorer 的两个依赖均为维护专用连接，不能与正在接流量的业务池共用。
type Restorer struct {
	DB                     *sql.DB
	Redis                  *redis.Client
	RoutingMode            string
	LocalMySQL             bool
	MappingRedis           *redis.Client
	RouteSchema            string
	AuditedOwnerUpperBound string
}

// 首版只恢复 home zone 自己的 zone 库。异区钉落点/player_store 必须另验权威 placement，不能猜。
func validTarget(schema string, zone uint32, addr string, db int) bool {
	host, port, err := net.SplitHostPort(addr)
	p, pErr := strconv.ParseUint(port, 10, 16)
	return zone > 0 && zone <= 999999 && schema == fmt.Sprintf("zone_%d_db", zone) &&
		err == nil && pErr == nil && p > 0 && host != "" && !strings.ContainsAny(host, " /\\@\r\n\t") && db == 0
}

func (p Plan) Validate() error {
	if p.ValidateTarget() != nil ||
		len(p.MaintenanceID) == 0 || len(p.MaintenanceID) > 128 || len(p.Players) == 0 || len(p.Players) > 1000 {
		return errors.New("维护计划格式、范围或工单号无效")
	}
	seen := make(map[string]bool, len(p.Players))
	var audited uint64
	if p.Format == FormatV2 {
		var err error
		audited, err = auditedBound(p.AuditedOwnerUpperBound)
		if err != nil {
			return err
		}
	} else if p.AuditedOwnerUpperBound != "" {
		return errors.New("v1 不接受新增恢复证明")
	}
	for _, row := range p.Players {
		if _, err := positive(row.ID); err != nil || seen[row.ID] {
			return errors.New("维护计划玩家号无效或重复")
		}
		seen[row.ID] = true
		if err := validatePlannedRoute(p, row); err != nil {
			return err
		}
		owner, err := positive(row.Owner)
		if err != nil {
			return errors.New("维护计划归属计数器无效")
		}
		if p.Format == FormatV2 && owner <= audited {
			return errors.New("当前 owner 未严格高于独立审核的旧代上界")
		}
		e, s, err := savebarrier.Parse(row.Original)
		if err != nil || (e == 0) != (s == 0) || e > owner {
			return errors.New("维护计划原始围栏无效")
		}
		want := row.Original
		if e == 0 {
			if !p.AllowLegacy {
				return errors.New("零围栏需要显式允许历史迁移")
			}
			want = savebarrier.Version(owner, 1)
		}
		if row.Target != want {
			return errors.New("维护计划目标围栏无效")
		}
		for _, digest := range row.Rows {
			b, err := hex.DecodeString(digest)
			if err != nil || len(b) != sha256.Size || hex.EncodeToString(b) != digest {
				return errors.New("维护计划 SQL 行摘要无效")
			}
		}
	}
	return nil
}

func positive(raw string) (uint64, error) {
	v, err := strconv.ParseUint(raw, 10, 64)
	if err != nil || v == 0 || strconv.FormatUint(v, 10) != raw {
		return 0, errors.New("非规范正整数")
	}
	return v, nil
}

// Prepare 在两行点锁下生成计划，只读业务数据；不写 SQL、Redis 或资产明文。
func (r Restorer) Prepare(ctx context.Context, schema string, zone uint32, maintenanceID string, ids []string, allowLegacy bool) (Plan, error) {
	p := Plan{Format: Format, Schema: schema, Zone: zone, MaintenanceID: maintenanceID, AllowLegacy: allowLegacy, RedisAddr: r.Redis.Options().Addr, RedisDB: r.Redis.Options().DB, RoutingMode: r.RoutingMode, LocalMySQL: r.LocalMySQL}
	if r.MappingRedis != nil || r.RouteSchema != "" || r.RoutingMode != RoutingModeRedis || schema != fmt.Sprintf("zone_%d_db", zone) || p.RedisDB != 0 || r.AuditedOwnerUpperBound != "" {
		p.Format, p.RouteSchema = FormatV2, r.RouteSchema
		p.AuditedOwnerUpperBound = r.AuditedOwnerUpperBound
		p.MappingAddr, p.MappingDB = r.mapping().Options().Addr, r.mapping().Options().DB
	}
	// 先校验范围，避免非法输入触发连接或读取。
	if p.ValidateTarget() != nil || len(ids) == 0 || len(ids) > 1000 || len(maintenanceID) == 0 || len(maintenanceID) > 128 {
		return p, errors.New("维护范围无效")
	}
	seen := map[string]bool{}
	if p.Format == FormatV2 {
		if _, err := auditedBound(p.AuditedOwnerUpperBound); err != nil {
			return p, err
		}
	}
	for _, id := range ids {
		if _, err := positive(id); err != nil || seen[id] {
			return p, errors.New("玩家号无效或重复")
		}
		seen[id] = true
	}
	for _, id := range ids {
		row, err := r.prepareOne(ctx, p, id)
		if err != nil {
			return p, fmt.Errorf("玩家 %s 预检失败: %w", id, err)
		}
		p.Players = append(p.Players, row)
	}
	return p, p.Validate()
}

func (r Restorer) prepareOne(ctx context.Context, p Plan, id string) (Player, error) {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	tx, err := r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return Player{}, err
	}
	defer tx.Rollback()
	row, err := r.lockRoute(ctx, tx, p, id)
	if err != nil {
		return Player{}, err
	}
	rows, err := readPair(ctx, tx, p.Schema, id)
	if err != nil {
		return Player{}, err
	}
	pid, _ := positive(id)
	// GET 仅获取计数器；Lua 在同一原子段复核它，不能把此预检当成发布点。
	owner, err := r.Redis.Get(ctx, ownerepoch.OwnerEpochKey(pid)).Result()
	if err != nil {
		return Player{}, errors.New("归属计数器缺失或无法读取，需要先单独恢复计数器")
	}
	n, err := positive(owner)
	if err != nil || n < rows[0].epoch {
		return Player{}, errors.New("归属计数器缺失、畸形或落后于 SQL")
	}
	row.Owner, row.Original, row.Rows = owner, savebarrier.Version(rows[0].epoch, rows[0].seq), [2]string{rows[0].digest, rows[1].digest}
	row.Target = row.Original
	if rows[0].epoch == 0 {
		if !p.AllowLegacy {
			return Player{}, errors.New("历史零围栏未获迁移选项")
		}
		row.Target = savebarrier.Version(n, 1)
	}
	if err := r.inspect(ctx, p, row, false, row.Original == row.Target); err != nil {
		return Player{}, err
	}
	return row, nil
}

// ApplyOne 每次只提交一名计划内玩家。失败保留维护标记，已成功的前缀可按原计划重入。
// 零围栏先原子提交双表元数据，再重新点锁强读复核，锁内才发布 Redis；SQL 提交失败绝不发布 ACK。
func (r Restorer) ApplyOne(ctx context.Context, p Plan, index int) error {
	if err := p.Validate(); err != nil {
		return err
	}
	if p.RedisAddr != r.Redis.Options().Addr || p.RedisDB != r.Redis.Options().DB || p.RoutingMode != r.RoutingMode || p.LocalMySQL != r.LocalMySQL || p.RouteSchema != r.RouteSchema || (p.Format == FormatV2 && (p.MappingAddr != r.mapping().Options().Addr || p.MappingDB != r.mapping().Options().DB)) {
		return errors.New("实际 Redis 连接或路由模式偏离审定计划")
	}
	if p.Format == Format && (r.mapping().Options().Addr != p.RedisAddr || r.mapping().Options().DB != 0) {
		return errors.New("v1 实际映射必须与父缓存同实例 DB0")
	}
	if p.AuditedOwnerUpperBound != r.AuditedOwnerUpperBound {
		return errors.New("独立审核的旧代上界偏离计划")
	}
	if index < 0 || index >= len(p.Players) {
		return errors.New("维护计划玩家索引越界")
	}
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	row := p.Players[index]
	tx, err := r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		return err
	}
	defer func() {
		if tx != nil {
			_ = tx.Rollback()
		}
	}()
	if err := r.recheckRoute(ctx, tx, p, row); err != nil {
		return err
	}
	current, err := readPair(ctx, tx, p.Schema, row.ID)
	if err != nil {
		return err
	}
	if err := match(row, current); err != nil {
		return err
	}
	if err := r.inspect(ctx, p, row, false, savebarrier.Version(current[0].epoch, current[0].seq) == row.Target); err != nil {
		return err
	}
	if savebarrier.Version(current[0].epoch, current[0].seq) != row.Target {
		e, s, _ := savebarrier.Parse(row.Target)
		for _, table := range []string{"player_database", "player_database_1"} {
			result, err := tx.ExecContext(ctx, "UPDATE `"+p.Schema+"`."+table+" SET fence_owner_epoch=?,fence_save_seq=? WHERE player_id=? AND fence_owner_epoch=0 AND fence_save_seq=0", e, s, row.ID)
			if err != nil {
				return err
			}
			n, err := result.RowsAffected()
			if err != nil || n != 1 {
				return errors.New("历史围栏迁移 CAS 失败")
			}
		}
		if err := tx.Commit(); err != nil {
			return fmt.Errorf("双表围栏提交失败，未发布屏障: %w", err)
		}
		tx, err = r.DB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
		if err != nil {
			return err
		}
		if err := r.recheckRoute(ctx, tx, p, row); err != nil {
			return err
		}
		current, err = readPair(ctx, tx, p.Schema, row.ID)
		if err != nil {
			return err
		}
		if err := match(row, current); err != nil {
			return err
		}
		if savebarrier.Version(current[0].epoch, current[0].seq) != row.Target {
			return errors.New("已提交围栏在重锁期间改变")
		}
	}
	if err := r.inspect(ctx, p, row, true, true); err != nil {
		return err
	}
	// 本事务只有锁定读。Redis 已确认真实提交的 SQL，结束读事务不改变资产结局。
	return tx.Rollback()
}

type sqlRow struct {
	epoch, seq uint64
	digest     string
}

func readPair(ctx context.Context, tx *sql.Tx, schema, id string) ([2]sqlRow, error) {
	var pair [2]sqlRow
	for i, table := range []string{"player_database", "player_database_1"} {
		row, err := readRow(ctx, tx, schema, table, id)
		if err != nil {
			return pair, err
		}
		pair[i] = row
	}
	if pair[0].epoch != pair[1].epoch || pair[0].seq != pair[1].seq || (pair[0].epoch == 0) != (pair[0].seq == 0) {
		return pair, errors.New("两张 SQL 行围栏不同版或不完整")
	}
	return pair, nil
}

func readRow(ctx context.Context, tx *sql.Tx, schema, table, id string) (sqlRow, error) {
	rows, err := tx.QueryContext(ctx, "SELECT * FROM `"+schema+"`."+table+" WHERE player_id=? FOR UPDATE", id)
	if err != nil {
		return sqlRow{}, err
	}
	defer rows.Close()
	columns, err := rows.Columns()
	if err != nil {
		return sqlRow{}, err
	}
	if !rows.Next() {
		if err := rows.Err(); err != nil {
			return sqlRow{}, err
		}
		return sqlRow{}, errors.New("SQL 玩家行缺失")
	}
	values := make([]sql.RawBytes, len(columns))
	args := make([]any, len(columns))
	for i := range values {
		args[i] = &values[i]
	}
	if err := rows.Scan(args...); err != nil {
		return sqlRow{}, err
	}
	var row sqlRow
	found := [3]bool{}
	h := sha256.New()
	for i, col := range columns {
		v := values[i]
		switch col {
		case "fence_owner_epoch", "fence_save_seq":
			n, err := strconv.ParseUint(string(v), 10, 64)
			if err != nil || strconv.FormatUint(n, 10) != string(v) {
				return row, errors.New("SQL 围栏值无效")
			}
			if col == "fence_owner_epoch" {
				row.epoch, found[1] = n, true
			} else {
				row.seq, found[2] = n, true
			}
			continue
		case "player_id":
			if string(v) != id {
				return row, errors.New("SQL 行玩家身份不符")
			}
			found[0] = true
		}
		// 长度前缀与 NULL 标记避免字段边界或 NULL/空串出现相同摘要。
		var size [8]byte
		binary.BigEndian.PutUint64(size[:], uint64(len(col)))
		h.Write(size[:])
		h.Write([]byte(col))
		if v == nil {
			h.Write([]byte{0})
		} else {
			h.Write([]byte{1})
			binary.BigEndian.PutUint64(size[:], uint64(len(v)))
			h.Write(size[:])
			h.Write(v)
		}
	}
	if found != [3]bool{true, true, true} {
		return row, errors.New("SQL 玩家身份或围栏列缺失")
	}
	if rows.Next() {
		return row, errors.New("SQL 玩家主键不唯一")
	}
	if err := rows.Err(); err != nil {
		return row, err
	}
	row.digest = hex.EncodeToString(h.Sum(nil))
	return row, nil
}

func match(want Player, current [2]sqlRow) error {
	v := savebarrier.Version(current[0].epoch, current[0].seq)
	if v != want.Original && v != want.Target {
		return errors.New("SQL 围栏偏离审定计划")
	}
	for i := range current {
		if current[i].digest != want.Rows[i] {
			return errors.New("SQL 资产行偏离审定计划")
		}
	}
	return nil
}

func validRoute(zone uint32, home, raw string) error {
	homeZone, present, err := placement.ParseHomeZone(home)
	if err != nil || !present || homeZone != zone || home != strconv.FormatUint(uint64(zone), 10) {
		return errors.New("玩家 home zone 与计划不符")
	}
	rec, err := placement.Parse(raw)
	if err != nil || rec.Frozen || rec.StorageID != zone || rec.Encode() != raw {
		return errors.New("玩家不在本区稳定落点，须走有权威路由证明的维护流程")
	}
	return nil
}

func MaintenanceKey(zone uint32) string {
	return fmt.Sprintf("player:save_barrier:maintenance:%d", zone)
}

func recoveryKeys(zone uint32, id uint64) []string {
	parent := string((&databasepb.PlayerAllData{}).ProtoReflect().Descriptor().FullName())
	first := string((&databasepb.PlayerDatabase{}).ProtoReflect().Descriptor().FullName())
	second := string((&databasepb.PlayerDatabase_1{}).ProtoReflect().Descriptor().FullName())
	keys := []string{MaintenanceKey(zone), ownerepoch.OwnerEpochKey(id), fmt.Sprintf("%s:%d", parent, id), fmt.Sprintf("%s:%d", first, id), fmt.Sprintf("%s:%d", second, id), fmt.Sprintf("player:{%d}:asset_sql_commit", id), ownerepoch.HandoffKey(id), savebarrier.Key(id)}
	for bucket := 0; bucket < 64; bucket++ {
		keys = append(keys, fmt.Sprintf("player:save_pending:%d:%d", zone, bucket))
	}
	keys = append(keys, placement.HomeZoneKey(id), placement.Key(id), placement.MergeFenceKey(zone))
	return keys
}

func (r Restorer) inspect(ctx context.Context, p Plan, row Player, publish, committed bool) error {
	id, _ := positive(row.ID)
	write := "0"
	if publish {
		write = "1"
	}
	allowExisting := "0"
	if committed {
		allowExisting = "1"
	}
	keys := recoveryKeys(p.Zone, id)
	if p.RoutingMode != RoutingModeRedis {
		if err := r.inspectMapping(ctx, p, row); err != nil {
			return err
		}
		keys = keys[:72]
	}
	n, err := restoreScript.Run(ctx, r.Redis, keys, p.MaintenanceID, row.Owner, row.Target, write, allowExisting, row.HomeZone, row.Placement).Int()
	if err != nil {
		return fmt.Errorf("维护屏障检查拒绝: %w", err)
	}
	if n != 1 {
		return errors.New("维护屏障回复无效")
	}
	if p.RoutingMode != RoutingModeRedis {
		return r.inspectMapping(ctx, p, row)
	}
	return nil
}
