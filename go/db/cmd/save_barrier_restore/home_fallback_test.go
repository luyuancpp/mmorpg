package main

// -allow-home-fallback(2026-10-20 修复 r2-mz#1)只在 plan 阶段生效并写进计划;guard / apply 拒收,apply 以审定计划为准。

import (
	"crypto/sha256"
	"db/internal/barrierrestore"
	"encoding/hex"
	"encoding/json"
	"path/filepath"
	"shared/savebarrier"
	"testing"

	"github.com/stretchr/testify/require"
)

func TestAllowHomeFallbackIsAPlanOnlyOption(t *testing.T) {
	plan := options{mode: "plan", schema: "zone_17_db", zone: 17, maintenanceID: "维护", planFile: "plan.json", players: "7", redisAddr: "127.0.0.1:1", routingMode: barrierrestore.RoutingModeRedis, dsnEnv: "仅变量名", redisPasswordEnv: "仅变量名", writersStopped: true, queuesDrained: true, allowHomeFallback: true}
	require.NoError(t, validateOptions(plan))

	apply := plan
	apply.mode, apply.players, apply.planSHA = "apply", "", "审定摘要"
	require.Error(t, validateOptions(apply), "apply 不接受放宽开关:放宽只能取自审定计划")
	apply.allowHomeFallback = false
	require.NoError(t, validateOptions(apply))

	guard := options{mode: "guard", schema: "player_store_1000000_db", zone: 17, maintenanceID: "维护", redisAddr: "127.0.0.1:1", redisDB: 2, mappingAddr: "127.0.0.1:2", routingMode: barrierrestore.RoutingModeDBFirst, routeSchema: "global_routes", dsnEnv: "仅变量名", redisPasswordEnv: "仅变量名", writersStopped: true, queuesDrained: true}
	require.NoError(t, validateOptions(guard))
	guard.allowHomeFallback = true
	require.Error(t, validateOptions(guard), "guard 不接受计划或资产选项")
}

func TestPlanFileCarriesHomeFallback(t *testing.T) {
	digest := sha256.Sum256([]byte("合成资产摘要"))
	rows := [2]string{hex.EncodeToString(digest[:]), hex.EncodeToString(digest[:])}
	p := barrierrestore.Plan{Format: barrierrestore.Format, Schema: "zone_17_db", Zone: 17, MaintenanceID: "维护", RedisAddr: "127.0.0.1:6379", RoutingMode: barrierrestore.RoutingModeRedis, AllowHomeFallback: true, Players: []barrierrestore.Player{{ID: "7", Owner: "9", HomeZone: "17", Placement: "", Original: savebarrier.Version(8, 90), Target: savebarrier.Version(8, 90), Rows: rows}}}
	dir := t.TempDir()
	write := func(name string, p barrierrestore.Plan) (string, string) {
		b, err := json.Marshal(p)
		require.NoError(t, err)
		path := filepath.Join(dir, name)
		require.NoError(t, writeNew(path, b))
		h := sha256.Sum256(b)
		return path, hex.EncodeToString(h[:])
	}

	path, sum := write("with-fallback.json", p)
	got, err := readPlan(path, sum)
	require.NoError(t, err)
	require.True(t, got.AllowHomeFallback)
	require.Equal(t, "", got.Players[0].Placement)

	// 同一份计划去掉开关:摘要随之改变,空落点不再被接受(Validate 拒绝)。
	p.AllowHomeFallback = false
	path2, sum2 := write("without-fallback.json", p)
	require.NotEqual(t, sum, sum2)
	_, err = readPlan(path2, sum2)
	require.Error(t, err)
}
