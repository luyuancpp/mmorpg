package store

import (
	"bytes"
	"context"
	"database/sql"
	"errors"

	dbpb "proto/common/database"
	"shared/accountdirectory"
)

const AccountRoleImportTableName = "account_role_import_receipt"
const accountRoleImportDDL = "CREATE TABLE IF NOT EXISTS account_role_import_receipt (request_id BINARY(32) NOT NULL COMMENT 'pb:1',request_digest BINARY(32) NOT NULL COMMENT 'pb:2',player_id BIGINT UNSIGNED NOT NULL COMMENT 'pb:3',account VARBINARY(191) NOT NULL COMMENT 'pb:4',owner_revision BIGINT UNSIGNED NOT NULL COMMENT 'pb:5',role_proto MEDIUMBLOB NOT NULL COMMENT 'pb:6',allow_restore INT UNSIGNED NOT NULL COMMENT 'pb:7',created_ms BIGINT UNSIGNED NOT NULL COMMENT 'pb:8',PRIMARY KEY(request_id)) ENGINE=InnoDB"

func beginRoleImport(ctx context.Context, tx *sql.Tx, requestID []byte, account string, player uint64, raw []byte, mode roleCreateMode) (*dbpb.AccountRoleImportReceipt, error) {
	if len(requestID) != 32 {
		return nil, accountdirectory.ErrInvalid
	}
	r := &dbpb.AccountRoleImportReceipt{RequestId: requestID, PlayerId: player, Account: account, RoleProto: raw}
	// replace 只在首次提交检查墓碑恢复许可。提交后的原请求恢复不再改目录，
	// 可以从 false 改为 true；它不是导入数据条款，不改变原业务摘要与原修订。
	digest, err := roleTradeDigest(r)
	if err != nil {
		return nil, err
	}
	if mode == roleImportRestore {
		r.AllowRestore = 1
	}
	result, err := tx.ExecContext(ctx, "INSERT INTO account_role_import_receipt(request_id,request_digest,player_id,account,owner_revision,role_proto,allow_restore,created_ms) VALUES (?,?,?,?,0,?,?,"+dbNowMsSQL+") ON DUPLICATE KEY UPDATE request_id=request_id", requestID, digest, player, []byte(account), raw, r.AllowRestore)
	if err != nil {
		return nil, err
	}
	inserted, err := result.RowsAffected()
	if err != nil {
		return nil, err
	}
	if err = tx.QueryRowContext(ctx, "SELECT request_digest,player_id,account,owner_revision,role_proto,allow_restore,created_ms FROM account_role_import_receipt WHERE request_id=? FOR UPDATE", requestID).Scan(&r.RequestDigest, &r.PlayerId, &r.Account, &r.OwnerRevision, &r.RoleProto, &r.AllowRestore, &r.CreatedMs); err != nil {
		return nil, err
	}
	if !bytes.Equal(r.RequestDigest, digest) {
		return nil, accountdirectory.ErrConflict
	}
	if (inserted != 0 && inserted != 1) || (inserted == 0 && r.OwnerRevision == 0) || r.PlayerId != player || r.Account != account || len(r.RoleProto) > 16384 || r.AllowRestore > 1 {
		return nil, errors.New("维护导入收据损坏")
	}
	return r, nil
}
func finishRoleImport(ctx context.Context, tx *sql.Tx, r *dbpb.AccountRoleImportReceipt, revision uint64) error {
	if r == nil {
		return nil // 普通建角没有维护收据。
	}
	result, err := tx.ExecContext(ctx, "UPDATE account_role_import_receipt SET owner_revision=? WHERE request_id=? AND owner_revision=0", revision, r.RequestId)
	if err != nil {
		return err
	}
	n, err := result.RowsAffected()
	if err != nil {
		return err
	}
	if n != 1 {
		return errors.New("维护导入收据未与目录原子提交")
	}
	return nil
}
