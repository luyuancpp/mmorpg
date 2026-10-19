package barrierrestore

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"net"
	"strconv"
	"strings"

	"github.com/redis/go-redis/v9"
	"shared/placement"
)

func validAddress(addr string) bool {
	host, port, err := net.SplitHostPort(addr)
	n, e := strconv.ParseUint(port, 10, 16)
	return err == nil && e == nil && n > 0 && host != "" && !strings.ContainsAny(host, " /\\@\r\n\t")
}

func validSchema(s string) bool {
	if len(s) == 0 || len(s) > 64 {
		return false
	}
	for i, c := range s {
		if c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(i > 0 && c >= '0' && c <= '9') {
			return false
		}
	}
	return true
}

// ValidateTarget 供 CLI 在连接前校验目标；v1 的范围保持不变。
func (p Plan) ValidateTarget() error {
	if p.Format == Format {
		if p.RoutingMode != RoutingModeRedis || !validTarget(p.Schema, p.Zone, p.RedisAddr, p.RedisDB) || p.MappingAddr != "" || p.MappingDB != 0 || p.RouteSchema != "" {
			return errors.New("v1 目标范围无效")
		}
		return nil
	}
	_, known := placement.StorageIDFromDBName(p.Schema)
	if p.Format != FormatV2 || !known || p.Zone == 0 || p.Zone > placement.MaxZoneStorageID || !validAddress(p.RedisAddr) || p.RedisDB < 0 || p.RedisDB > 65535 || !validAddress(p.MappingAddr) || p.MappingDB != 0 {
		return errors.New("v2 目标范围无效")
	}
	switch p.RoutingMode {
	case RoutingModeRedis:
		if p.MappingAddr != p.RedisAddr || p.RedisDB != 0 || p.RouteSchema != "" {
			return errors.New("同实例路由要求同地址 DB0")
		}
	case RoutingModeSplit:
		if p.RouteSchema != "" {
			return errors.New("Redis 权威路由不接受 SQL 路由 schema")
		}
	case RoutingModeDBFirst:
		if !validSchema(p.RouteSchema) {
			return errors.New("db_first 必须显式指定同一 SQL 实例内的路由 schema")
		}
	default:
		return errors.New("未知权威路由模式")
	}
	return nil
}

func validatePlannedRoute(p Plan, row Player) error {
	if p.Format == Format {
		if row.RouteGeneration != "" || row.RouteWatermark != "" || row.RouteRevision != "" {
			return errors.New("v1 不接受 SQL 路由证明")
		}
		return validRoute(p.Zone, row.HomeZone, row.Placement)
	}
	if row.HomeZone != strconv.FormatUint(uint64(p.Zone), 10) {
		return errors.New("home zone 偏离计划")
	}
	storage, _ := placement.StorageIDFromDBName(p.Schema)
	if row.Placement == "" {
		if p.RoutingMode != RoutingModeDBFirst || storage != p.Zone {
			return errors.New("仅 db_first 本区空落点可按 home 回退")
		}
	} else {
		rec, err := placement.Parse(row.Placement)
		if err != nil || rec.Frozen || rec.StorageID != storage || rec.Encode() != row.Placement {
			return errors.New("稳定落点与实际 SQL schema 不符")
		}
	}
	if p.RoutingMode == RoutingModeDBFirst {
		if len(row.RouteGeneration) != 32 || strings.Trim(row.RouteGeneration, "0123456789abcdef") != "" {
			return errors.New("路由库世代无效")
		}
		if _, err := positive(row.RouteRevision); err != nil {
			return errors.New("路由 rev 无效")
		}
		if _, err := positive(row.RouteWatermark); err != nil {
			return errors.New("路由库水位无效")
		}
	} else if row.RouteGeneration != "" || row.RouteRevision != "" || row.RouteWatermark != "" {
		return errors.New("Redis 权威路由不得夹带 SQL 路由证明")
	}
	return nil
}

func (r Restorer) mapping() *redis.Client {
	if r.MappingRedis != nil {
		return r.MappingRedis
	}
	return r.Redis
}

