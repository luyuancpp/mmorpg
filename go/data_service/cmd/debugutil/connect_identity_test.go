package debugutil

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"io"
	"strings"
	"testing"
)

// r2-ds#1(2026-10-22):TiDB 的实例身份按未知处理;本地调试环境标记读不到就不放行。

func TestMySQLServerIdentity(t *testing.T) {
	notTiDB := scriptedAnswer{err: errors.New("Error 1305 (42000): FUNCTION zone_1_db.tidb_version does not exist")}
	cases := []struct {
		name    string
		answers map[string]scriptedAnswer
		want    string
	}{
		{"MySQL 取 server_uuid", map[string]scriptedAnswer{
			tidbVersionQuery: notTiDB,
			serverUUIDQuery:  {value: "3e11fa47-71ca-11e1-9e33-c80aa9429562"},
		}, "3e11fa47-71ca-11e1-9e33-c80aa9429562"},
		// TiDB 即便答得出 server_uuid,也只能标识单个 tidb-server 进程,同一集群换个 Pod 就变:按未知处理。
		{"TiDB 一律未知", map[string]scriptedAnswer{
			tidbVersionQuery: {value: "Release Version: v8.5.2"},
			serverUUIDQuery:  {value: "per-tidb-server-value"},
		}, ""},
		{"server_uuid 为 NULL", map[string]scriptedAnswer{
			tidbVersionQuery: notTiDB,
			serverUUIDQuery:  {value: nil},
		}, ""},
		{"server_uuid 查询失败", map[string]scriptedAnswer{
			tidbVersionQuery: notTiDB,
			serverUUIDQuery:  {err: errors.New("Error 1193 (HY000): Unknown system variable 'server_uuid'")},
		}, ""},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			if got := MySQLServerIdentity(context.Background(), openScriptedDB(t, c.answers)); got != c.want {
				t.Fatalf("identity = %q, want %q", got, c.want)
			}
		})
	}
}

func TestHasLocalTargetMarker(t *testing.T) {
	cases := []struct {
		name   string
		answer scriptedAnswer
		want   bool
	}{
		{"标记行存在", scriptedAnswer{value: int64(1)}, true},
		{"表在但没有标记行", scriptedAnswer{value: int64(0)}, false},
		{"表不存在", scriptedAnswer{err: errors.New("Error 1146 (42S02): Table 'zone_1_db.debug_import_local_target' doesn't exist")}, false},
		{"无权读取", scriptedAnswer{err: errors.New("Error 1142 (42000): SELECT command denied")}, false},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			db := openScriptedDB(t, map[string]scriptedAnswer{localTargetMarkerQuery: c.answer})
			if got := HasLocalTargetMarker(context.Background(), db); got != c.want {
				t.Fatalf("marker = %v, want %v", got, c.want)
			}
		})
	}
	// 手册与拒绝信息给出的建标记语句,必须建出查询认的那张表、那一行。
	if !strings.Contains(LocalTargetMarkerSetupSQL, "CREATE TABLE IF NOT EXISTS "+LocalTargetMarkerTable) ||
		!strings.Contains(LocalTargetMarkerSetupSQL, "'"+LocalTargetMarkerValue+"'") {
		t.Fatalf("setup SQL does not create the marker the check reads: %s", LocalTargetMarkerSetupSQL)
	}
}

// ── 按查询原文回答的假驱动:只认 answers 里的语句,收到别的语句直接报错 ──

// scriptedAnswer 是对一条查询的固定回答:单行单列的 value;err 非 nil 时查询直接失败。
type scriptedAnswer struct {
	value driver.Value
	err   error
}

func openScriptedDB(t *testing.T, answers map[string]scriptedAnswer) *sql.DB {
	t.Helper()
	db := sql.OpenDB(scriptedConnector{answers: answers})
	t.Cleanup(func() { _ = db.Close() })
	return db
}

type scriptedConnector struct{ answers map[string]scriptedAnswer }

func (c scriptedConnector) Connect(context.Context) (driver.Conn, error) {
	return scriptedConn{answers: c.answers}, nil
}

func (scriptedConnector) Driver() driver.Driver { return scriptedDriver{} }

type scriptedDriver struct{}

func (scriptedDriver) Open(string) (driver.Conn, error) {
	return nil, errors.New("脚本驱动只经 sql.OpenDB(scriptedConnector) 使用")
}

type scriptedConn struct{ answers map[string]scriptedAnswer }

func (c scriptedConn) QueryContext(_ context.Context, query string, _ []driver.NamedValue) (driver.Rows, error) {
	answer, ok := c.answers[query]
	if !ok {
		return nil, fmt.Errorf("unexpected query %q", query)
	}
	if answer.err != nil {
		return nil, answer.err
	}
	return &singleValueRows{value: answer.value}, nil
}

func (scriptedConn) Prepare(query string) (driver.Stmt, error) {
	return nil, fmt.Errorf("unexpected prepare %q", query)
}

func (scriptedConn) Close() error { return nil }

func (scriptedConn) Begin() (driver.Tx, error) { return nil, errors.New("脚本驱动不支持事务") }

type singleValueRows struct {
	value driver.Value
	done  bool
}

func (r *singleValueRows) Columns() []string { return []string{"value"} }

func (r *singleValueRows) Close() error { return nil }

func (r *singleValueRows) Next(dest []driver.Value) error {
	if r.done {
		return io.EOF
	}
	r.done = true
	dest[0] = r.value
	return nil
}
