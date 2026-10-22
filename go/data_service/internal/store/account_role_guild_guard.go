package store

import (
	"context"
	"database/sql"
	"errors"
	"regexp"

	"shared/accountdirectory"
)

var roleGuildSchema = regexp.MustCompile(`^[a-z][a-z0-9_]{0,63}$`)

// ConfigureGuildExit 在装配完成、对外服务前调用。默认必须离帮；schema缺失不降级成已离帮。
// 同一TiDB连接上的跨schema事务与guild的account_role前置点锁构成提交互锁。
func (s *AccountRoleTradeStore) ConfigureGuildExit(required bool, schema string) error {
	if required && !roleGuildSchema.MatchString(schema) {
		return errors.New("角色出售的帮会权威schema未显式配置")
	}
	s.requireGuildExit, s.guildSchema = required, schema
	return nil
}

// 调用点已持有account_role，且不再取得guild主表锁。锁序：account_role→player_state→member。
// guild全部成员新增入口先锁同一account_role，故状态行缺失/成员不存在也不会形成插入窗口。
func (s *AccountRoleTradeStore) verifyGuildExit(ctx context.Context, tx *sql.Tx, player uint64) error {
	if !s.requireGuildExit {
		return nil
	}
	if !roleGuildSchema.MatchString(s.guildSchema) {
		return accountdirectory.ErrUnavailable
	}
	var state uint64
	err := tx.QueryRowContext(ctx, "SELECT player_id FROM `"+s.guildSchema+"`.guild_player_state WHERE player_id=? FOR UPDATE", player).Scan(&state)
	if err != nil && !errors.Is(err, sql.ErrNoRows) {
		return err
	}
	var guild uint64
	err = tx.QueryRowContext(ctx, "SELECT guild_id FROM `"+s.guildSchema+"`.guild_member WHERE player_id=? FOR UPDATE", player).Scan(&guild)
	if errors.Is(err, sql.ErrNoRows) {
		return nil
	}
	if err != nil {
		return err
	}
	return accountdirectory.ErrConflict
}
