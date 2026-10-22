// Package placement提供路由真源与提交前缀，业务域必须在获取角色P之前调用完整集合。
package placement

import (
	"context"
	"database/sql"
	"errors"
	"regexp"
	"sort"
	"strings"
)

var ErrRoleRouteFenced = errors.New("角色全局路由或维护围栏未允许本次事务")
var roleRouteSchemaPattern = regexp.MustCompile(`^[A-Za-z_][A-Za-z0-9_]{0,63}$`)

type roleOriginalRoute struct {
	player, revision uint64
	home             uint32
}
type roleFenceKey struct{ zone, shard uint32 }

// LockRoleRoutes预读完整route集合，再中性ODKU全部原home的pid%64片并按(zone,shard)锁，
// 最后按route聚簇键锁且复核home/rev/frozen。返回后调用者才可取得全P。
// 缺片沿DS正规中性记录懒建，已有active行不改；不先做缺行FOR UPDATE，避免插入锁成环。
// 事务重入只允许同一完整玩家集合；禁止调用者持P之后补新fence或新增参与者。
func LockRoleRoutes(ctx context.Context, tx *sql.Tx, schema string, players []uint64) error {
	if tx == nil || !roleRouteSchemaPattern.MatchString(schema) || len(players) == 0 {
		return ErrRoleRouteFenced
	}
	prefix := "`" + schema + "`."
	var writable uint32
	if err := tx.QueryRowContext(ctx, "SELECT writable FROM "+prefix+"route_meta WHERE id=1").Scan(&writable); err != nil {
		return err
	}
	if writable != 1 {
		return ErrRoleRouteFenced
	}
	routes := make([]roleOriginalRoute, 0, len(players))
	fenceSet := map[roleFenceKey]bool{}
	seen := map[uint64]bool{}
	for _, player := range players {
		if player == 0 {
			return ErrRoleRouteFenced
		}
		if seen[player] {
			continue
		}
		seen[player] = true
		var home sql.NullInt64
		var revision uint64
		var frozen sql.NullString
		if err := tx.QueryRowContext(ctx, "SELECT home_zone,rev,frozen_run_id FROM "+prefix+"player_route WHERE route_shard=? AND player_id=?", RouteRowShard(player), player).Scan(&home, &revision, &frozen); err != nil {
			return err
		}
		if !home.Valid || home.Int64 <= 0 || home.Int64 > int64(^uint32(0)) || revision == 0 || frozen.Valid {
			return ErrRoleRouteFenced
		}
		route := roleOriginalRoute{player: player, home: uint32(home.Int64), revision: revision}
		routes = append(routes, route)
		shard, err := RouteFenceShard(player, MaxRouteFenceShards)
		if err != nil {
			return err
		}
		fenceSet[roleFenceKey{zone: route.home, shard: shard}] = true
	}
	fences := make([]roleFenceKey, 0, len(fenceSet))
	for key := range fenceSet {
		fences = append(fences, key)
	}
	sort.Slice(fences, func(i, j int) bool {
		if fences[i].zone != fences[j].zone {
			return fences[i].zone < fences[j].zone
		}
		return fences[i].shard < fences[j].shard
	})
	// 不先对缺片FOR UPDATE再INSERT；中性ODKU不改任何已有active围栏。
	for _, key := range fences {
		if _, err := tx.ExecContext(ctx, "INSERT INTO "+prefix+"zone_merge_fence(zone_id,shard) VALUES(?,?) ON DUPLICATE KEY UPDATE zone_id=zone_id", key.zone, key.shard); err != nil {
			return err
		}
	}
	for _, key := range fences {
		var active sql.NullBool
		var run sql.NullString
		if err := tx.QueryRowContext(ctx, "SELECT (run_id IS NOT NULL AND expires_at_ms>CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED)),run_id FROM "+prefix+"zone_merge_fence WHERE zone_id=? AND shard=? FOR UPDATE", key.zone, key.shard).Scan(&active, &run); err != nil {
			return err
		}
		if !active.Valid || active.Bool {
			return ErrRoleRouteFenced
		}
	}
	sort.Slice(routes, func(i, j int) bool {
		left, right := RouteRowShard(routes[i].player), RouteRowShard(routes[j].player)
		if left != right {
			return left < right
		}
		return routes[i].player < routes[j].player
	})
	for _, route := range routes {
		var home sql.NullInt64
		var revision uint64
		var frozen sql.NullString
		if err := tx.QueryRowContext(ctx, "SELECT home_zone,rev,frozen_run_id FROM "+prefix+"player_route WHERE route_shard=? AND player_id=? FOR UPDATE", RouteRowShard(route.player), route.player).Scan(&home, &revision, &frozen); err != nil {
			return err
		}
		if !home.Valid || home.Int64 != int64(route.home) || revision != route.revision || frozen.Valid {
			return ErrRoleRouteFenced
		}
	}
	return nil
}

// CheckRoleRouteContract只读核实际使用的正式列、聚簇键顺序与可写开关，不创建或修复其它服务的表。
func CheckRoleRouteContract(ctx context.Context, db *sql.DB, schema string) error {
	if db == nil || !roleRouteSchemaPattern.MatchString(schema) {
		return ErrRoleRouteFenced
	}
	prefix := "`" + schema + "`."
	for _, query := range []string{"SELECT route_shard,player_id,home_zone,rev,frozen_run_id FROM " + prefix + "player_route LIMIT 0", "SELECT zone_id,shard,run_id,expires_at_ms FROM " + prefix + "zone_merge_fence LIMIT 0"} {
		rows, err := db.QueryContext(ctx, query)
		if err != nil {
			return err
		}
		if err = rows.Close(); err != nil {
			return err
		}
	}
	rows, err := db.QueryContext(ctx, "SELECT table_name,column_name FROM information_schema.statistics WHERE table_schema=? AND index_name='PRIMARY' AND table_name IN('player_route','zone_merge_fence','route_meta') ORDER BY table_name,seq_in_index", schema)
	if err != nil {
		return err
	}
	keys := map[string][]string{}
	for rows.Next() {
		var table, column string
		if err = rows.Scan(&table, &column); err != nil {
			rows.Close()
			return err
		}
		keys[table] = append(keys[table], column)
	}
	if err = rows.Err(); err != nil {
		rows.Close()
		return err
	}
	if err = rows.Close(); err != nil {
		return err
	}
	if strings.Join(keys["player_route"], ",") != "route_shard,player_id" || strings.Join(keys["zone_merge_fence"], ",") != "zone_id,shard" || strings.Join(keys["route_meta"], ",") != "id" {
		return ErrRoleRouteFenced
	}
	var writable uint32
	if err := db.QueryRowContext(ctx, "SELECT writable FROM "+prefix+"route_meta WHERE id=1").Scan(&writable); err != nil {
		return err
	}
	if writable != 1 {
		return ErrRoleRouteFenced
	}
	return nil
}
