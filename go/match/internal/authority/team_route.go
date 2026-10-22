package authority

import (
	"context"
	"database/sql"
	"errors"

	"shared/placement"
)

var ErrTeamRoute = errors.New("team: 全局路由/合服围栏未允许原队伍事务")

// 完整route前缀在全部P之前执行；与Inbox/Guild共用同一64片真源算法。
func (s *Store) lockTeamRoutes(ctx context.Context, tx *sql.Tx, players []uint64) error {
	if err := placement.LockRoleRoutes(ctx, tx, s.roleSchema, players); err != nil {
		if errors.Is(err, placement.ErrRoleRouteFenced) {
			return ErrTeamRoute
		}
		return err
	}
	return nil
}
func (s *Store) CheckTeamRouteContract(ctx context.Context) error {
	if s == nil {
		return ErrTeamRoute
	}
	if err := placement.CheckRoleRouteContract(ctx, s.DB, s.roleSchema); err != nil {
		if errors.Is(err, placement.ErrRoleRouteFenced) {
			return ErrTeamRoute
		}
		return err
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
