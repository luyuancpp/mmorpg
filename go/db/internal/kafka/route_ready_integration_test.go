//go:build integration

package kafka

// 真 Redis 用例(D1 第 3 批,docs/handoff/launch-readiness-20261013/spec_D1_batch1b2_2_3.md §2 M1、§13.2):读脚本在 Redis 7.2
// 上能不能调 INFO(规格 §2 M1「未核实」一条的取证),以及 VerifyRouteReady 对真实实例身份的判定。miniredis 不支持 INFO 的
// server / replication 段,这些只能在这里测。
//
// 跑法(工作目录 go/db):
//
//	GODB_REDIS_ITEST_ADDR=127.0.0.1:6390 GODB_IT_REQUIRE=1 go test -tags=integration -count=1 -v -run 'TestRealRedisRouteReady' ./internal/kafka/
//
// 依赖地址取 GODB_REDIS_ITEST_ADDR(与 TestDurableEnqueue_RealRedis 同一个变量,Redis >= 7.2,必须是主节点);未设 / 连不上时
// Skip 并写明原因;设了 GODB_IT_REQUIRE=1 时这两种情况改为 Fatal —— PASS / FAIL / SKIP 必须可区分,不能把 SKIP 报成通过。
//
// 键:前两个用例只碰随机前缀的键(读脚本的 KEYS[1] 由调用方给,不必是 mapping:route:ready);唯一碰固定键
// mapping:route:ready 的用例(生产读方 + 就绪闸)先要求当前库为空(DBSIZE = 0),否则 Skip(REQUIRE 时 Fatal):
// 在有数据的库上挂一个真身份的就绪标记,会让打开了读方开关的服务信任这份副本。绝不 FLUSHDB,只删自己建的键。

