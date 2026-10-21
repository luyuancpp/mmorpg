package main

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

	"data_service/cmd/debugutil"
	comp "proto/common/component"
	dbpb "proto/common/database"
	"shared/authoritysql"
	"shared/generated/persistschema"

	"github.com/go-sql-driver/mysql"
	"github.com/luyuancpp/proto2mysql"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
)

func importRowsTiDB(t *testing.T) (*sql.DB, *proto2mysql.DB) {
	t.Helper()
	dsn := os.Getenv("ACCOUNT_DIRECTORY_TEST_DSN")
	if dsn == "" {
		if os.Getenv("ACCOUNT_DIRECTORY_REQUIRE_TIDB") == "1" {
			t.Fatal("缺少本线程专用ACCOUNT_DIRECTORY_TEST_DSN")
		}
		t.Skip("未配置专用导入TiDB")
	}
	cfg, err := mysql.ParseDSN(dsn)
	require.NoError(t, err)
	host, _, err := net.SplitHostPort(cfg.Addr)
	require.NoError(t, err)
	require.True(t, host == "localhost" || (net.ParseIP(host) != nil && net.ParseIP(host).IsLoopback()))
	cfg.DBName = ""
	admin, err := sql.Open("mysql", cfg.FormatDSN())
	require.NoError(t, err)
	var nonce [8]byte
	_, err = rand.Read(nonce[:])
	require.NoError(t, err)
	name := "role_import_it_" + hex.EncodeToString(nonce[:])
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	_, err = admin.ExecContext(ctx, "CREATE DATABASE `"+name+"`")
	require.NoError(t, err)
	t.Cleanup(func() {
		_, err := admin.Exec("DROP DATABASE `" + name + "`")
		require.NoError(t, err)
		require.NoError(t, admin.Close())
	})
	cfg.DBName = name
	db, err := authoritysql.Open(ctx, cfg.FormatDSN(), name, true, 8)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, db.Close()) })
	model := proto2mysql.NewDB()
	require.NoError(t, model.OpenDB(db, name))
	for _, row := range []proto.Message{&dbpb.PlayerDatabase{}, &dbpb.PlayerDatabase_1{}} {
		model.RegisterTable(row)
		require.NoError(t, model.CreateOrUpdateTable(row))
	}
	return db, model
}

func importRowsPlan(balance uint64) *importPlan {
	currency := &comp.CurrencyComp{Values: []uint64{balance}}
	currency.ProtoReflect().SetUnknown(protowire.AppendBytes(protowire.AppendTag(nil, 91, protowire.BytesType), []byte("future-currency")))
	return &importPlan{playerID: 17, seq: 3, rows: []planRow{{table: "player_database", msg: &dbpb.PlayerDatabase{PlayerId: 17, Currency: currency, PersistSchemaGen: persistschema.Gen}}, {table: "player_database_1", msg: &dbpb.PlayerDatabase_1{PlayerId: 17, PersistSchemaGen: persistschema.Gen}}}}
}
func requireImportedRows(t *testing.T, ctx context.Context, db *sql.DB, epoch uint64, expected *importPlan) {
	t.Helper()
	for _, row := range expected.rows {
		want := proto.Clone(row.msg)
		require.NoError(t, debugutil.SetRowFence(want, epoch, expected.seq))
		got, found, err := debugutil.ReadPlayerRow(ctx, db, func() proto.Message { return want.ProtoReflect().Type().New().Interface() }, 17)
		require.NoError(t, err)
		require.True(t, found)
		require.True(t, proto.Equal(want, got), "完整玩家原件与未知PB字段必须保留")
	}
}
func TestTiDBBundleImportFencesRollbackBothRowsAndPreserveUnknownFields(t *testing.T) {
	db, model := importRowsTiDB(t)
	ctx, cancel := context.WithTimeout(context.Background(), 40*time.Second)
	defer cancel()
	original := importRowsPlan(100)
	for _, r := range original.rows {
		row := proto.Clone(r.msg)
		require.NoError(t, debugutil.SetRowFence(row, 20, 3))
		require.NoError(t, model.Insert(row))
	}
	_, err := db.ExecContext(ctx, "UPDATE player_database_1 SET fence_owner_epoch=22 WHERE player_id=17")
	require.NoError(t, err)
	incoming := importRowsPlan(200)
	err = writeLocalRows(ctx, db, incoming, 21)
	require.Error(t, err, "第二表较新必须回滚第一表已做的替换")
	got, found, err := debugutil.ReadPlayerRow(ctx, db, func() proto.Message { return &dbpb.PlayerDatabase{} }, 17)
	require.NoError(t, err)
	require.True(t, found)
	want := proto.Clone(original.rows[0].msg)
	require.NoError(t, debugutil.SetRowFence(want, 20, 3))
	require.True(t, proto.Equal(want, got))
	require.NoError(t, writeLocalRows(ctx, db, incoming, 23))
	requireImportedRows(t, ctx, db, 23, incoming)
	require.Error(t, writeLocalRows(ctx, db, original, 22))
	requireImportedRows(t, ctx, db, 23, incoming)
	_, err = db.ExecContext(ctx, "UPDATE player_database_1 SET persist_schema_gen=4294967295 WHERE player_id=17")
	require.NoError(t, err)
	require.Error(t, writeLocalRows(ctx, db, original, 24), "不删除较新覆盖世代行")
	got, found, err = debugutil.ReadPlayerRow(ctx, db, func() proto.Message { return &dbpb.PlayerDatabase{} }, 17)
	require.NoError(t, err)
	require.True(t, found)
	want = proto.Clone(incoming.rows[0].msg)
	require.NoError(t, debugutil.SetRowFence(want, 23, 3))
	require.True(t, proto.Equal(want, got), "第二表世代拒绝时第一表也回滚")
}

func TestTiDBBundleConcurrentImportsCannotDeleteNewerSQLFence(t *testing.T) {
	db, model := importRowsTiDB(t)
	ctx, cancel := context.WithTimeout(context.Background(), 40*time.Second)
	defer cancel()
	original := importRowsPlan(100)
	for _, r := range original.rows {
		row := proto.Clone(r.msg)
		require.NoError(t, debugutil.SetRowFence(row, 20, 3))
		require.NoError(t, model.Insert(row))
	}
	type result struct {
		epoch uint64
		err   error
	}
	results := make(chan result, 2)
	start := make(chan struct{})
	var wg sync.WaitGroup
	for _, epoch := range []uint64{24, 25} {
		wg.Add(1)
		go func(e uint64) {
			defer wg.Done()
			<-start
			results <- result{e, writeLocalRows(ctx, db, importRowsPlan(e*100), e)}
		}(epoch)
	}
	close(start)
	wg.Wait()
	close(results)
	count := 0
	for r := range results {
		count++
		if r.epoch == 25 {
			require.NoError(t, r.err)
		} else if r.err != nil {
			var duplicate *mysql.MySQLError
			require.True(t, errors.As(r.err, &duplicate))
			require.Equal(t, uint16(1062), duplicate.Number)
		}
	}
	require.Equal(t, 2, count)
	requireImportedRows(t, ctx, db, 25, importRowsPlan(2500))
}
