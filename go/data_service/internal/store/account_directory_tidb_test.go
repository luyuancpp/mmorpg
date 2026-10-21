package store

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"errors"
	"net"
	"os"
	"sync"
	"testing"
	"time"

	"github.com/go-sql-driver/mysql"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	base "proto/common/base"
	dspb "proto/data_service"
	"shared/accountdirectory"
	"shared/authoritysql"
)

// 显式专用 TiDB DSN；每例只建／删自己的随机库，不借用其它任务的现有库或端口假设。
func accountDirectoryTiDB(t *testing.T) (*AccountDirectoryStore, *sql.DB) {
	t.Helper()
	dsn := os.Getenv("ACCOUNT_DIRECTORY_TEST_DSN")
	if dsn == "" {
		if os.Getenv("ACCOUNT_DIRECTORY_REQUIRE_TIDB") == "1" {
			t.Fatal("缺少ACCOUNT_DIRECTORY_TEST_DSN")
		}
		t.Skip("未配置专用账号目录TiDB")
	}
	cfg, err := mysql.ParseDSN(dsn)
	require.NoError(t, err)
	host, _, err := net.SplitHostPort(cfg.Addr)
	require.NoError(t, err)
	require.True(t, host == "localhost" || (net.ParseIP(host) != nil && net.ParseIP(host).IsLoopback()))
	cfg.DBName = ""
	admin, err := sql.Open("mysql", cfg.FormatDSN())
	require.NoError(t, err)
	raw := make([]byte, 8)
	_, err = rand.Read(raw)
	require.NoError(t, err)
	name := "account_directory_it_" + hex.EncodeToString(raw)
	_, err = admin.Exec("CREATE DATABASE `" + name + "`")
	require.NoError(t, err)
	t.Cleanup(func() {
		_, err := admin.Exec("DROP DATABASE `" + name + "`")
		require.NoError(t, err)
		require.NoError(t, admin.Close())
	})
	cfg.DBName = name
	db, err := authoritysql.Open(context.Background(), cfg.FormatDSN(), name, true, 8)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, db.Close()) })
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	conf := MySQLConfig{Host: cfg.Addr, User: cfg.User, Password: cfg.Passwd, DBName: name, MaxOpenConn: 8, MaxIdleConn: 2}
	require.NoError(t, MigrateSchema(ctx, conf, MigrateOptions{}))
	s, err := NewAccountDirectoryStore(conf, true, 2)
	require.NoError(t, err)
	// 此夹具隔离目录原有契约；跨域挂起资产互锁由专门用例装配真实交易表后启用。
	s.contracts = &roleAssetContractGuard{disabled: true}
	t.Cleanup(func() { require.NoError(t, s.Close()) })
	return s, db
}