import (
	"context"
	"fmt"
	"os"
	"testing"
	"time"

	"shared/placement"

	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

const (
	// routeReadyITAddrEnv 是真 Redis 的地址(host:port)。
	routeReadyITAddrEnv = "GODB_REDIS_ITEST_ADDR"
	// routeReadyITRequireEnv 为 "1" 时依赖缺失 / 前提不满足一律 Fatal,不 Skip。
	routeReadyITRequireEnv = "GODB_IT_REQUIRE"
	// routeReadyITTimeout 是每个用例的总预算。
	routeReadyITTimeout = 20 * time.Second
)

// routeReadyITSkip 在依赖不满足时 Skip;GODB_IT_REQUIRE=1 时改为 Fatal。
func routeReadyITSkip(t *testing.T, format string, args ...any) {
	t.Helper()
	if os.Getenv(routeReadyITRequireEnv) == "1" {
		t.Fatalf(routeReadyITRequireEnv+"=1, refusing to skip: "+format, args...)
	}
	t.Skipf(format, args...)
}

// routeReadyITClient 连 GODB_REDIS_ITEST_ADDR 并 PING;返回的 ctx 带用例总预算。
func routeReadyITClient(t *testing.T) (*redis.Client, context.Context) {
	t.Helper()
	addr := os.Getenv(routeReadyITAddrEnv)
	if addr == "" {
		routeReadyITSkip(t, "set %s (e.g. 127.0.0.1:6390, a disposable Redis >= 7.2 master) to run", routeReadyITAddrEnv)
	}
	rc := redis.NewClient(&redis.Options{Addr: addr})
	t.Cleanup(func() { _ = rc.Close() })
	ctx, cancel := context.WithTimeout(context.Background(), routeReadyITTimeout)
	t.Cleanup(cancel)
	if err := rc.Ping(ctx).Err(); err != nil {
		routeReadyITSkip(t, "redis at %s=%s is unreachable: %v", routeReadyITAddrEnv, addr, err)
	}
	return rc, ctx
}

// routeReadyITIdentity 在客户端侧读 INFO server / replication,解析出本实例身份(与脚本里取到的对照)。
func routeReadyITIdentity(t *testing.T, ctx context.Context, rc *redis.Client) placement.RedisInstanceIdentity {
	t.Helper()
	srv, err := rc.Info(ctx, "server").Result()
	require.NoError(t, err)
	repl, err := rc.Info(ctx, "replication").Result()
	require.NoError(t, err)
	id, err := placement.ParseRedisInstanceIdentity(srv, repl)
	require.NoError(t, err)
	require.Equal(t, "master", id.Role, "%s must point at a master (the read script is only trusted on a master)", routeReadyITAddrEnv)
	return id
}

// routeReadyITKeys 返回本用例专用的随机前缀键(标记、两把映射键),用例结束时删除。
func routeReadyITKeys(t *testing.T, rc *redis.Client) (ready, k1, k2 string) {
	t.Helper()
	prefix := fmt.Sprintf("godb:itest:routeready:%d:", time.Now().UnixNano())
	ready, k1, k2 = prefix+"ready", prefix+"placement", prefix+"zone"
	t.Cleanup(func() { _ = rc.Del(context.Background(), ready, k1, k2).Err() })
	return ready, k1, k2
}

// routeReadyITRun 直接以给定 KEYS 执行读脚本(KEYS[1] 是标记,其余是映射键)并解析回复。
func routeReadyITRun(t *testing.T, ctx context.Context, rc *redis.Client, keys ...string) placement.RouteReadyRead {
	t.Helper()
	raw, err := routeReadyReadScript.Run(ctx, rc, keys).Result()
	require.NoError(t, err, "the route ready read script must run on Redis >= 7.2 (INFO inside a script, spec §2 M1)")
	read, err := placement.ParseRouteReadyRead(raw, len(keys)-1)
	require.NoError(t, err)
	return read
}

// flipHex 把 40 位十六进制身份的第一个字符换掉(仍是合法格式,但不是这个实例)。
func flipHex(s string) string {
	if s[0] == '0' {
		return "1" + s[1:]
	}
	return "0" + s[1:]
}

// 规格 §2 M1 的取证:脚本里能调 INFO,取回的 run_id / master_replid 是 40 位小写十六进制、role=master,且与客户端侧
// INFO 解析出的身份一致;映射键按 MGET 口径回值(不存在为 nil)。
func TestRealRedisRouteReadyReadScriptCallsInfo(t *testing.T) {
	rc, ctx := routeReadyITClient(t)
	id := routeReadyITIdentity(t, ctx, rc)
	ready, k1, k2 := routeReadyITKeys(t, rc)
	require.NoError(t, rc.Set(ctx, k1, "102:3", 0).Err())

	if srv, err := rc.Info(ctx, "server").Result(); err == nil {
		t.Logf("redis_version line: %q", infoLine(srv, "redis_version"))
	}
	read := routeReadyITRun(t, ctx, rc, ready, k1, k2)
	assert.Len(t, read.Identity.RunID, 40)
	assert.Len(t, read.Identity.MasterReplID, 40)
	assert.Equal(t, "master", read.Identity.Role)
	assert.Equal(t, id, read.Identity, "the script reports the same identity as INFO outside the script")
	_, err := placement.ParseRedisInstanceIdentity("run_id:"+read.Identity.RunID+"\r\n",
		"role:"+read.Identity.Role+"\r\nmaster_replid:"+read.Identity.MasterReplID+"\r\n")
	require.NoError(t, err, "the identity fields are 40 lowercase hex chars")
	assert.False(t, read.MarkerPresent)
	assert.Equal(t, []any{"102:3", nil}, read.Values)
}

// VerifyRouteReady 对真实身份的判定:标记身份等于实例 → Trusted;run_id 或 master_replid 不符 → IdentityMismatch;
// 标记缺席 → Absent;标记不是 string 类型 → Absent(脚本用 pcall 读,不让整段报错),映射键照样回值。
func TestRealRedisRouteReadyVerdicts(t *testing.T) {
	rc, ctx := routeReadyITClient(t)
	id := routeReadyITIdentity(t, ctx, rc)
	ready, k1, k2 := routeReadyITKeys(t, rc)
	require.NoError(t, rc.Set(ctx, k1, "102:3", 0).Err())
	require.NoError(t, rc.Set(ctx, k2, "1", 0).Err())

	verify := func(t *testing.T) placement.RouteReadyRead {
		t.Helper()
		read := routeReadyITRun(t, ctx, rc, ready, k1, k2)
		assert.Equal(t, []any{"102:3", "1"}, read.Values)
		return read
	}
	setMarker := func(t *testing.T, runID, replID string) {
		t.Helper()
		require.NoError(t, rc.Del(ctx, ready).Err())
		require.NoError(t, rc.Set(ctx, ready, routeReadyTestMarker(t, runID, replID), 0).Err())
	}

	t.Run("trusted", func(t *testing.T) {
		setMarker(t, id.RunID, id.MasterReplID)
		verdict, marker := placement.VerifyRouteReady(verify(t))
		assert.Equal(t, placement.RouteReadyTrusted, verdict, "verdict=%s", verdict)
		assert.Equal(t, id.RunID, marker.RedisRunID)
	})
	t.Run("identity_mismatch_run_id", func(t *testing.T) {
		setMarker(t, flipHex(id.RunID), id.MasterReplID)
		verdict, _ := placement.VerifyRouteReady(verify(t))
		assert.Equal(t, placement.RouteReadyIdentityMismatch, verdict, "verdict=%s", verdict)
	})
	t.Run("identity_mismatch_master_replid", func(t *testing.T) {
		setMarker(t, id.RunID, flipHex(id.MasterReplID))
		verdict, _ := placement.VerifyRouteReady(verify(t))
		assert.Equal(t, placement.RouteReadyIdentityMismatch, verdict, "verdict=%s", verdict)
	})
	t.Run("absent", func(t *testing.T) {
		require.NoError(t, rc.Del(ctx, ready).Err())
		read := verify(t)
		assert.False(t, read.MarkerPresent)
		verdict, _ := placement.VerifyRouteReady(read)
		assert.Equal(t, placement.RouteReadyAbsent, verdict, "verdict=%s", verdict)
	})
	t.Run("non_string_marker_is_absent", func(t *testing.T) {
		require.NoError(t, rc.Del(ctx, ready).Err())
		require.NoError(t, rc.RPush(ctx, ready, routeReadyTestMarker(t, id.RunID, id.MasterReplID)).Err())
		read := verify(t)
		assert.False(t, read.MarkerPresent, "a list-typed marker reads as absent instead of failing the script")
		verdict, _ := placement.VerifyRouteReady(read)
		assert.Equal(t, placement.RouteReadyAbsent, verdict, "verdict=%s", verdict)
	})
}

// 生产读方(固定键 mapping:route:ready)与就绪闸在真 Redis 上:启动探针能跑(脚本里调得了 INFO);没有标记时闸关着;
// 挂上本实例身份的标记后闸放行、逐条读按 Trusted 回映射值;撤掉标记后下一次刷新关闸。只在空库上跑。
func TestRealRedisRouteReadyReaderAndGateOnFixedKey(t *testing.T) {
	rc, ctx := routeReadyITClient(t)
	id := routeReadyITIdentity(t, ctx, rc)
	n, err := rc.DBSize(ctx).Result()
	require.NoError(t, err)
	if n != 0 {
		routeReadyITSkip(t, "this test writes the fixed key %s and needs an empty database, found DBSIZE=%d (use a disposable Redis)",
			placement.RouteReadyKey, n)
	}
	_, k1, k2 := routeReadyITKeys(t, rc)
	t.Cleanup(func() { _ = rc.Del(context.Background(), placement.RouteReadyKey).Err() })
	require.NoError(t, rc.Set(ctx, k1, "1000000:4", 0).Err())

	reader := redisRouteReadyReader{rc: rc}
	g := newRouteReadyGate(reader, time.Second)
	require.NoError(t, g.probe(ctx), "the startup probe must run on a real Redis")
	assert.False(t, g.opened(), "no marker: the gate stays closed")

	require.NoError(t, rc.Set(ctx, placement.RouteReadyKey, routeReadyTestMarker(t, id.RunID, id.MasterReplID), 0).Err())
	g.apply(g.read(ctx, time.Second))
	assert.True(t, g.opened(), "a marker carrying this instance's identity opens the gate")

	read, err := reader.readRouteReady(ctx, k1, k2)
	require.NoError(t, err)
	verdict, _ := placement.VerifyRouteReady(read)
	assert.Equal(t, placement.RouteReadyTrusted, verdict, "verdict=%s", verdict)
	assert.Equal(t, []any{"1000000:4", nil}, read.Values)

	require.NoError(t, rc.Del(ctx, placement.RouteReadyKey).Err())
	g.apply(g.read(ctx, time.Second))
	assert.False(t, g.opened(), "removing the marker closes the gate on the next refresh")
}

// infoLine 取 INFO 原文里 field 那一行的值(只给日志用)。
func infoLine(info, field string) string {
	prefix := field + ":"
	for start := 0; start < len(info); {
		end := start
		for end < len(info) && info[end] != '\n' {
			end++
		}
		line := info[start:end]
		if len(line) > 0 && line[len(line)-1] == '\r' {
			line = line[:len(line)-1]
		}
		if len(line) >= len(prefix) && line[:len(prefix)] == prefix {
			return line[len(prefix):]
		}
		start = end + 1
	}
	return ""
}
