package authority

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"go/ast"
	"go/parser"
	"go/token"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
	"testing"
	"time"

	"github.com/go-sql-driver/mysql"
	"github.com/stretchr/testify/require"
	base "proto/common/base"
	dspb "proto/data_service"
	"shared/authoritysql"
	"shared/placement"
	"shared/roleidentity"
)

func fixtureRules() Rules {
	return Rules{MaxContentBytes: 512, RateLimitPerSecond: 5, HistoryMaxEntries: 200, HistoryTTLSeconds: 604800, HistoryDefaultLimit: 20, HistoryMaxLimit: 50, SensitiveWords: []string{"广告"}}
}

func nativeStore(t *testing.T, configured ...Rules) *Store {
	t.Helper()
	address := os.Getenv("CHAT_IT_TIDB")
	if address == "" {
		if os.Getenv("CHAT_IT_REQUIRE_TIDB") == "1" {
			t.Fatal("真实聊天SQL验收缺CHAT_IT_TIDB")
		}
		t.Skip("未启用独立真实TiDB聊天验收")
	}
	require.Equal(t, "127.0.0.1:24000", address, "只消费root已核本任务TiDB，不探测其它服务")
	ctx, cancel := context.WithTimeout(context.Background(), 45*time.Second)
	defer cancel()
	controlConfig := mysql.NewConfig()
	controlConfig.Net = "tcp"
	controlConfig.Addr = address
	controlConfig.User = "root"
	controlConfig.Timeout = time.Second
	controlConfig.ReadTimeout = 5 * time.Second
	controlConfig.WriteTimeout = 5 * time.Second
	connector, err := mysql.NewConnector(controlConfig)
	require.NoError(t, err)
	admin := sql.OpenDB(authoritysql.PinCurrentSession(connector, true))
	admin.SetMaxOpenConns(2)
	require.NoError(t, authoritysql.AssertCurrentSession(ctx, admin, true))
	var random [16]byte
	_, err = rand.Read(random[:])
	require.NoError(t, err)
	name := "chat_it_" + hex.EncodeToString(random[:])
	_, err = admin.ExecContext(ctx, "CREATE DATABASE `"+name+"`")
	require.NoError(t, err)
	// 先登记原成功CREATE的清理；后续Close以LIFO先于DROP，只清本例随机库。
	t.Cleanup(func() {
		cleanup, done := context.WithTimeout(context.Background(), 15*time.Second)
		defer done()
		_, e := admin.ExecContext(cleanup, "DROP DATABASE `"+name+"`")
		require.NoError(t, e)
		require.NoError(t, admin.Close())
	})
	controlConfig.DBName = name
	db, err := authoritysql.OpenForMigration(ctx, controlConfig.FormatDSN(), name, true)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, db.Close()) })
	require.NoError(t, Migrate(ctx, db))
	loadFormalRoleFixtures(t, ctx, db)
	runtimeDB, err := authoritysql.Open(ctx, controlConfig.FormatDSN(), name, true, 8)
	require.NoError(t, err)
	t.Cleanup(func() { require.NoError(t, runtimeDB.Close()) })
	rules := fixtureRules()
	require.LessOrEqual(t, len(configured), 1)
	if len(configured) == 1 {
		rules = configured[0]
	}
	s, err := New(ctx, runtimeDB, name, rules)
	require.NoError(t, err)
	return s
}

// AST读取DS唯一正式nullable路由及account_role DDL，不复制第二份权威表形状。
func loadFormalRoleFixtures(t *testing.T, ctx context.Context, db *sql.DB) {
	t.Helper()
	_, source, _, ok := runtime.Caller(0)
	require.True(t, ok)
	root := filepath.Join(filepath.Dir(source), "..", "..", "..", "data_service", "internal", "store")
	wanted := map[string]bool{"playerRouteBootstrapDDL": true, "zoneMergeFenceBootstrapDDL": true, "zoneMergedIntoBootstrapDDL": true, "routeMetaBootstrapDDL": true, "routeReconcileBootstrapDDL": true, "accountRoleDDL": true}
	values := map[string]string{}
	var literal func(ast.Expr) string
	literal = func(e ast.Expr) string {
		switch v := e.(type) {
		case *ast.BasicLit:
			require.Equal(t, token.STRING, v.Kind)
			s, err := strconv.Unquote(v.Value)
			require.NoError(t, err)
			return s
		case *ast.BinaryExpr:
			require.Equal(t, token.ADD, v.Op)
			return literal(v.X) + literal(v.Y)
		default:
			t.Fatalf("DS正规DDL表达式须显式审阅：%T", e)
			return ""
		}
	}
	for _, name := range []string{"schema.go", "account_directory_store.go"} {
		file, err := parser.ParseFile(token.NewFileSet(), filepath.Join(root, name), nil, parser.SkipObjectResolution)
		require.NoError(t, err)
		for _, decl := range file.Decls {
			group, ok := decl.(*ast.GenDecl)
			if !ok || group.Tok != token.CONST {
				continue
			}
			for _, spec := range group.Specs {
				v, ok := spec.(*ast.ValueSpec)
				if !ok {
					continue
				}
				for i, key := range v.Names {
					if wanted[key.Name] {
						require.Less(t, i, len(v.Values))
						values[key.Name] = literal(v.Values[i])
					}
				}
			}
		}
	}
	for _, name := range []string{"playerRouteBootstrapDDL", "zoneMergeFenceBootstrapDDL", "zoneMergedIntoBootstrapDDL", "routeMetaBootstrapDDL", "routeReconcileBootstrapDDL", "accountRoleDDL"} {
		ddl, ok := values[name]
		require.True(t, ok, "缺正规DDL %s", name)
		_, err := db.ExecContext(ctx, ddl)
		require.NoError(t, err)
	}
	_, err := db.ExecContext(ctx, "INSERT INTO route_meta(id,generation,writable) VALUES(1,'636861742d6974',1)")
	require.NoError(t, err)
	_, err = db.ExecContext(ctx, "INSERT INTO route_reconcile(id) VALUES(1)")
	require.NoError(t, err)
	for _, player := range []uint64{11, 22, ^uint64(0)} {
		_, err = db.ExecContext(ctx, "INSERT INTO player_route(route_shard,player_id,home_zone,rev,updated_by,created_at_ms,updated_at_ms) VALUES(?,?,1,1,'chat-it',1,1)", placement.RouteRowShard(player), player)
		require.NoError(t, err)
		_, err = db.ExecContext(ctx, "INSERT INTO account_role(player_id,account,role_proto,owner_revision,updated_ms,trade_listing_id) VALUES(?,?,?,7,1,0)", player, []byte("original-"+strconv.FormatUint(player, 10)), []byte{})
		require.NoError(t, err)
	}
}

func actor(t *testing.T, id uint64, account string, revision uint64) context.Context {
	t.Helper()
	ctx, err := roleidentity.Capture(context.Background(), &dspb.AccountRoleOwnership{Account: account, OwnerRevision: revision, Role: &base.AccountSimplePlayer{PlayerId: id}})
	require.NoError(t, err)
	return ctx
}