// 同一事务、同一 TiDB 实例内先锁路由再锁资产；锁序与 route_tool 解冻一致。
// 只读路由，绝不在恢复工具中维护第二份路由真相。
func (r Restorer) lockRoute(ctx context.Context, tx *sql.Tx, p Plan, id string) (Player, error) {
	row := Player{ID: id}
	pid, _ := positive(id)
	if p.RoutingMode != RoutingModeDBFirst {
		var err error
		row.HomeZone, err = r.mapping().Get(ctx, placement.HomeZoneKey(pid)).Result()
		if err != nil {
			return row, errors.New("权威 home zone 映射缺失或不可读")
		}
		row.Placement, err = r.mapping().Get(ctx, placement.Key(pid)).Result()
		if err != nil {
			return row, errors.New("显式 placement 映射缺失或不可读")
		}
		return row, validatePlannedRoute(p, row)
	}
	prefix := "`" + p.RouteSchema + "`."
	if err := tx.QueryRowContext(ctx, "SELECT watermark_ms FROM "+prefix+"route_reconcile WHERE id=1 FOR UPDATE").Scan(&row.RouteWatermark); err != nil {
		return row, fmt.Errorf("路由水位锁定失败: %w", err)
	}
	var writable uint64
	if err := tx.QueryRowContext(ctx, "SELECT generation,writable FROM "+prefix+"route_meta WHERE id=1 FOR UPDATE").Scan(&row.RouteGeneration, &writable); err != nil {
		return row, fmt.Errorf("路由世代锁定失败: %w", err)
	}
	if writable != 0 {
		return row, errors.New("必须先通过 route_tool freeze 冻结 SQL 路由写者")
	}
	var conflicts uint64
	if err := tx.QueryRowContext(ctx, "SELECT COUNT(*) FROM "+prefix+"zone_merged_into WHERE src_zone=?", p.Zone).Scan(&conflicts); err != nil {
		return row, err
	}
	if conflicts != 0 {
		return row, errors.New("home zone 已合走，不能用源区残留行恢复")
	}
	if err := tx.QueryRowContext(ctx, "SELECT COUNT(*) FROM "+prefix+"zone_merge_fence WHERE zone_id=? AND run_id IS NOT NULL AND expires_at_ms>CAST(UNIX_TIMESTAMP(NOW(3))*1000 AS UNSIGNED)", p.Zone).Scan(&conflicts); err != nil {
		return row, err
	}
	if conflicts != 0 {
		return row, errors.New("SQL 合服围栏仍有效，须先完成或裁决旧维护")
	}
	var home, storage, version, frozen sql.NullString
	query := "SELECT home_zone,storage_id,placement_version,frozen_run_id,rev FROM " + prefix + "player_route WHERE route_shard=? AND player_id=? FOR UPDATE"
	if err := tx.QueryRowContext(ctx, query, placement.RouteRowShard(pid), id).Scan(&home, &storage, &version, &frozen, &row.RouteRevision); err != nil {
		return row, fmt.Errorf("完整主键路由锁定失败: %w", err)
	}
	if !home.Valid || (frozen.Valid && frozen.String != "") || storage.Valid != version.Valid {
		return row, errors.New("路由缺归属、正在搬迁或列不完整")
	}
	row.HomeZone = home.String
	if storage.Valid {
		s, err := positive(storage.String)
		v, vErr := positive(version.String)
		if err != nil || vErr != nil || s > uint64(^uint32(0)) {
			return row, errors.New("SQL 落点字段无效")
		}
		row.Placement = placement.StableValue(uint32(s), v)
	}
	return row, validatePlannedRoute(p, row)
}

func (r Restorer) recheckRoute(ctx context.Context, tx *sql.Tx, p Plan, want Player) error {
	got, err := r.lockRoute(ctx, tx, p, want.ID)
	if err != nil {
		return err
	}
	if got.HomeZone != want.HomeZone || got.Placement != want.Placement || got.RouteGeneration != want.RouteGeneration || got.RouteWatermark != want.RouteWatermark || got.RouteRevision != want.RouteRevision {
		return errors.New("权威路由偏离审定计划")
	}
	return nil
}

func routeGuardValue(maintenance string) string { return "save_barrier_restore:" + maintenance }

// 上界须来自独立保留并审定的旧代证据；0 也是显式证明，空值不能等同于 0。
func auditedBound(raw string) (uint64, error) {
	n, err := strconv.ParseUint(raw, 10, 64)
	if err != nil || strconv.FormatUint(n, 10) != raw {
		return 0, errors.New("v2 必须提供独立审定的旧 owner 上界，不能从回退 SQL 或当前计数器猜测")
	}
	return n, nil
}

