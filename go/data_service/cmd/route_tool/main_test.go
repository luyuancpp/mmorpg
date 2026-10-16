package main

// route_tool 入口的参数解析与退出码(第 1a 批规格 f 节「cmd/route_tool/main_test.go」)。
// 只钉「失败一律退出码 2、不 panic、不往 stdout 写东西」;import / audit 本身的逻辑在 internal/routetool 的用例里。

import (
	"bytes"
	"context"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"sync"
	"testing"

	"data_service/internal/routetool"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// lockedBuffer 是并发安全的 bytes.Buffer:run 会把 go-zero logx 改写到 stderr,
// go-zero 的后台 goroutine 可能在 run 返回之后仍往里写。
type lockedBuffer struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (b *lockedBuffer) Write(p []byte) (int, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Write(p)
}

func (b *lockedBuffer) String() string {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.String()
}

func runCLI(args ...string) (int, string, string) {
	var stdout, stderr lockedBuffer
	code := run(context.Background(), args, &stdout, &stderr)
	return code, stdout.String(), stderr.String()
}

// TestRouteToolBadArgsExitTwo:缺 -f、缺 / 未知子命令、-batch 越界、子命令不认的参数、多余参数,一律退出码 2 并打印用法。
func TestRouteToolBadArgsExitTwo(t *testing.T) {
	cases := []struct {
		name string
		args []string
		want string
	}{
		{"missing -f", []string{"audit"}, "missing -f"},
		{"missing subcommand", []string{"-f", "x.yaml"}, "missing subcommand"},
		{"unknown subcommand", []string{"-f", "x.yaml", "export"}, "unknown subcommand"},
		{"batch zero", []string{"-f", "x.yaml", "audit", "-batch=0"}, "-batch=0"},
		{"batch over the store limit", []string{"-f", "x.yaml", "import-from-redis", "-batch=1001"}, "-batch=1001"},
		{"negative max-report", []string{"-f", "x.yaml", "import-from-redis", "-max-report=-1"}, "-max-report=-1"},
		{"zero timeout", []string{"-f", "x.yaml", "audit", "-timeout=0s"}, "-timeout=0s"},
		{"dry-run is import only", []string{"-f", "x.yaml", "audit", "-dry-run=false"}, "dry-run"},
		{"recheck flags are audit only", []string{"-f", "x.yaml", "import-from-redis", "-max-recheck=1"}, "max-recheck"},
		{"allow-db-first is import only", []string{"-f", "x.yaml", "audit", "-allow-db-first"}, "allow-db-first"},
		{"extra positional argument", []string{"-f", "x.yaml", "audit", "extra"}, "unexpected arguments"},
		// tidb-prepare(G1a):-zones 必填、逐项校验;它不认 import / audit 的参数,别的子命令也不认它的参数。
		{"tidb-prepare missing zones", []string{"-f", "x.yaml", "tidb-prepare"}, "-zones"},
		{"tidb-prepare zone zero", []string{"-f", "x.yaml", "tidb-prepare", "-zones=0"}, "-zones"},
		{"tidb-prepare zone over the max", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1000000"}, "-zones"},
		{"tidb-prepare zone not a number", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1,a"}, "-zones"},
		{"tidb-prepare empty zone item", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1,"}, "-zones"},
		{"tidb-prepare storage zero", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1", "-storages=0"}, "-storages"},
		{"batch is not a tidb-prepare flag", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1", "-batch=10"}, "batch"},
		{"zones is tidb-prepare only", []string{"-f", "x.yaml", "audit", "-zones=1"}, "zones"},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			code, stdout, stderr := runCLI(c.args...)
			assert.Equal(t, routetool.ExitFailed, code)
			assert.Empty(t, stdout)
			assert.Contains(t, stderr, c.want)
			assert.Contains(t, stderr, "用法:")
		})
	}
}

// TestRouteToolMissingConfigExitsTwo:配置读不到 → 退出码 2(不是 conf.MustLoad 的 log.Fatalf 退出 1)。
func TestRouteToolMissingConfigExitsTwo(t *testing.T) {
	code, stdout, stderr := runCLI("-f", filepath.Join(t.TempDir(), "absent.yaml"), "audit")
	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "absent.yaml")
}

// TestRouteToolUnreachableRedisExitsTwoWithoutPanic:mapping Redis 不可达 → 退出码 2,不 panic、不退出进程
// (MustNewRedis 会经 logx.Must 直接退出),也不去碰全局库。
func TestRouteToolUnreachableRedisExitsTwoWithoutPanic(t *testing.T) {
	addr := closedLocalAddr(t)
	path := filepath.Join(t.TempDir(), "data_service.yaml")
	require.NoError(t, os.WriteFile(path, []byte(fmt.Sprintf(minimalYaml, addr)), 0o600))

	code, stdout, stderr := runCLI("-f", path, "audit")

	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, addr)
	assert.Contains(t, stderr, "unreachable")
}

// TestRouteToolRealImportUnderDBFirstNeedsExplicitFlag:yaml 是 RouteStore.Mode=db_first 时,真导入默认拒绝
// (退出码 2,在连 Redis / 库之前就拦下)—— db_first 下库才是真源,从缓存回填库方向是反的,只有提案 §7 P3 那一次合法。
// -allow-db-first 放行、dry-run 不受影响:这两种都越过这道闸,走到连 Redis(这里地址不可达 → unreachable)。
func TestRouteToolRealImportUnderDBFirstNeedsExplicitFlag(t *testing.T) {
	addr := closedLocalAddr(t)
	path := filepath.Join(t.TempDir(), "data_service.yaml")
	yaml := fmt.Sprintf(minimalYaml, addr) + "RouteStore:\n  Mode: db_first\n"
	require.NoError(t, os.WriteFile(path, []byte(yaml), 0o600))

	t.Run("refused without the flag", func(t *testing.T) {
		code, stdout, stderr := runCLI("-f", path, "import-from-redis", "-dry-run=false")
		assert.Equal(t, routetool.ExitFailed, code)
		assert.Empty(t, stdout)
		assert.Contains(t, stderr, "-allow-db-first")
		assert.NotContains(t, stderr, "unreachable", "必须在连 Redis 之前就拒绝")
	})
	for _, c := range []struct {
		name string
		args []string
	}{
		{"real import with the flag passes the gate", []string{"-f", path, "import-from-redis", "-dry-run=false", "-allow-db-first"}},
		{"dry-run passes the gate", []string{"-f", path, "import-from-redis"}},
		{"audit passes the gate", []string{"-f", path, "audit"}},
	} {
		t.Run(c.name, func(t *testing.T) {
			code, stdout, stderr := runCLI(c.args...)
			assert.Equal(t, routetool.ExitFailed, code)
			assert.Empty(t, stdout)
			assert.Contains(t, stderr, "unreachable", "越过 db_first 闸之后才会去连 Redis")
			assert.NotContains(t, stderr, "a real import is refused")
		})
	}
}

// TestRouteToolTiDBPrepareDoesNotTouchRedis:tidb-prepare 只碰全局库,不连 mapping Redis。两个地址都指向刚关掉的
// 不同端口:失败必须来自全局库(stderr 带 MySQL 的地址),而不是 Redis(不带 Redis 的地址、没有 Redis 的 unreachable 文案)。
// SnapshotMySQL.Host 必须显式写:不写会回落默认 127.0.0.1:3306,本机可能真有 MySQL。
func TestRouteToolTiDBPrepareDoesNotTouchRedis(t *testing.T) {
	// 两个监听同时开着才保证端口不同,拿到地址后一起关掉。
	redisL, err := net.Listen("tcp", "127.0.0.1:0")
	require.NoError(t, err)
	mysqlL, err := net.Listen("tcp", "127.0.0.1:0")
	require.NoError(t, err)
	redisAddr, mysqlAddr := redisL.Addr().String(), mysqlL.Addr().String()
	require.NoError(t, redisL.Close())
	require.NoError(t, mysqlL.Close())
	path := filepath.Join(t.TempDir(), "data_service.yaml")
	yaml := fmt.Sprintf(minimalYaml, redisAddr) + fmt.Sprintf("SnapshotMySQL:\n  Host: %s\n", mysqlAddr)
	require.NoError(t, os.WriteFile(path, []byte(yaml), 0o600))

	code, stdout, stderr := runCLI("-f", path, "tidb-prepare", "-zones=1,2")

	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, mysqlAddr, "失败应来自全局库")
	assert.NotContains(t, stderr, redisAddr, "tidb-prepare 不该去连 mapping Redis")
	assert.NotContains(t, stderr, "unreachable", "那是 mapping Redis 的文案")
}

// 建池失败时 stderr 括号里的提示按原因分流(store.OpenRouteStoreHint,三个建 route store 的入口共用),
// 纯函数用例在 store 包的 TestOpenRouteStoreHintByCause。

// minimalYaml 是能被 conf.Load 接受的最小配置(同 internal/config 的 minimalYaml),MappingRedis.Host 由用例填。
const minimalYaml = `
Name: dataservice.rpc
ListenOn: 127.0.0.1:0
MappingRedis:
  Host: %s
  Type: node
Regions:
  - Id: 1
    Zones: [1]
    Redis:
      Addrs:
        - 127.0.0.1:7001
`

// closedLocalAddr 返回一个刚关掉的本机端口:连它必然被拒绝。
func closedLocalAddr(t *testing.T) string {
	t.Helper()
	l, err := net.Listen("tcp", "127.0.0.1:0")
	require.NoError(t, err)
	addr := l.Addr().String()
	require.NoError(t, l.Close())
	return addr
}
