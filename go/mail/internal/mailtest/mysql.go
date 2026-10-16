// Package mailtest 是 mail 真库集成测试的公共门控与建库(设计 docs/design/mail-system.md §10 ④)。
//
// 只给 _test.go 用:data 与 logic 两个包都有真库用例,测试文件之间不能互相 import,所以放成一个普通包。
// 它不进任何生产代码路径(生产代码 import 它会把 testing 带进二进制,评审时一眼可见)。
//
// 门控(照 friend / trade):
//   - MAIL_TEST_MYSQL_DSN 指向**一次性库**,库名只许 mail_test 或 mail_it_<n>_<n>(白名单:appuser 对别的库也有权限,
//     黑名单挡不住 DSN 指错)。用例会 DROP / 重建 mail 的全部表与 schema_migrations 台账。
//   - MAIL_REQUIRE_MYSQL_TESTS=1 是验收开关:设了却没给 DSN 就判红,而不是整组静默 SKIP(不带 -v 的 go test 对 SKIP 只输出 ok)。
//   - 必须是 MySQL:TiDB 没有间隙锁,发件的锁序用例在它上面恒绿;MariaDB 的 RC / ODKU 语义也不同。连上之后按
//     VERSION() / @@version_comment 判,是 TiDB / MariaDB 就判红(不是跳过 —— 跳过等于假绿)。
package mailtest

import (
	"context"
	"database/sql"
	"fmt"
	"os"
	"regexp"
	"strings"
	"testing"
	"time"

	// 注册 "mysql" 驱动(data 包也注册;重复的空导入无害)。
	_ "github.com/go-sql-driver/mysql"
	"google.golang.org/protobuf/proto"

	dbpb "proto/db"

	"schemamigrate"
)

const (
	// DSNEnv 指向一次性测试库的 DSN 环境变量。
	DSNEnv = "MAIL_TEST_MYSQL_DSN"
	// RequireEnv 设了就是验收模式:缺 DSN 判红。
	RequireEnv = "MAIL_REQUIRE_MYSQL_TESTS"
)

// DBNamePattern:测试只许碰这两类一次性库。
var DBNamePattern = regexp.MustCompile(`^(mail_test|mail_it_\d+_\d+)$`)

// setupTimeout 是建连 + 重建表的上限(schemamigrate 自带锁等待与每条语句超时,这里只防整体挂住)。
const setupTimeout = 60 * time.Second

// RequireDSN 返回测试 DSN;没配时验收模式判红、否则跳过。
func RequireDSN(t testing.TB) string {
	t.Helper()
	dsn := strings.TrimSpace(os.Getenv(DSNEnv))
	if dsn == "" {
		if os.Getenv(RequireEnv) != "" {
			t.Fatalf("%s 已设置(验收模式),但 %s 为空 —— 存储层集成用例会整组静默 SKIP", RequireEnv, DSNEnv)
		}
		t.Skipf("%s 未设置,跳过真实 MySQL 集成用例", DSNEnv)
	}
	return dsn
}

// TableNames 按 proto 的 OptionTableName 取表名(与 schemamigrate 建表用的是同一个来源,不手抄清单)。
func TableNames(tables []proto.Message) ([]string, error) {
	names := make([]string, 0, len(tables))
	for _, m := range tables {
		name, _ := proto.GetExtension(m.ProtoReflect().Descriptor().Options(), dbpb.E_OptionTableName).(string)
		if name == "" {
			return nil, fmt.Errorf("message %s 没有 OptionTableName", m.ProtoReflect().Descriptor().FullName())
		}
		names = append(names, name)
	}
	return names, nil
}

