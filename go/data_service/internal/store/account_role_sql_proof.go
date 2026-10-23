package store

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/binary"
	"errors"
	"fmt"
	"hash"
	"sort"
	"strconv"

	dbpb "proto/common/database"
	dspb "proto/data_service"
	"shared/accountdirectory"
	"shared/generated/persistschema"
	"shared/placement"
	"shared/savebarrier"
)

// SQLRoleFreezeVerifier 只使用同一个 TiDB 权威池/事务。配置白名单不能改写权威落点，
// 仅限制允许跨 schema 强读的 storage_id；缺权限、缺行、旧围栏一律拒绝。
type SQLRoleFreezeVerifier struct{ allowed map[uint32]bool }

func NewSQLRoleFreezeVerifier(storageIDs []uint32) (*SQLRoleFreezeVerifier, error) {
	if len(storageIDs) == 0 || len(storageIDs) > 1024 {
		return nil, accountdirectory.ErrInvalid
	}
	v := &SQLRoleFreezeVerifier{allowed: make(map[uint32]bool, len(storageIDs))}
	for _, id := range storageIDs {
		if _, ok := placement.StoreDBName(id); !ok || v.allowed[id] {
			return nil, accountdirectory.ErrInvalid
		}
		v.allowed[id] = true
	}
	return v, nil
}

func (v *SQLRoleFreezeVerifier) VerifyFrozen(ctx context.Context, tx *sql.Tx, r *dbpb.AccountRoleTrade) (*dspb.AccountRoleFreezeEvidence, error) {
	if v == nil || tx == nil || r == nil || !accountdirectory.ValidSaveFloor(r.RequiredOwnerEpoch, r.RequiredSaveSeq) {
		return nil, accountdirectory.ErrUnavailable
	}
	holders, err := readRoleTradeHolders(ctx, tx, r.ListingId)
	if err != nil {
		return nil, err
	}
	for _, h := range holders {
		if !h.Drained || h.Seal.PlayerId != r.PlayerId || h.Seal.OwnerRevision != r.SellerOwnerRevision {
			return nil, accountdirectory.ErrConflict
		}
	}
	// 路由锁先于两张 zone 行，阻止读证据期间迁移切库。只认全局 SQL，不回落 Redis。
	var home, storage, version sql.NullInt64
	var frozen sql.NullString
	var rev uint64
	err = tx.QueryRowContext(ctx, "SELECT home_zone,storage_id,placement_version,frozen_run_id,rev FROM player_route WHERE route_shard=? AND player_id=? FOR UPDATE", placement.RouteRowShard(r.PlayerId), r.PlayerId).Scan(&home, &storage, &version, &frozen, &rev)
	if err != nil {
		return nil, fmt.Errorf("角色存盘证据缺权威落点: %w", err)
	}
	if !home.Valid || home.Int64 < 1 || home.Int64 > int64(placement.MaxZoneStorageID) || rev == 0 || frozen.Valid {
		return nil, accountdirectory.ErrConflict
	}
	storageID := uint32(home.Int64)
	if storage.Valid {
		if storage.Int64 < 1 || storage.Int64 > int64(^uint32(0)) || !version.Valid || version.Int64 < 1 {
			return nil, accountdirectory.ErrConflict
		}
		storageID = uint32(storage.Int64)
	} else if version.Valid {
		return nil, accountdirectory.ErrConflict
	}
	if !v.allowed[storageID] {
		return nil, accountdirectory.ErrUnavailable
	}
	schema, ok := placement.StoreDBName(storageID)
	if !ok {
		return nil, accountdirectory.ErrUnavailable
	}
	digest := sha256.New()
	writeProofPart(digest, []byte("account-role-sql-proof-v1"))
	writeProofPart(digest, []byte(schema))
	epoch, seq, gen, err := hashPlayerSQLRow(ctx, tx, schema, "player_database", r.PlayerId, digest)
	if err != nil {
		return nil, err
	}
	epoch1, seq1, gen1, err := hashPlayerSQLRow(ctx, tx, schema, "player_database_1", r.PlayerId, digest)
	if err != nil {
		return nil, err
	}
	if epoch != epoch1 || seq != seq1 || gen != gen1 || saveFloorAfter(r.RequiredOwnerEpoch, r.RequiredSaveSeq, epoch, seq) {
		return nil, accountdirectory.ErrConflict
	}
	kind := dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_SCENE
	if epoch == savebarrier.InitialOwnerEpoch && seq == savebarrier.InitialSaveSeq {
		if r.RequiredOwnerEpoch != 0 || r.RequiredSaveSeq != 0 || len(holders) != 0 {
			return nil, accountdirectory.ErrConflict
		}
		var marker, revision uint64
		if err := tx.QueryRowContext(ctx, "SELECT initial_player_commit,owner_revision FROM account_role WHERE player_id=? FOR UPDATE", r.PlayerId).Scan(&marker, &revision); err != nil {
			return nil, err
		}
		if marker != 1 || revision != 1 || r.SellerOwnerRevision != revision {
			return nil, accountdirectory.ErrConflict
		}
		first, err := initialSQLRowMatches(ctx, tx, schema, "player_database", r.PlayerId, (&dbpb.PlayerDatabase{}).ProtoReflect().Descriptor().Fields())
		if err != nil { return nil, err }
		second, err := initialSQLRowMatches(ctx, tx, schema, "player_database_1", r.PlayerId, (&dbpb.PlayerDatabase_1{}).ProtoReflect().Descriptor().Fields())
		if err != nil { return nil, err }
		if !first || !second { return nil, accountdirectory.ErrConflict }
		kind = dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_INITIAL_COMMIT
	} else if epoch == 0 && seq == 0 {
		// 只有实际未进场种子、没有历史存盘下限、没有本轮原生 holder 才能采用此分支。
		if r.RequiredOwnerEpoch != 0 || r.RequiredSaveSeq != 0 || len(holders) != 0 {
			return nil, accountdirectory.ErrConflict
		}
		kind = dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_NO_ADMISSION
	}
	return &dspb.AccountRoleFreezeEvidence{PlayerId: r.PlayerId, ListingId: r.ListingId, OwnerRevision: r.SellerOwnerRevision, OwnerEpoch: epoch, SaveSeq: seq, PersistedDigest: digest.Sum(nil), Kind: kind}, nil
}

