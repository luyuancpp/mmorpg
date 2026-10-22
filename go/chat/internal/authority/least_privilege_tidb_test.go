package authority

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"errors"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"
	"testing"
	"time"

	"github.com/go-sql-driver/mysql"
	"github.com/stretchr/testify/require"
	"shared/authoritysql"
)

// TestChatTiDBLeastPrivilegeRuntimeContract只使用本例随机库、角色和用户。
// GRANT业务知识唯一来自生产模板；凭据仅存内存，失败信息不输出SQL、DSN或驱动错误正文。
func TestChatTiDBLeastPrivilegeRuntimeContract(t *testing.T) {
	s := nativeStore(t)
	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()
	admin := s.db
	var chatSchema string
	chatPrivilegeNoError(t, "读取本例库名", admin.QueryRowContext(ctx, "SELECT DATABASE()").Scan(&chatSchema))
	require.Regexp(t, `^chat_it_[0-9a-f]{32}$`, chatSchema)
	require.Equal(t, chatSchema, s.roleSchema)

	// nativeStore的原chat库仍由原夹具清理；本例另建global库，真实核验两个GRANT域隔离。
	globalSchema := chatSchema + "_global"
	chatPrivilegeNoError(t, "创建本例全局目录库", chatPrivilegeExec(ctx, admin, "CREATE DATABASE `"+globalSchema+"`"))
	t.Cleanup(func() {
		cleanup, done := context.WithTimeout(context.Background(), 15*time.Second)
		defer done()
		chatPrivilegeNoError(t, "清理本例全局目录库", chatPrivilegeExec(cleanup, admin, "DROP DATABASE `"+globalSchema+"`"))
	})
	globalAdmin := chatPrivilegeOpen(t, ctx, "root", "", globalSchema, true)
	loadFormalRoleFixtures(t, ctx, globalAdmin)

	runtimeRole := chatPrivilegeUUID(t)
	migrationRole := chatPrivilegeUUID(t)
	runtimeUser := chatPrivilegeUUID(t)
	migrationUser := chatPrivilegeUUID(t)
	runtimePassword := chatPrivilegePassword(t)
	migrationPassword := chatPrivilegePassword(t)
	chatPrivilegeApplyTemplate(t, ctx, admin, "chat_migration_role.sql", chatSchema, globalSchema,
		"mmorpg_chat_migrator", migrationRole)
	chatPrivilegeApplyTemplate(t, ctx, admin, "chat_runtime_role.sql", chatSchema, globalSchema,
		"mmorpg_chat_runtime", runtimeRole)
	chatPrivilegeCreateUser(t, ctx, admin, runtimeUser, runtimePassword, runtimeRole)
	chatPrivilegeCreateUser(t, ctx, admin, migrationUser, migrationPassword, migrationRole)

	runtimeDB := chatPrivilegeOpen(t, ctx, runtimeUser, runtimePassword, chatSchema, false)
	migrationDB := chatPrivilegeOpen(t, ctx, migrationUser, migrationPassword, chatSchema, true)
	chatPrivilegeDefaultRole(t, ctx, runtimeDB, runtimeRole)
	chatPrivilegeDefaultRole(t, ctx, migrationDB, migrationRole)
	var initialFences int
	chatPrivilegeNoError(t, "核初始目录围栏为空", globalAdmin.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM zone_merge_fence").Scan(&initialFences))
	require.Zero(t, initialFences)

	limited, err := New(ctx, runtimeDB, globalSchema, s.rules)
	chatPrivilegeNoError(t, "有限业务账号启动权威存储", err)
	originalContext, cancelOriginal := context.WithTimeout(original(t, 11), 90*time.Second)
	defer cancelOriginal()
	in := sendRequest("least-privilege-original", 22)
	first, err := limited.Send(originalContext, in)
	chatPrivilegeNoError(t, "有限业务账号首次发言", err)
	require.Zero(t, first.Tip)
	require.False(t, first.Duplicate)
	replayed, err := limited.Send(originalContext, in)
	chatPrivilegeNoError(t, "有限业务账号重放原UUID", err)
	require.True(t, replayed.Duplicate)
	require.Len(t, pull(t, limited, originalContext, 22), 1)
	require.Equal(t, 1, count(t, limited))
	var currentFences int
	chatPrivilegeNoError(t, "核业务账号中性ODKU创建围栏", globalAdmin.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM zone_merge_fence").Scan(&currentFences))
	require.Greater(t, currentFences, 0)

	// 显式证明SELECT FOR UPDATE权限，而非用管理员的New/Send/Pull替代。
	tx, err := runtimeDB.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	chatPrivilegeNoError(t, "有限业务账号开启锁事务", err)
	defer tx.Rollback() // 断言中止也归还连接；提交后的ErrTxDone无需作为清理失败。
	var revision uint64
	err = tx.QueryRowContext(ctx, "SELECT owner_revision FROM `"+globalSchema+"`.account_role WHERE player_id=11 FOR UPDATE").Scan(&revision)
	if err != nil {
		_ = tx.Rollback()
		chatPrivilegeNoError(t, "有限业务账号持真实角色P锁", err)
	}
	require.EqualValues(t, 7, revision)
	var originalBytes []byte
	err = tx.QueryRowContext(ctx, "SELECT request_payload FROM chat_send_authority WHERE player_id=11 AND request_id=? FOR UPDATE",
		[]byte(in.RequestId)).Scan(&originalBytes)
	if err != nil {
		_ = tx.Rollback()
		chatPrivilegeNoError(t, "有限业务账号锁定原发送收据", err)
	}
	chatPrivilegeNoError(t, "有限业务账号提交只读锁事务", tx.Commit())
	require.NotEmpty(t, originalBytes)

	for _, attempt := range []struct {
		name  string
		query string
		args  []any
	}{
		{"业务账号不能建表", "CREATE TABLE forbidden_runtime_create(id INT)", nil},
		{"业务账号不能改表", "ALTER TABLE chat_send_authority ADD COLUMN forbidden_runtime_alter INT", nil},
		{"业务账号不能删表", "DROP TABLE chat_send_authority", nil},
		{"业务账号不能删除成功原件", "DELETE FROM chat_send_authority WHERE player_id=11 AND request_id=?", []any{[]byte(in.RequestId)}},
		{"业务账号不能更新成功原件", "UPDATE chat_send_authority SET accepted_ms=accepted_ms WHERE player_id=11 AND request_id=?", []any{[]byte(in.RequestId)}},
		{"业务账号不能更新全局归属", "UPDATE `" + globalSchema + "`.account_role SET owner_revision=owner_revision WHERE player_id=11", nil},
	} {
		_, err = runtimeDB.ExecContext(ctx, attempt.query, attempt.args...)
		chatPrivilegeDenied(t, attempt.name, err)
	}
	var afterBytes []byte
	chatPrivilegeNoError(t, "管理员核成功原件仍完整", admin.QueryRowContext(ctx,
		"SELECT request_payload FROM chat_send_authority WHERE player_id=11 AND request_id=?", []byte(in.RequestId)).Scan(&afterBytes))
	require.Equal(t, originalBytes, afterBytes)
	require.Equal(t, 1, count(t, limited))

	// 必须真实缺表后由有限迁移账号创建；IF NOT EXISTS碰到旧表不能冒充CREATE能力。
	chatPrivilegeNoError(t, "管理员仅删除本例聊天两表", chatPrivilegeExec(ctx, admin,
		"DROP TABLE `"+chatSchema+"`.chat_send_authority,`"+chatSchema+"`.chat_policy"))
	var tables int
	chatPrivilegeNoError(t, "核聊天两表确实不存在", admin.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM information_schema.tables WHERE table_schema=? AND table_name IN ('chat_send_authority','chat_policy')", chatSchema).Scan(&tables))
	require.Zero(t, tables)
	chatPrivilegeNoError(t, "有限迁移账号真实创建正式两表", Migrate(ctx, migrationDB))
	chatPrivilegeNoError(t, "核迁移账号创建两表完成", admin.QueryRowContext(ctx,
		"SELECT COUNT(*) FROM information_schema.tables WHERE table_schema=? AND table_name IN ('chat_send_authority','chat_policy')", chatSchema).Scan(&tables))
	require.Equal(t, 2, tables)
	for _, query := range []string{
		"UPDATE `" + globalSchema + "`.account_role SET owner_revision=owner_revision WHERE player_id=11",
		"ALTER TABLE `" + globalSchema + "`.account_role ADD COLUMN forbidden_migration_alter INT",
		"CREATE TABLE `" + globalSchema + "`.forbidden_migration_create(id INT)",
		"INSERT INTO chat_policy(id,policy_hash) VALUES(99,UNHEX(REPEAT('00',32)))",
	} {
		_, err = migrationDB.ExecContext(ctx, query)
		chatPrivilegeDenied(t, "有限迁移账号不得越界DDL或写业务/全局归属", err)
	}

	afterCreate, err := New(ctx, runtimeDB, globalSchema, s.rules)
	chatPrivilegeNoError(t, "迁移后有限业务账号重新启动", err)
	accepted, err := afterCreate.Send(originalContext, sendRequest("least-privilege-after-create", 22))
	chatPrivilegeNoError(t, "真正CREATE后有限业务账号发言", err)
	require.Zero(t, accepted.Tip)
	require.False(t, accepted.Duplicate)
	require.Len(t, pull(t, afterCreate, originalContext, 22), 1)
	chatPrivilegeNoError(t, "核全局归属未被越权修改", globalAdmin.QueryRowContext(ctx,
		"SELECT owner_revision FROM account_role WHERE player_id=11").Scan(&revision))
	require.EqualValues(t, 7, revision)
}

func chatPrivilegeExec(ctx context.Context, db *sql.DB, query string) error {
	_, err := db.ExecContext(ctx, query)
	return err
}

func chatPrivilegeNoError(t *testing.T, phase string, err error) {
	t.Helper()
	if err == nil {
		return
	}
	var mysqlErr *mysql.MySQLError
	var number uint16
	if errors.As(err, &mysqlErr) {
		number = mysqlErr.Number
	}
	t.Fatalf("%s失败（驱动类型=%T，MySQL错误号=%d；不输出SQL、DSN或错误正文）", phase, err, number)
}

func chatPrivilegeDenied(t *testing.T, phase string, err error) {
	t.Helper()
	var mysqlErr *mysql.MySQLError
	if !errors.As(err, &mysqlErr) {
		t.Fatalf("%s未得到MySQL权限错误（驱动类型=%T）", phase, err)
	}
	switch mysqlErr.Number {
	case 1044, 1142, 1143, 1227: // 库/表/列/指定权限拒绝；语法错、缺表、超时和连接错不能冒充权限通过。
	default:
		t.Fatalf("%s错误不是权限拒绝（MySQL错误号=%d）", phase, mysqlErr.Number)
	}
}

func chatPrivilegeUUID(t *testing.T) string {
	t.Helper()
	var value [16]byte
	_, err := rand.Read(value[:])
	chatPrivilegeNoError(t, "生成本例UUID身份", err)
	value[6] = value[6]&0x0f | 0x40
	value[8] = value[8]&0x3f | 0x80
	return hex.EncodeToString(value[:]) // 32字节标识，满足TiDB/MySQL用户和角色名上限。
}

func chatPrivilegePassword(t *testing.T) string {
	t.Helper()
	var value [32]byte
	_, err := rand.Read(value[:])
	chatPrivilegeNoError(t, "生成仅内存随机凭据", err)
	return hex.EncodeToString(value[:])
}

func chatPrivilegeApplyTemplate(t *testing.T, ctx context.Context, admin *sql.DB, name, chatSchema, globalSchema, originalRole, role string) {
	t.Helper()
	require.Regexp(t, `^chat_it_[0-9a-f]{32}$`, chatSchema)
	require.Equal(t, chatSchema+"_global", globalSchema)
	require.Regexp(t, `^[0-9a-f]{32}$`, role)
	require.Contains(t, []string{"chat_runtime_role.sql", "chat_migration_role.sql"}, name)
	expectedRole := "mmorpg_chat_runtime"
	if name == "chat_migration_role.sql" {
		expectedRole = "mmorpg_chat_migrator"
	}
	require.Equal(t, expectedRole, originalRole)
	_, source, _, ok := runtime.Caller(0)
	require.True(t, ok)
	path := filepath.Join(filepath.Dir(source), "..", "..", "..", "..", "deploy", "sql", name)
	bytes, err := os.ReadFile(path)
	chatPrivilegeNoError(t, "读取唯一生产授权模板", err)
	var lines []string
	for _, line := range strings.Split(string(bytes), "\n") {
		line = strings.TrimSpace(line)
		if line != "" && !strings.HasPrefix(line, "--") {
			lines = append(lines, line)
		}
	}
	replacer := strings.NewReplacer("`mmorpg_chat`", "`"+chatSchema+"`", "`mmorpg_global`", "`"+globalSchema+"`", "'"+originalRole+"'", "'"+role+"'")
	created := false
	for _, statement := range strings.Split(strings.Join(lines, "\n"), ";") {
		statement = strings.TrimSpace(statement)
		if statement == "" {
			continue
		}
		require.True(t, strings.HasPrefix(statement, "CREATE ROLE IF NOT EXISTS ") || strings.HasPrefix(statement, "GRANT "), "生产模板新增语句须先显式审阅")
		statement = replacer.Replace(statement)
		require.NotContains(t, statement, "mmorpg_")
		if strings.HasPrefix(statement, "CREATE ROLE IF NOT EXISTS ") {
			require.False(t, created, "一个模板只能创建本例一个角色")
			// 只移除创建幂等：碰撞必须失败，不能接管同名已有角色；全部GRANT仍逐字来自模板。
			statement = strings.Replace(statement, "CREATE ROLE IF NOT EXISTS ", "CREATE ROLE ", 1)
			require.Equal(t, "CREATE ROLE '"+role+"'", statement)
			chatPrivilegeNoError(t, "创建本例随机角色", chatPrivilegeExec(ctx, admin, statement))
			created = true
			t.Cleanup(func() {
				cleanup, done := context.WithTimeout(context.Background(), 10*time.Second)
				defer done()
				chatPrivilegeNoError(t, "清理本例随机角色", chatPrivilegeExec(cleanup, admin, "DROP ROLE '"+role+"'"))
			})
			continue
		}
		require.True(t, created, "GRANT之前必须已成功CREATE本例角色")
		require.True(t, strings.HasSuffix(statement, " TO '"+role+"'"), "模板只可授权本例随机角色")
		require.True(t, strings.Contains(statement, " ON `"+chatSchema+"`.") || strings.Contains(statement, " ON `"+globalSchema+"`."), "模板只可引用本例随机库")
		chatPrivilegeNoError(t, "执行唯一生产模板授权", chatPrivilegeExec(ctx, admin, statement))
	}
	require.True(t, created)
}

func chatPrivilegeCreateUser(t *testing.T, ctx context.Context, admin *sql.DB, user, password, role string) {
	t.Helper()
	require.Regexp(t, `^[0-9a-f]{32}$`, user)
	require.Regexp(t, `^[0-9a-f]{32}$`, role)
	if !regexp.MustCompile(`^[0-9a-f]{64}$`).MatchString(password) {
		t.Fatal("随机凭据形状异常，不输出内容")
	}
	account := "'" + user + "'@'%'"
	chatPrivilegeNoError(t, "创建本例随机用户", chatPrivilegeExec(ctx, admin, "CREATE USER "+account+" IDENTIFIED BY '"+password+"'"))
	t.Cleanup(func() {
		cleanup, done := context.WithTimeout(context.Background(), 10*time.Second)
		defer done()
		chatPrivilegeNoError(t, "清理本例随机用户", chatPrivilegeExec(cleanup, admin, "DROP USER "+account))
	})
	chatPrivilegeNoError(t, "授予本例角色", chatPrivilegeExec(ctx, admin, "GRANT '"+role+"' TO "+account))
	chatPrivilegeNoError(t, "登记本例默认角色", chatPrivilegeExec(ctx, admin, "SET DEFAULT ROLE '"+role+"' TO "+account))
}

func chatPrivilegeOpen(t *testing.T, ctx context.Context, user, password, schema string, migration bool) *sql.DB {
	t.Helper()
	config := mysql.NewConfig()
	config.Net, config.Addr, config.User, config.Passwd, config.DBName = "tcp", "127.0.0.1:24000", user, password, schema
	config.Timeout, config.ReadTimeout, config.WriteTimeout = time.Second, 5*time.Second, 5*time.Second
	var db *sql.DB
	var err error
	if migration {
		db, err = authoritysql.OpenForMigration(ctx, config.FormatDSN(), schema, true)
	} else {
		db, err = authoritysql.Open(ctx, config.FormatDSN(), schema, true, 2)
	}
	chatPrivilegeNoError(t, "打开本例强会话连接池", err)
	t.Cleanup(func() { chatPrivilegeNoError(t, "关闭本例连接池", db.Close()) })
	return db
}

func chatPrivilegeDefaultRole(t *testing.T, ctx context.Context, db *sql.DB, expected string) {
	t.Helper()
	connection, err := db.Conn(ctx)
	chatPrivilegeNoError(t, "取得有限账号物理会话", err)
	defer func() { chatPrivilegeNoError(t, "归还有限账号物理会话", connection.Close()) }()
	_, err = connection.ExecContext(ctx, "SET ROLE DEFAULT")
	chatPrivilegeNoError(t, "激活有限账号默认角色", err)
	var active string
	chatPrivilegeNoError(t, "核验有限账号实际角色", connection.QueryRowContext(ctx, "SELECT CURRENT_ROLE()").Scan(&active))
	require.Contains(t, active, expected)
}