// Guard 只建立永久维护元数据。失败保留已建闸；不会代替停业务/路由写者或 SQL freeze。
func (r Restorer) Guard(ctx context.Context, p Plan) error {
	if err := p.ValidateTarget(); err != nil {
		return err
	}
	if p.Format != FormatV2 || p.RoutingMode == RoutingModeRedis || p.MaintenanceID == "" || len(p.MaintenanceID) > 128 {
		return errors.New("guard 只用于 v2 跨接缝维护")
	}
	if p.RedisAddr != r.Redis.Options().Addr || p.RedisDB != r.Redis.Options().DB || p.MappingAddr != r.mapping().Options().Addr || p.MappingDB != r.mapping().Options().DB || p.RoutingMode != r.RoutingMode || p.RouteSchema != r.RouteSchema {
		return errors.New("维护闸连接目标不符")
	}
	// 第一把闸若建成、第二把失败，必须保留第一把，避免误放流。
	if err := guardScript.Run(ctx, r.Redis, []string{MaintenanceKey(p.Zone)}, p.MaintenanceID).Err(); err != nil {
		return err
	}
	return guardScript.Run(ctx, r.mapping(), []string{MaintenanceKey(p.Zone), placement.MergeFenceKey(p.Zone)}, p.MaintenanceID, routeGuardValue(p.MaintenanceID)).Err()
}

var guardScript = redis.NewScript(`
if #KEYS ~= #ARGV then return redis.error_reply('guard arguments invalid') end
for i=1,#KEYS do
 local t=redis.call('TYPE',KEYS[i]).ok
 if t ~= 'none' and (t ~= 'string' or redis.call('GET',KEYS[i]) ~= ARGV[i] or redis.call('PTTL',KEYS[i]) ~= -1) then return redis.error_reply('conflicting maintenance guard') end
end
for i=1,#KEYS do redis.call('SET',KEYS[i],ARGV[i]) end
return 1
`)

// 广播围栏拦新建角/pin/搬迁入口；在途搬迁收尾与 SETNX 回填仍须外部停止。
// 恢复拒绝 frozen，发布前后复核；失联保持维护，不能把 guard 当自动停写证明。
var mappingInspectScript = redis.NewScript(`
local function permanent(k,v)
 return redis.call('TYPE',k).ok == 'string' and redis.call('GET',k) == v and redis.call('PTTL',k) == -1
end
if not permanent(KEYS[1],ARGV[1]) or not permanent(KEYS[4],'save_barrier_restore:'..ARGV[1]) then return redis.error_reply('permanent route maintenance guard required') end
local absent = redis.call('EXISTS',KEYS[2],KEYS[3],KEYS[5]) == 0
if not (ARGV[5] == '1' and absent) then
 if not permanent(KEYS[2],ARGV[2]) then return redis.error_reply('home route cache changed') end
 if ARGV[3] == '' then
  if redis.call('EXISTS',KEYS[3]) ~= 0 then return redis.error_reply('placement cache must be absent') end
 elseif not permanent(KEYS[3],ARGV[3]) then return redis.error_reply('placement route cache changed') end
 if ARGV[5] == '1' and not permanent(KEYS[5],ARGV[4]) then return redis.error_reply('route revision cache changed') end
end
if ARGV[5] == '1' then
 if redis.call('EXISTS',KEYS[6]) ~= 0 then return redis.error_reply('disarm route ready marker before recovery') end
 if redis.call('TYPE',KEYS[7]).ok ~= 'string' or redis.call('PTTL',KEYS[7]) ~= -1 then return redis.error_reply('permanent route watermark required') end
 return redis.call('GET',KEYS[7])
end
return 'ok'
`)

func (r Restorer) inspectMapping(ctx context.Context, p Plan, row Player) error {
	id, _ := positive(row.ID)
	flag := "0"
	if p.RoutingMode == RoutingModeDBFirst {
		flag = "1"
	}
	keys := []string{MaintenanceKey(p.Zone), placement.HomeZoneKey(id), placement.Key(id), placement.MergeFenceKey(p.Zone), placement.MapRevKey(id), placement.RouteReadyKey, placement.RouteWatermarkKey}
	raw, err := mappingInspectScript.Run(ctx, r.mapping(), keys, p.MaintenanceID, row.HomeZone, row.Placement, row.RouteRevision, flag).Text()
	if err != nil {
		return fmt.Errorf("权威路由维护检查拒绝: %w", err)
	}
	if flag == "1" {
		watermark, err := placement.DecodeRouteWatermark(raw)
		bound, boundErr := positive(row.RouteWatermark)
		if err != nil || boundErr != nil || watermark.Generation != row.RouteGeneration || watermark.WatermarkMs > bound {
			return errors.New("路由 SQL 水位回退或世代与 mapping 不符，须先完成路由反向修复")
		}
	} else if raw != "ok" {
		return errors.New("路由维护回复无效")
	}
	return nil
}