func writeProofPart(h hash.Hash, part []byte) {
	var size [8]byte
	binary.BigEndian.PutUint64(size[:], uint64(len(part)))
	_, _ = h.Write(size[:])
	_, _ = h.Write(part)
}

// 两表均只读一行；摘要保留所有 SQL 列的原字节（包括未知 PB 字段所在 blob），
// 不用解码后重写的旧模型代替原件。按列名排序，避免新老建表列序差异影响摘要。
func hashPlayerSQLRow(ctx context.Context, tx *sql.Tx, schema, table string, player uint64, digest hash.Hash) (uint64, uint64, uint64, error) {
	// schema 仅从 StoreDBName 派生，table 仅为上面两个常量，不接收 caller 标识符。
	rows, err := tx.QueryContext(ctx, "SELECT * FROM `"+schema+"`.`"+table+"` WHERE player_id=? FOR UPDATE", player)
	if err != nil {
		return 0, 0, 0, err
	}
	defer rows.Close()
	columns, err := rows.Columns()
	if err != nil || len(columns) == 0 || len(columns) > 256 {
		return 0, 0, 0, errors.New("玩家 SQL 证据列形状异常")
	}
	if !rows.Next() {
		if err = rows.Err(); err != nil {
			return 0, 0, 0, err
		}
		return 0, 0, 0, accountdirectory.ErrMissing
	}
	values := make([]sql.RawBytes, len(columns))
	dest := make([]any, len(columns))
	for n := range values {
		dest[n] = &values[n]
	}
	if err = rows.Scan(dest...); err != nil {
		return 0, 0, 0, err
	}
	byName := make(map[string][]byte, len(columns))
	total := 0
	for n, name := range columns {
		if _, duplicate := byName[name]; duplicate {
			return 0, 0, 0, errors.New("玩家 SQL 证据重复列")
		}
		byName[name] = values[n]
		total += len(values[n])
	}
	// 证据读取不是批量导出；超过正常玩家行预算显式拒绝，避免持目录锁无限读 blob。
	if total > 16<<20 {
		return 0, 0, 0, errors.New("玩家 SQL 证据单行超过16MiB")
	}
	readUint := func(name string) (uint64, error) {
		raw, ok := byName[name]
		if !ok || len(raw) == 0 {
			return 0, errors.New("玩家 SQL 证据缺版本列")
		}
		return strconv.ParseUint(string(raw), 10, 64)
	}
	id, err := readUint("player_id")
	if err != nil || id != player {
		return 0, 0, 0, errors.New("玩家 SQL 证据身份不符")
	}
	epoch, err := readUint("fence_owner_epoch")
	if err != nil {
		return 0, 0, 0, err
	}
	seq, err := readUint("fence_save_seq")
	if err != nil || !accountdirectory.ValidSaveFloor(epoch, seq) {
		return 0, 0, 0, errors.New("玩家 SQL 证据围栏非法")
	}
	gen, err := readUint("persist_schema_gen")
	if gen == 0 {
		gen = 1 // 与持久覆盖世代的正式协议一致，0 是未盖戳种子。
	}
	if err != nil || gen > uint64(persistschema.Gen) {
		return 0, 0, 0, errors.New("玩家 SQL 证据世代超前或非法")
	}
	writeProofPart(digest, []byte(table))
	sort.Strings(columns)
	for _, name := range columns {
		writeProofPart(digest, []byte(name))
		if byName[name] == nil {
			_, _ = digest.Write([]byte{0})
		} else {
			_, _ = digest.Write([]byte{1})
			writeProofPart(digest, byName[name])
		}
	}
	if rows.Next() {
		return 0, 0, 0, errors.New("玩家 SQL 证据主键重复")
	}
	return epoch, seq, gen, rows.Err()
}
