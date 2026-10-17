package authoritysql

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"os"
	"testing"
	"time"

	"github.com/go-sql-driver/mysql"
)

func TestOpenRejectsWrongDatabaseBeforeConnecting(t *testing.T) {
	if _, err := Open(context.Background(), "root@tcp(127.0.0.1:1)/other", "expected", true, 4); err == nil {
		t.Fatal("不应连接错误库")
	}
}

func TestMigrationRejectsWrongDatabaseBeforeConnecting(t *testing.T) {
	if _, err := OpenForMigration(context.Background(), "root@tcp(127.0.0.1:1)/other", "expected", true); err == nil {
		t.Fatal("迁移不应连接错误库")
	}
}
func TestPhysicalTiDBConnectionsPinStrongSession(t *testing.T) {
	if os.Getenv("MAIL_RANK_TEST_TIDB") != "127.0.0.1:4000" {
		t.Skip("仅显式MAIL_RANK_TEST_TIDB=127.0.0.1:4000启用隔离真实TiDB")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	var random [8]byte
	if _, err := rand.Read(random[:]); err != nil {
		t.Fatal(err)
	}
	database := "mail_rank_pool_" + hex.EncodeToString(random[:])
	t.Logf("本用例隔离库=%s", database)
	// DDL安装/清理的连接与被测业务池分开；业务池仍固定5s socket预算。
	admin, err := sql.Open("mysql", "root@tcp(127.0.0.1:4000)/?timeout=1s&readTimeout=2m&writeTimeout=2m")
	if err != nil {
		t.Fatal(err)
	}
	defer admin.Close()
	if _, err = admin.ExecContext(ctx, "CREATE DATABASE "+database); err != nil {
		t.Fatal(err)
	}
	defer func() {
		clean, stop := context.WithTimeout(context.Background(), 30*time.Second)
		defer stop()
		if _, e := admin.ExecContext(clean, "DROP DATABASE "+database); e != nil {
			t.Errorf("清随机库: %v", e)
		}
	}()
	cfg := mysql.NewConfig()
	cfg.User = "root"
	cfg.Net = "tcp"
	cfg.Addr = "127.0.0.1:4000"
	cfg.DBName = database
	cfg.Params = map[string]string{"tidb_read_consistency": "'weak'", "tidb_snapshot": "'2000-01-01 00:00:00'", "tidb_read_staleness": "-5", "tidb_replica_read": "'follower'", "transaction_isolation": "'REPEATABLE-READ'"}
	db, err := Open(ctx, cfg.FormatDSN(), database, true, 4)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	var held []*sql.Conn
	defer func() {
		for _, conn := range held {
			_ = conn.Close()
		}
	}()
	for i := 0; i < 4; i++ {
		conn, e := db.Conn(ctx)
		if e != nil {
			t.Fatal(e)
		}
		held = append(held, conn)
		var mode, isolation, replica, consistency, snapshot string
		var stale, low int64
		if e = conn.QueryRowContext(ctx, "SELECT @@tidb_txn_mode,@@transaction_isolation,@@tidb_read_staleness,@@tidb_low_resolution_tso,@@tidb_replica_read,@@tidb_read_consistency,@@tidb_snapshot").Scan(&mode, &isolation, &stale, &low, &replica, &consistency, &snapshot); e != nil {
			t.Fatal(e)
		}
		if mode != "pessimistic" || isolation != "READ-COMMITTED" || stale != 0 || low != 0 || replica != "leader" || consistency != "strict" || snapshot != "" {
			t.Fatalf("物理连接%d未固定强会话", i)
		}
	}
}