// WithProductionSession 让测试连接池与生产(svc.BuildDSN)同一套会话变量:READ COMMITTED、锁等待 2s、严格模式。
// mailtest 不能 import svc(svc 依赖 etcd / Kafka 等一整套),所以单写一份;两边任何一边改了都要同步。
func WithProductionSession(dsn string) string {
	var extra []string
	if !strings.Contains(dsn, "transaction_isolation=") {
		extra = append(extra, "transaction_isolation=%27READ-COMMITTED%27")
	}
	if !strings.Contains(dsn, "innodb_lock_wait_timeout=") {
		extra = append(extra, "innodb_lock_wait_timeout=2")
	}
	if !strings.Contains(dsn, "sql_mode=") {
		extra = append(extra, "sql_mode=%27STRICT_TRANS_TABLES%27")
	}
	if len(extra) == 0 {
		return dsn
	}
	sep := "?"
	if strings.Contains(dsn, "?") {
		sep = "&"
	}
	return dsn + sep + strings.Join(extra, "&")
}

// OpenMySQL 门控 + 建连 + 拒绝非 MySQL + 库名白名单 + 删表重建(经 schemamigrate.Up,与生产同一条建表路径)。
// tables 是 data.Tables()。返回的 ctx 就是建库用的那个 setupTimeout(60s)ctx,用例接着用它:
// 从建库开始整个用例最多 60 秒 —— 并发 / 屏障类用例万一卡住,也会在这里超时失败,而不是挂到 go test 的全局超时。
func OpenMySQL(t testing.TB, tables []proto.Message) (*sql.DB, context.Context) {
	t.Helper()
	dsn := RequireDSN(t)
	db, err := sql.Open("mysql", WithProductionSession(dsn))
	if err != nil {
		t.Fatalf("open MySQL: %v", err)
	}
	t.Cleanup(func() { _ = db.Close() })
	// 并发用例要 20 条连接同时在事务里;池比并发度小的话,后来的 goroutine 在 database/sql 的队列里排队,压根进不了 InnoDB。
	db.SetMaxOpenConns(32)
	db.SetMaxIdleConns(32)

	ctx, cancel := context.WithTimeout(context.Background(), setupTimeout)
	t.Cleanup(cancel)
	if err := db.PingContext(ctx); err != nil {
		t.Fatalf("ping MySQL: %v", err)
	}

	var version, comment string
	if err := db.QueryRowContext(ctx, "SELECT VERSION(), @@version_comment").Scan(&version, &comment); err != nil {
		t.Fatalf("read server version: %v", err)
	}
	lower := strings.ToLower(version + " " + comment)
	if strings.Contains(lower, "tidb") || strings.Contains(lower, "mariadb") {
		t.Fatalf("%s 指向的是 %q(%s):mail 的锁序用例必须跑在 MySQL 上(TiDB 没有间隙锁,在它上面恒绿)", DSNEnv, version, comment)
	}
	t.Logf("mail integration tests on MySQL %s (%s)", version, comment)

	var dbName sql.NullString
	if err := db.QueryRowContext(ctx, "SELECT DATABASE()").Scan(&dbName); err != nil {
		t.Fatalf("SELECT DATABASE(): %v", err)
	}
	if !dbName.Valid || !DBNamePattern.MatchString(dbName.String) {
		t.Fatalf("测试 DSN 指向库 %q(DSN 里没写库名时为空):只允许 mail_test 或 mail_it_<n>_<n>(防止误删别的服务的表与台账)",
			dbName.String)
	}

	names, err := TableNames(tables)
	if err != nil {
		t.Fatalf("table names: %v", err)
	}
	// 本服务全部表 + schemamigrate 台账:漏了台账,上一轮的台账会让 Up 把"表不在"判成需人工项。
	for _, table := range append(names, schemamigrate.SchemaMigrationsTable) {
		if _, err := db.ExecContext(ctx, "DROP TABLE IF EXISTS `"+table+"`"); err != nil {
			t.Fatalf("drop %s: %v", table, err)
		}
	}
	report, err := schemamigrate.Up(ctx, db, schemamigrate.Options{Database: dbName.String, Tables: tables, Logf: t.Logf})
	if err != nil {
		t.Fatalf("schemamigrate.Up: %v", err)
	}
	if len(report.Manual) > 0 {
		t.Fatalf("新建库不应出现需人工项: %v", report.Manual)
	}
	return db, ctx
}
