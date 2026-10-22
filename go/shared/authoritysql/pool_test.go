package authoritysql

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"errors"
	"net"
	"os"
	"strconv"
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
	address := os.Getenv("MAIL_RANK_TEST_TIDB")
	if address == "" {
		if os.Getenv("MAIL_RANK_REQUIRE_TESTS") == "1" {
			t.Fatal("MAIL_RANK_TEST_TIDB必须显式配置本任务独占TiDB")
		}
		t.Skip("需显式本任务数值loopback TiDB端点，随机自有库")
	}
	if err := physicalTiDBFixtureEndpoint(address); err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	var random [8]byte
	if _, err := rand.Read(random[:]); err != nil {
		t.Fatal(err)
	}
	database := "mail_rank_pool_" + hex.EncodeToString(random[:])
	t.Logf("本用例隔离库=%s", database)
	// 随机库DDL前先只读系统库，确认真实TiDB及强会话；业务池仍固定5s socket预算。
	admin, err := OpenForMigration(ctx, "root@tcp("+address+")/mysql", "mysql", true)
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
	cfg.Addr = address
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

func physicalTiDBFixtureEndpoint(address string) error {
	host, port, err := net.SplitHostPort(address)
	if err != nil {
		return errors.New("物理连接夹具必须显式数值loopback端口")
	}
	ip := net.ParseIP(host)
	number, err := strconv.Atoi(port)
	if ip == nil || !ip.IsLoopback() || err != nil || number < 1 || number > 65535 {
		return errors.New("物理连接夹具仅接受数值loopback与合法非零端口")
	}
	return nil
}

func TestPhysicalTiDBFixtureEndpointRejectsForeignOrInvalidPort(t *testing.T) {
	for _, address := range []string{"127.0.0.1:24000", "[::1]:24000"} {
		if err := physicalTiDBFixtureEndpoint(address); err != nil {
			t.Fatal(err)
		}
	}
	for _, address := range []string{"localhost:24000", "10.0.0.1:24000", "127.0.0.1:0", "127.0.0.1:65536", "127.0.0.1:mysql", "127.0.0.1"} {
		if physicalTiDBFixtureEndpoint(address) == nil {
			t.Fatal("非法夹具端点未拒绝")
		}
	}
}
