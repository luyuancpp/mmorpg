//go:build integration

package store_test

// 第 3 批读方(route_reads.go 的 RD1–RD4)在真实 InnoDB 上的语义。假驱动(route_reads_contract_test.go)证不了这几条:
//  1. RD1 的 LEFT JOIN:没有围栏片行 / run_id 为 NULL / 已过期都读成「未封锁」,生效读成「封锁」,判据是库时钟;
//     只读 63 号片(只在 0 号片生效的围栏读不到 —— 持有者写满 64 片的契约由第 2 批的写者守住);
//  2. RD2 / RD3 沿 (route_shard, player_id) 键集翻页无漏无重、不混入别的 zone / 落点,计划能用上 idx_route_home / idx_route_storage;
//  3. RD4 没有生效围栏时也回库时钟,有时只列 63 号片上生效的行。
//
// 跑法同 route_store_integration_test.go(本地 docker 的 appuser 没有建库权限,用 root):
//   DATA_SERVICE_IT_MYSQL_USER=root DATA_SERVICE_IT_MYSQL_PASSWORD=<deploy/docker-compose.yml> \
//     go test -tags=integration ./internal/store/... -run "RouteReads" -count=1 -v
// 加 DATA_SERVICE_IT_REQUIRE=1:依赖不可达(MySQL 连不上、没有建库权限)从 SKIP 变成 FAIL,杜绝把 SKIP 报成通过。
// 前提同 route_store_integration_test.go:建 route store 要求 MySQL ≥ 8.0.16(探针 P4;更老是 FAIL 不是 SKIP)。