func TestTiDBAccountDirectoryCapReplayRepairAndRemoval(t *testing.T) {
	s, db := accountDirectoryTiDB(t)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	_, err := s.Read(ctx, "Account", true)
	require.NoError(t, err)
	type result struct {
		id  uint64
		err error
	}
	results := make(chan result, 8)
	var workers sync.WaitGroup
	for i := uint64(1); i <= 8; i++ {
		workers.Add(1)
		go func(id uint64) {
			defer workers.Done()
			_, err := s.Create(ctx, "Account", &base.AccountSimplePlayer{PlayerId: id})
			results <- result{id, err}
		}(i)
	}
	workers.Wait()
	close(results)
	created := []uint64{}
	full := 0
	for r := range results {
		if r.err == nil {
			created = append(created, r.id)
		} else {
			require.ErrorIs(t, r.err, accountdirectory.ErrFull)
			full++
		}
	}
	require.Len(t, created, 2)
	require.Equal(t, 6, full)
	roles, err := s.Read(ctx, "Account", false)
	require.NoError(t, err)
	require.Len(t, roles.Players, 2)
	id := created[0]
	same := &base.AccountSimplePlayer{PlayerId: id}
	_, err = s.Create(ctx, "Account", same)
	require.NoError(t, err, "满位不拒绝已提交的原角色重放")
	_, err = s.Create(ctx, "account", same)
	require.ErrorIs(t, err, accountdirectory.ErrConflict)
	owned, err := s.Lookup(ctx, id)
	require.NoError(t, err)
	require.EqualValues(t, 1, owned.OwnerRevision)
	// SQL角色原件带未来字段；通过真实Repair接口补齐后仍须逐字节保留。
	unknown := protowire.AppendVarint(protowire.AppendTag(nil, 99, protowire.VarintType), 7)
	same.ProtoReflect().SetUnknown(unknown)
	raw, err := proto.Marshal(same)
	require.NoError(t, err)
	_, err = db.ExecContext(ctx, "UPDATE account_role SET role_proto=? WHERE player_id=?", raw, id)
	require.NoError(t, err)
	_, err = s.Repair(ctx, "Account", &base.AccountSimplePlayer{PlayerId: id, ClassId: 3, AppearanceId: "04_mountain_guardian_boy"}, 1)
	require.NoError(t, err)
	owned, err = s.Lookup(ctx, id)
	require.NoError(t, err)
	require.Equal(t, unknown, owned.Role.ProtoReflect().GetUnknown())
	require.EqualValues(t, 3, owned.Role.ClassId)
	_, err = s.Repair(ctx, "Account", &base.AccountSimplePlayer{PlayerId: id, ClassId: 4}, 1)
	require.ErrorIs(t, err, accountdirectory.ErrConflict)
	_, err = db.ExecContext(ctx, "UPDATE account_role SET owner_revision=2 WHERE player_id=?", id)
	require.NoError(t, err)
	_, err = s.Repair(ctx, "Account", &base.AccountSimplePlayer{PlayerId: id, Name: "晚到旧链"}, 1)
	require.ErrorIs(t, err, accountdirectory.ErrConflict)
	require.ErrorIs(t, s.Remove(ctx, "Account", []*dspb.AccountRoleVersion{{PlayerId: id, OwnerRevision: 1}}), accountdirectory.ErrConflict)
	_, err = s.Lookup(ctx, id)
	require.NoError(t, err, "旧修订删除不得动新归属")
	require.NoError(t, s.Remove(ctx, "Account", []*dspb.AccountRoleVersion{{PlayerId: id, OwnerRevision: 2}}))
	require.NoError(t, s.Remove(ctx, "Account", []*dspb.AccountRoleVersion{{PlayerId: id, OwnerRevision: 2}}), "相同删除幂等")
	_, err = s.Lookup(ctx, id)
	require.ErrorIs(t, err, accountdirectory.ErrMissing)
	_, err = s.Create(ctx, "Account", &base.AccountSimplePlayer{PlayerId: id})
	require.ErrorIs(t, err, accountdirectory.ErrConflict, "重放旧建角不能复活已删除目录")
	_, err = s.ImportRole(ctx, "Account", &base.AccountSimplePlayer{PlayerId: id}, true, roleImportTestID(1))
	require.NoError(t, err)
	restored, err := s.Lookup(ctx, id)
	require.NoError(t, err)
	require.Greater(t, restored.OwnerRevision, uint64(2))
	_, err = s.Repair(ctx, "Account", &base.AccountSimplePlayer{PlayerId: id, Name: "删除前旧链"}, 1)
	require.ErrorIs(t, err, accountdirectory.ErrConflict)
	canceled, stop := context.WithCancel(ctx)
	stop()
	_, err = s.Create(canceled, "Account", &base.AccountSimplePlayer{PlayerId: 99})
	require.True(t, errors.Is(err, context.Canceled))
	_, err = s.Lookup(ctx, 99)
	require.ErrorIs(t, err, accountdirectory.ErrMissing)
}

func TestTiDBAccountDirectoryPreservesExactAccountIdentity(t *testing.T) {
	s, _ := accountDirectoryTiDB(t)
	ctx := context.Background()
	for i, a := range []string{"Case", "case", " Case"} {
		_, err := s.Create(ctx, a, &base.AccountSimplePlayer{PlayerId: uint64(100 + i)})
		require.NoError(t, err)
	}
	for i, a := range []string{"Case", "case", " Case"} {
		roles, err := s.Read(ctx, a, false)
		require.NoError(t, err)
		require.Len(t, roles.Players, 1)
		require.EqualValues(t, 100+i, roles.Players[0].PlayerId)
	}
	for i := 0; i < 4; i++ {
		conn, err := s.db.Conn(ctx)
		require.NoError(t, err)
		var isolation, mode, replica string
		require.NoError(t, conn.QueryRowContext(ctx, "SELECT @@transaction_isolation,@@tidb_txn_mode,@@tidb_replica_read").Scan(&isolation, &mode, &replica))
		require.Equal(t, "READ-COMMITTED", isolation)
		require.Equal(t, "pessimistic", mode)
		require.Equal(t, "leader", replica)
		defer conn.Close()
	}
}
