package main

import (
	"bytes"
	"context"
	"crypto/sha256"
	"db/internal/barrierrestore"
	"encoding/hex"
	"encoding/json"
	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"os"
	"path/filepath"
	"shared/placement"
	"shared/savebarrier"
	"testing"
)

func TestPlanFileIntegrityAndNoOverwrite(t *testing.T) {
	digest := sha256.Sum256([]byte("合成资产摘要"))
	p := barrierrestore.Plan{Format: barrierrestore.Format, Schema: "zone_17_db", Zone: 17, MaintenanceID: "维护", RedisAddr: "127.0.0.1:6379", RoutingMode: barrierrestore.RoutingModeRedis, Players: []barrierrestore.Player{{ID: "7", Owner: "9", HomeZone: "17", Placement: "17:1", Original: savebarrier.Version(8, 90), Target: savebarrier.Version(8, 90), Rows: [2]string{hex.EncodeToString(digest[:]), hex.EncodeToString(digest[:])}}}}
	b, err := json.Marshal(p)
	if err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(t.TempDir(), "plan.json")
	if err := writeNew(path, b); err != nil {
		t.Fatal(err)
	}
	if err := writeNew(path, []byte("覆盖")); err == nil {
		t.Fatal("覆盖既有计划")
	}
	h := sha256.Sum256(b)
	if _, err := readPlan(path, hex.EncodeToString(h[:])); err != nil {
		t.Fatal(err)
	}
	if _, err := readPlan(path, hex.EncodeToString(digest[:])); err == nil {
		t.Fatal("未拒绝计划篡改")
	}
	for _, bad := range [][]byte{append(b, []byte(" {}")...), []byte(`{"format":"unknown"}`)} {
		os.WriteFile(path, bad, 0600)
		h := sha256.Sum256(bad)
		if _, err := readPlan(path, hex.EncodeToString(h[:])); err == nil {
			t.Fatal("未拒绝非法计划")
		}
	}
}

func TestGuardUsesExplicitRedisTargetsAndDoesNotNeedSQL(t *testing.T) {
	cache, mapping := miniredis.RunT(t), miniredis.RunT(t)
	o := options{mode: "guard", schema: "player_store_1000000_db", zone: 17, maintenanceID: "恢复工单", redisAddr: cache.Addr(), redisDB: 2, mappingAddr: mapping.Addr(), routingMode: barrierrestore.RoutingModeDBFirst, routeSchema: "global_routes", dsnEnv: "BARRIER_GUARD_EMPTY_DSN", redisPasswordEnv: "BARRIER_GUARD_EMPTY_CACHE_PASSWORD", mappingPasswordEnv: "BARRIER_GUARD_EMPTY_MAPPING_PASSWORD", writersStopped: true, queuesDrained: true}
	for _, name := range []string{o.dsnEnv, o.redisPasswordEnv, o.mappingPasswordEnv} {
		t.Setenv(name, "")
	}
	var out, errOut bytes.Buffer
	require.Zero(t, run(context.Background(), o, &out, &errOut), errOut.String())
	require.Zero(t, run(context.Background(), o, &out, &errOut), errOut.String())
	rc := redis.NewClient(&redis.Options{Addr: cache.Addr(), DB: 2})
	defer rc.Close()
	value, err := rc.Get(context.Background(), barrierrestore.MaintenanceKey(17)).Result()
	require.NoError(t, err)
	require.Equal(t, "恢复工单", value)
	value, err = mapping.Get(placement.MergeFenceKey(17))
	require.NoError(t, err)
	require.Equal(t, "save_barrier_restore:恢复工单", value)
	require.Empty(t, cache.Keys(), "guard 不得意外写父缓存 DB0")
}

