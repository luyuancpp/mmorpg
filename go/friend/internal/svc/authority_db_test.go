package svc

import (
	"context"
	"database/sql"
	"os"
	"strings"
	"testing"
	"time"

	"friend/internal/config"

	drivermysql "github.com/go-sql-driver/mysql"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestBuildDSN_AuthorityModeAppliesToEveryConnection(t *testing.T) {
	c := config.MySQLConf{Host: "localhost:4000", User: "test", DBName: "test"}
	for _, required := range []bool{false, true} {
		parsed, err := drivermysql.ParseDSN(BuildDSN(c, required))
		require.NoError(t, err)
		assert.Equal(t, "'READ-COMMITTED'", parsed.Params["transaction_isolation"])
		assert.Equal(t, "'STRICT_TRANS_TABLES'", parsed.Params["sql_mode"])
		if required {
			assert.Equal(t, "'pessimistic'", parsed.Params["tidb_txn_mode"])
		} else {
			assert.NotContains(t, parsed.Params, "tidb_txn_mode", "MySQL开发池不能发送TiDB专有参数")
		}
	}
}

func authorityTestConfig(t *testing.T, envName string) config.MySQLConf {
	t.Helper()
	dsn := strings.TrimSpace(os.Getenv(envName))
	if dsn == "" {
		requireName := strings.Replace(envName, "_TEST_", "_REQUIRE_", 1)
		requireName = strings.TrimSuffix(requireName, "_DSN") + "_TESTS"
		if os.Getenv(requireName) != "" {
			t.Fatalf("%s要求真实数据库，但%s未设置", requireName, envName)
		}
		t.Skipf("%s未设置，未执行真实权威池校验", envName)
	}
	parsed, err := drivermysql.ParseDSN(dsn)
	require.NoError(t, err)
	return config.MySQLConf{Host: parsed.Addr, User: parsed.User, Password: parsed.Passwd, DBName: parsed.DBName, MaxOpenConn: 3, MaxIdleConn: 3}
}

func TestAuthorityDB_MySQLDevelopmentAndProductionGate(t *testing.T) {
	c := authorityTestConfig(t, "FRIEND_TEST_MYSQL_DSN")
	db, err := OpenMySQL(c, false)
	require.NoError(t, err)
	t.Cleanup(func() { _ = db.Close() })
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	var isolation string
	require.NoError(t, db.QueryRowContext(ctx, "SELECT @@transaction_isolation").Scan(&isolation))
	assert.Equal(t, "READ-COMMITTED", strings.ToUpper(isolation))
	wrong, err := OpenMySQL(c, true)
	if wrong != nil {
		_ = wrong.Close()
	}
	require.Error(t, err, "生产TiDB模式不能放行真实MySQL")
}

func TestAuthorityDB_TiDBPessimisticOnEveryConnection(t *testing.T) {
	c := authorityTestConfig(t, "FRIEND_TEST_TIDB_DSN")
	db, err := OpenMySQL(c, true)
	require.NoError(t, err)
	t.Cleanup(func() { _ = db.Close() })
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	var conns []*sql.Conn
	t.Cleanup(func() {
		for _, conn := range conns {
			_ = conn.Close()
		}
	})
	for i := 0; i < 3; i++ {
		conn, err := db.Conn(ctx)
		require.NoError(t, err)
		conns = append(conns, conn)
		var mode, isolation string
		require.NoError(t, conn.QueryRowContext(ctx, "SELECT @@tidb_txn_mode,@@transaction_isolation").Scan(&mode, &isolation))
		assert.Equal(t, "pessimistic", mode)
		assert.Equal(t, "READ-COMMITTED", strings.ToUpper(isolation))
	}
}
