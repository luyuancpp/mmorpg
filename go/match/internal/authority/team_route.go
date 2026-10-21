package authority

import (
	"context"
	"database/sql"
	"errors"
	"sort"

	"shared/placement"
)

var ErrTeamRoute = errors.New("team: 全局路由/合服围栏未允许原队伍事务")

type teamOriginalRoute struct {
	player, revision uint64
	home             uint32
}
type teamFenceKey struct{ zone, shard uint32 }

// Team完整事务前缀：预读完整route集合→全fence中性ODKU/按(zone,shard)锁→
// 全route按聚簇键锁且复核→全P。禁止持P后追加任何fence或新增参与者。
func (s *Store) lockTeamRoutes(ctx context.Context, tx *sql.Tx, players []uint64) error {
	if s.roleSchema == "" || len(players) == 0 {
		return ErrTeamRoute
	}
	prefix := "`" + s.roleSchema + "`."
	var writable uint32
	if err := tx.QueryRowContext(ctx, "SELECT writable FROM "+prefix+"route_meta WHERE id=1").Scan(&writable); err != nil {
		return err
	}
	if writable != 1 {
		return ErrTeamRoute
	}
	routes := make([]teamOriginalRoute, 0, len(players))
	fenceSet := map[teamFenceKey]bool{}
	seen := map[uint64]bool{}
	for _, player := range players {
		if player == 0 {
			return ErrOriginalPlayer
		}
		if seen[player] {
			continue
		}
		seen[player] = true
		var home sql.NullInt64
		var revision uint64
		var frozen sql.NullString
		if err := tx.QueryRowContext(ctx, "SELECT home_zone,rev,frozen_run_id FROM "+prefix+"player_route WHERE route_shard=? AND player_id=?", placement.RouteRowShard(player), player).Scan(&home, &revision, &frozen); err != nil {
			return err
		}
		if !home.Valid || home.Int64 <= 0 || home.Int64 > int64(^uint32(0)) || revision == 0 || frozen.Valid {
			return ErrTeamRoute
		}
		route := teamOriginalRoute{player: player, home: uint32(home.Int64), revision: revision}
		routes = append(routes, route)
		shard, err := placement.RouteFenceShard(player, placement.MaxRouteFenceShards)
		if err != nil {
			return err
		}
		fenceSet[teamFenceKey{zone: route.home, shard: shard}] = true
	}
	fences := make([]teamFenceKey, 0, len(fenceSet))
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
			return ErrTeamRoute
		}
	}
	sort.Slice(routes, func(i, j int) bool {
		left, right := placement.RouteRowShard(routes[i].player), placement.RouteRowShard(routes[j].player)
		if left != right {
			return left < right
		}
		return routes[i].player < routes[j].player
	})
	for _, route := range routes {
		var home sql.NullInt64
		var revision uint64
		var frozen sql.NullString
		if err := tx.QueryRowContext(ctx, "SELECT home_zone,rev,frozen_run_id FROM "+prefix+"player_route WHERE route_shard=? AND player_id=? FOR UPDATE", placement.RouteRowShard(route.player), route.player).Scan(&home, &revision, &frozen); err != nil {
			return err
		}
		if !home.Valid || home.Int64 != int64(route.home) || revision != route.revision || frozen.Valid {
			return ErrTeamRoute
		}
	}
	return nil
}

func (s *Store) CheckTeamRouteContract(ctx context.Context) error {
	if s == nil || s.roleSchema == "" {
		return ErrTeamRoute
	}
	prefix := "`" + s.roleSchema + "`."
	for _, query := range []string{"SELECT home_zone,rev,frozen_run_id FROM " + prefix + "player_route LIMIT 0", "SELECT run_id,expires_at_ms FROM " + prefix + "zone_merge_fence LIMIT 0"} {
		rows, err := s.DB.QueryContext(ctx, query)
		if err != nil {
			return err
		}
		if err = rows.Close(); err != nil {
			return err
		}
	}
	var writable uint32
	if err := s.DB.QueryRowContext(ctx, "SELECT writable FROM "+prefix+"route_meta WHERE id=1").Scan(&writable); err != nil {
		return err
	}
	if writable != 1 {
		return ErrTeamRoute
	}
	return nil
}

// route前缀已锁住该参与者原行；提交不能把RPC预检期间的旧home写进新名册/consent。
func (s *Store) RequireTeamHomeZoneTx(ctx context.Context, tx *sql.Tx, player uint64, expected uint32) error {
	var home sql.NullInt64
	if expected == 0 {
		return ErrTeamRoute
	}
	if err := tx.QueryRowContext(ctx, "SELECT home_zone FROM `"+s.roleSchema+"`.player_route WHERE route_shard=? AND player_id=?", placement.RouteRowShard(player), player).Scan(&home); err != nil {
		return err
	}
	if !home.Valid || home.Int64 != int64(expected) {
		return ErrTeamRoute
	}
	return nil
}