func TestDeploymentOptionsFailClosed(t *testing.T) {
	o := options{mode: "plan", schema: "player_store_1000000_db", zone: 17, maintenanceID: "恢复", planFile: "plan.json", players: "7", redisAddr: "127.0.0.1:1", redisDB: 2, mappingAddr: "127.0.0.1:2", routingMode: barrierrestore.RoutingModeDBFirst, routeSchema: "global_routes", dsnEnv: "DSN_ENV_NAME", redisPasswordEnv: "PASSWORD_ENV_NAME", writersStopped: true, queuesDrained: true}
	o.auditedOwnerUpperBound = "8"
	require.NoError(t, validateOptions(o))
	for _, change := range []func(*options){func(o *options) { o.zone = 4294967313 }, func(o *options) { o.routeSchema = "bad`;DROP" }, func(o *options) { o.mappingDB = 1 }, func(o *options) { o.writersStopped = false }, func(o *options) { o.queuesDrained = false }, func(o *options) { o.redisDB = -1 }, func(o *options) { o.routingMode = "unknown" }} {
		bad := o
		change(&bad)
		require.Error(t, validateOptions(bad))
	}
	for _, floor := range []string{"", "-1", "+8", "08", "18446744073709551616"} {
		bad := o
		bad.auditedOwnerUpperBound = floor
		require.Error(t, validateOptions(bad), "上界必须显式且为标准uint64十进制")
	}
}

func TestApplyRejectsChangedAuditedFloorBeforeConnecting(t *testing.T) {
	digest := sha256.Sum256([]byte("合成资产摘要"))
	p := barrierrestore.Plan{Format: barrierrestore.FormatV2, Schema: "zone_17_db", Zone: 17, MaintenanceID: "维护", RedisAddr: "127.0.0.1:1", MappingAddr: "127.0.0.1:1", RoutingMode: barrierrestore.RoutingModeRedis, AuditedOwnerUpperBound: "18", Players: []barrierrestore.Player{{ID: "7", Owner: "19", HomeZone: "17", Placement: "17:1", Original: savebarrier.Version(8, 90), Target: savebarrier.Version(8, 90), Rows: [2]string{hex.EncodeToString(digest[:]), hex.EncodeToString(digest[:])}}}}
	body, err := json.Marshal(p)
	require.NoError(t, err)
	path := filepath.Join(t.TempDir(), "reviewed-plan.json")
	require.NoError(t, writeNew(path, body))
	hash := sha256.Sum256(body)
	o := options{mode: "apply", schema: p.Schema, zone: uint(p.Zone), maintenanceID: p.MaintenanceID, planFile: path, planSHA: hex.EncodeToString(hash[:]), redisAddr: p.RedisAddr, routingMode: p.RoutingMode, auditedOwnerUpperBound: "17", dsnEnv: "BARRIER_RESTORE_MISSING_DSN", redisPasswordEnv: "BARRIER_RESTORE_MISSING_PASSWORD", writersStopped: true, queuesDrained: true}
	t.Setenv(o.dsnEnv, "")
	t.Setenv(o.redisPasswordEnv, "")
	var out, errOut bytes.Buffer
	require.Equal(t, 2, run(context.Background(), o, &out, &errOut))
	require.Contains(t, errOut.String(), "独立审核旧代上界与审定计划不符")
	require.Empty(t, out.String())
}

func TestApplyRequiresExplicitMaintenanceAndReviewedPlan(t *testing.T) {
	o := options{mode: "apply", schema: "zone_17_db", zone: 17, maintenanceID: "维护", planFile: "plan.json", planSHA: "审定摘要", redisAddr: "127.0.0.1:1", routingMode: barrierrestore.RoutingModeRedis, dsnEnv: "仅变量名", redisPasswordEnv: "仅变量名", writersStopped: true, queuesDrained: true}
	if err := validateOptions(o); err != nil {
		t.Fatal(err)
	}
	for _, change := range []func(*options){func(o *options) { o.writersStopped = false }, func(o *options) { o.queuesDrained = false }, func(o *options) { o.planSHA = "" }, func(o *options) { o.allowLegacy = true }, func(o *options) { o.players = "7" }} {
		bad := o
		change(&bad)
		if err := validateOptions(bad); err == nil {
			t.Fatal("缺少维护前置仍可执行")
		}
	}
}
