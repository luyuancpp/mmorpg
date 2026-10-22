package data

import (
	"context"
	"database/sql"
	"errors"
	"regexp"

	"shared/roleidentity"
)

// 配置只在装配期调用；必须和邮件库位于同一TiDB强会话，不从玩家DSN推测全局目录。
func (r *Repo) ConfigureRoleAuthority(ctx context.Context, schema string) error {
	if !regexp.MustCompile(`^[a-z][a-z0-9_]{0,63}$`).MatchString(schema) {
		return errors.New("邮件原会话角色目录schema未配置")
	}
	rows, err := r.db.QueryContext(ctx, "SELECT account,owner_revision,trade_listing_id FROM `"+schema+"`.account_role LIMIT 0")
	if err != nil {
		return err
	}
	if err = rows.Close(); err != nil {
		return err
	}
	r.roleAuthority = schema
	return nil
}

func validateActor(ctx context.Context, player uint64) error {
	if original, present := roleidentity.Original(ctx); present && original.Role.GetPlayerId() != player {
		return ErrDenied
	}
	return nil
}

// 原身份存在时，先锁角色再取邮箱/邮件/操作锁，并持有到业务提交。后台任务不制造客户端actor。
func (r *Repo) lockActor(ctx context.Context, tx *sql.Tx) error {
	original, present := roleidentity.Original(ctx)
	if !present {
		return nil
	}
	if r.roleAuthority == "" {
		return ErrDenied
	}
	var account []byte
	var revision, listing uint64
	if err := tx.QueryRowContext(ctx, "SELECT account,owner_revision,trade_listing_id FROM `"+r.roleAuthority+"`.account_role WHERE player_id=? FOR UPDATE", original.Role.PlayerId).Scan(&account, &revision, &listing); err != nil {
		return err
	}
	if string(account) != original.Account || revision != original.OwnerRevision || listing != 0 {
		return ErrDenied
	}
	return nil
}