import (
	"context"
	"os"
	"sort"
	"strconv"
	"strings"
	"testing"

	"data_service/internal/store"
	"data_service/internal/store/storetest"

	"shared/placement"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// envReadsITRequire 设为 1 时,本文件的用例被跳过即判失败(PASS / FAIL / SKIP 必须可区分)。
const envReadsITRequire = "DATA_SERVICE_IT_REQUIRE"

// requireReadsIT 必须是每个用例的第一句:它登记的清理最后一个执行(在 storetest 删库之后),
// 发现用例被跳过(依赖不可达)且设了 DATA_SERVICE_IT_REQUIRE=1 就判失败。
// 用清理而不是自己再探测一遍依赖:跳过的判定只在 storetest 一处,这里不复制第二份。
func requireReadsIT(t *testing.T) {
	t.Helper()
	if os.Getenv(envReadsITRequire) != "1" {
		return
	}
	t.Cleanup(func() {
		if t.Skipped() {
			t.Errorf("%s=1: this integration test was skipped (a dependency is missing or unusable, see the SKIP reason above); treated as FAIL",
				envReadsITRequire)
		}
	})
}

// seedReadFence 绕过任何 store 直接造一行围栏分片(第 2 批之前库里没有围栏写者)。runID 为空 = 未封锁的空行;
// 否则 expires_at_ms = 库时钟 + offsetMs(负数 = 已过期),holder_json 带 fence_epoch。
func seedReadFence(t *testing.T, db *storetest.DB, zone, shard uint32, runID string, offsetMs int64, epoch uint64) {
	t.Helper()
	if runID == "" {
		_, err := db.Raw.Exec("INSERT INTO zone_merge_fence (zone_id, shard) VALUES (?, ?)", zone, shard)
		require.NoError(t, err)
		return
	}
	holder := `{"run_id":"` + runID + `","fence_epoch":` + strconv.FormatUint(epoch, 10) + `}`
	_, err := db.Raw.Exec(
		"INSERT INTO zone_merge_fence (zone_id, shard, run_id, purpose, src_zone, dst_zone, expires_at_ms, holder_json, fence_epoch)"+
			" VALUES (?, ?, ?, 'merge', ?, ?, "+store.RouteDBNowMsSQLForTest+" + ?, ?, ?)",
		zone, shard, runID, zone, zone+100, offsetMs, holder, epoch)
	require.NoError(t, err)
}

// readDBNowMs 读一次库时钟(与 store 同一个表达式),用来夹住 RD4 回的 now_ms。
func readDBNowMs(t *testing.T, db *storetest.DB) uint64 {
	t.Helper()
	var ms uint64
	require.NoError(t, db.Raw.QueryRow("SELECT "+store.RouteDBNowMsSQLForTest).Scan(&ms))
	return ms
}

// TestRouteReads_GetHomeAndFence_RealDB:RD1 在真库上的每一种情形。
func TestRouteReads_GetHomeAndFence_RealDB(t *testing.T) {
	requireReadsIT(t)
	db := storetest.NewMigratedDB(t)
	st := newRouteStore(t, db)
	ctx := context.Background()
	const run = "42-43-20261018T000000Z-0a1b2c3d"
	readShard := uint32(store.RouteFenceReadShardForTest)

	// 7101:zone 41 没有任何围栏行。
	seedRouteRow(t, db, 7101, 41, 0, 0, "", 1)
	// 7102:zone 42 的 63 号片生效。
	seedRouteRow(t, db, 7102, 42, 0, 0, "", 1)
	seedReadFence(t, db, 42, readShard, run, 3_600_000, 1)
	// 7103:zone 43 的 63 号片已过期(判据是库时钟)。
	seedRouteRow(t, db, 7103, 43, 0, 0, "", 1)
	seedReadFence(t, db, 43, readShard, run, -3_600_000, 1)
	// 7104:只有落点、没有 home。
	seedRouteRow(t, db, 7104, 0, placement.DefaultGlobalStorageID, 1, "", 2)
	// 7105:没有这一行。
	// 7106:zone 44 只在 0 号片生效(读方只读 63 号片;持有者写满 64 片的契约由第 2 批的写者守住)。
	seedRouteRow(t, db, 7106, 44, 0, 0, "", 1)
	seedReadFence(t, db, 44, 0, run, 3_600_000, 1)
	// 7107:zone 45 的 63 号片是未封锁的空行(W1 懒建的形状)。
	seedRouteRow(t, db, 7107, 45, 0, 0, "", 1)
	seedReadFence(t, db, 45, readShard, "", 0, 0)

	for _, tc := range []struct {
		player uint64
		want   store.RouteHomeFence
	}{
		{7101, store.RouteHomeFence{HomeZone: 41}},
		{7102, store.RouteHomeFence{HomeZone: 42, Fenced: true}},
		{7103, store.RouteHomeFence{HomeZone: 43}},
		{7104, store.RouteHomeFence{}},
		{7105, store.RouteHomeFence{}},
		{7106, store.RouteHomeFence{HomeZone: 44}},
		{7107, store.RouteHomeFence{HomeZone: 45}},
	} {
		got, err := st.GetHomeAndFence(ctx, tc.player)
		require.NoError(t, err, "player %d", tc.player)
		assert.Equal(t, tc.want, got, "player %d", tc.player)
	}

	// 库不可用是错误,不是缺席(路由层据此回 Unavailable)。另开一个 store 关掉,免得影响上面的 cleanup。
	broken, err := store.NewRouteStore(db.Cfg, routeTestOpts())
	require.NoError(t, err)
	require.NoError(t, broken.Close())
	_, err = broken.GetHomeAndFence(ctx, 7101)
	require.Error(t, err, "a closed pool must surface as an error, never as absence")

	// 计划取证(MySQL):按聚簇键点查 player_route。原样留档;TiDB 的计划形状不同,只记不判。
	stmt := inlineRouteArgs(t, store.RouteReadHomeFenceSQLForTest,
		[]uint64{uint64(readShard), uint64(placement.RouteRowShard(7102)), 7102})
	logExplainUnlessTiDB(t, db, "RD1", stmt)
}

// TestRouteReads_ScanRoutesByHome_RealDB:RD2 沿键集翻页覆盖本 zone 全部玩家(跨多个行片)、不重不漏、不混入别的 zone,
// 且按聚簇键严格升序;计划的候选索引里有 idx_route_home。
func TestRouteReads_ScanRoutesByHome_RealDB(t *testing.T) {
	requireReadsIT(t)
	db := storetest.NewMigratedDB(t)
	st := newRouteStore(t, db)
	ctx := context.Background()
	const zone, other uint32 = 51, 52

	want := map[uint64]bool{}
	for i := uint64(0); i < 40; i++ {
		player := 200001 + i*3 // 步长 3:40 个号散到 40 个不同的行片上
		seedRouteRow(t, db, player, zone, 0, 0, "", 1)
		want[player] = true
	}
	for i := uint64(0); i < 10; i++ {
		seedRouteRow(t, db, 300001+i, other, 0, 0, "", 1)
	}
	seedRouteRow(t, db, 300100, 0, placement.DefaultGlobalStorageID, 1, "", 1) // home 为 NULL:不属于任何 zone

	const page = 7
	var cursor store.RouteScanCursor
	var got []store.RouteRow
	for pages := 0; ; pages++ {
		require.Less(t, pages, 20, "pagination does not terminate")
		rows, err := st.ScanRoutesByHome(ctx, zone, cursor, page)
		require.NoError(t, err)
		got = append(got, rows...)
		if len(rows) < page {
			break
		}
		cursor = store.RouteScanCursorAt(rows[len(rows)-1])
	}
	require.Len(t, got, len(want))
	for i, row := range got {
		assert.True(t, want[row.PlayerID], "player %d is not in zone %d", row.PlayerID, zone)
		assert.Equal(t, zone, row.HomeZone)
		if i > 0 {
			assert.True(t, store.RouteScanCursorAt(got[i-1]).Less(store.RouteScanCursorAt(row)), "keyset order broken at %d", i)
		}
	}
	shards := map[uint32]bool{}
	for _, row := range got {
		shards[placement.RouteRowShard(row.PlayerID)] = true
	}
	assert.Greater(t, len(shards), page, "the fixture must span more shards than one page, or cross-shard paging is untested")

	// 计划取证:候选索引里必须有 idx_route_home(由谓词决定,与表大小无关);实际选中的 key / Extra 原样留档。
	stmt := inlineRouteArgs(t, store.RouteScanByHomeSQLForTest, []uint64{uint64(zone), 3, 3, 200010, page})
	if plan, ok := explainUnlessTiDB(t, db, stmt); ok {
		t.Logf("RD2 plan: %v", plan)
		assert.Contains(t, plan["possible_keys"], "idx_route_home", "RD2 must be able to use idx_route_home")
	}
}

// TestRouteReads_ScanRoutesByStorage_RealDB:RD3 只列 storage_id 等于参数的行,翻页无漏无重。
func TestRouteReads_ScanRoutesByStorage_RealDB(t *testing.T) {
	requireReadsIT(t)
	db := storetest.NewMigratedDB(t)
	st := newRouteStore(t, db)
	ctx := context.Background()
	storage := placement.DefaultGlobalStorageID

	var want []uint64
	for i := uint64(0); i < 5; i++ {
		player := 400001 + i*7
		seedRouteRow(t, db, player, uint32(61+i), storage, 1, "", 1) // 跨 zone,同一个落点
		want = append(want, player)
	}
	// 两个干扰项:没有落点的、另一个落点的。
	seedRouteRow(t, db, 400100, 61, 0, 0, "", 1)
	seedRouteRow(t, db, 400101, 61, storage+1, 1, "", 1)

	var got []uint64
	var cursor store.RouteScanCursor
	for pages := 0; ; pages++ {
		require.Less(t, pages, 10, "pagination does not terminate")
		rows, err := st.ScanRoutesByStorage(ctx, storage, cursor, 2)
		require.NoError(t, err)
		for _, row := range rows {
			assert.Equal(t, storage, row.StorageID)
			got = append(got, row.PlayerID)
		}
		if len(rows) < 2 {
			break
		}
		cursor = store.RouteScanCursorAt(rows[len(rows)-1])
	}
	sort.Slice(want, func(i, j int) bool { return want[i] < want[j] })
	sort.Slice(got, func(i, j int) bool { return got[i] < got[j] })
	assert.Equal(t, want, got)

	stmt := inlineRouteArgs(t, store.RouteScanByStorageSQLForTest, []uint64{uint64(storage), 0, 0, 0, 2})
	if plan, ok := explainUnlessTiDB(t, db, stmt); ok {
		t.Logf("RD3 plan: %v", plan)
		assert.Contains(t, plan["possible_keys"], "idx_route_storage", "RD3 must be able to use idx_route_storage")
	}
}

// TestRouteReads_ListActiveMergeFences_RealDB:RD4 没有生效围栏时也回库时钟;有时只列 63 号片上生效的行,按 zone 升序。
func TestRouteReads_ListActiveMergeFences_RealDB(t *testing.T) {
	requireReadsIT(t)
	db := storetest.NewMigratedDB(t)
	st := newRouteStore(t, db)
	ctx := context.Background()
	const run = "61-62-20261018T000000Z-0a1b2c3d"
	readShard := uint32(store.RouteFenceReadShardForTest)

	before := readDBNowMs(t, db)
	empty, err := st.ListActiveMergeFences(ctx)
	require.NoError(t, err)
	after := readDBNowMs(t, db)
	assert.Empty(t, empty.Fences)
	assert.GreaterOrEqual(t, empty.DBNowMs, before, "now_ms must be the database clock")
	assert.LessOrEqual(t, empty.DBNowMs, after, "now_ms must be the database clock")

	// 61 / 62 在 63 号片生效(故意倒序插入,结果要按 zone 升序);63 已过期;64 只在 0 号片生效;65 是未封锁的空行。
	seedReadFence(t, db, 62, readShard, run, 3_600_000, 5)
	seedReadFence(t, db, 61, readShard, run, 3_600_000, 3)
	seedReadFence(t, db, 63, readShard, run, -3_600_000, 1)
	seedReadFence(t, db, 64, 0, run, 3_600_000, 1)
	seedReadFence(t, db, 65, readShard, "", 0, 0)

	got, err := st.ListActiveMergeFences(ctx)
	require.NoError(t, err)
	require.Len(t, got.Fences, 2, "only the two active fences on shard %d: %+v", readShard, got.Fences)
	for i, wantZone := range []uint32{61, 62} {
		f := got.Fences[i]
		assert.Equal(t, wantZone, f.ZoneID)
		assert.Equal(t, run, f.RunID)
		assert.Equal(t, "merge", f.Purpose)
		assert.Equal(t, wantZone, f.SrcZone)
		assert.Equal(t, wantZone+100, f.DstZone)
		assert.Greater(t, f.ExpiresAtMs, got.DBNowMs, "an active fence expires after the clock it was judged with")
		epoch, ok := placement.MergeFenceHolderEpoch(f.HolderJSON)
		assert.True(t, ok, "holder_json %q", f.HolderJSON)
		assert.Equal(t, f.FenceEpoch, epoch, "holder_json and fence_epoch must agree")
	}
	assert.EqualValues(t, 3, got.Fences[0].FenceEpoch)
	assert.EqualValues(t, 5, got.Fences[1].FenceEpoch)
}

// explainUnlessTiDB 在 MySQL 上取 EXPLAIN FORMAT=TRADITIONAL 的第一行;TiDB 上计划形状不同,只记一笔、回 ok=false(不 Skip:
// 设了 DATA_SERVICE_IT_REQUIRE=1 时 Skip 会被判失败,而这一段只是取证,不是依赖)。
func explainUnlessTiDB(t *testing.T, db *storetest.DB, stmt string) (map[string]string, bool) {
	t.Helper()
	var version string
	require.NoError(t, db.Raw.QueryRow("SELECT VERSION()").Scan(&version))
	if strings.Contains(strings.ToUpper(version), "TIDB") {
		t.Logf("TiDB %s: EXPLAIN FORMAT=TRADITIONAL not applicable, plan not checked", version)
		return nil, false
	}
	return explainPlan(t, db, stmt), true
}

// logExplainUnlessTiDB 只把计划原样留档。
func logExplainUnlessTiDB(t *testing.T, db *storetest.DB, label, stmt string) {
	t.Helper()
	if plan, ok := explainUnlessTiDB(t, db, stmt); ok {
		t.Logf("%s plan: %v", label, plan)
	}
}
