//go:build integration

package routetool

// route_tool 端到端:真 MySQL(storetest 一次性库,真 store.RouteStore)+ miniredis 造数据(第 1a 批规格 f 节)。
//
//  1. import(dry-run)零写入;真导入之后 audit 为 0,再导一次全部 matched(幂等),maprev 成为新行的 rev;
//  2. 改一个 Redis 值 → audit 退出码 1(value_mismatch);
//  3. 库里插一行而 Redis 不写 → db_only_unpublished,退出码 1。
//
// 跑法同 store 包的集成用例(本地 docker 的 appuser 没有建库权限,用 root):
//   DATA_SERVICE_IT_MYSQL_USER=root DATA_SERVICE_IT_MYSQL_PASSWORD=<deploy/docker-compose.yml> \
//     go test -tags=integration ./internal/routetool/...

import (
	"bytes"
	"context"
	"testing"

	"data_service/internal/store"
	"data_service/internal/store/storetest"

	"shared/placement"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

func TestRouteToolImportThenAuditOnRealStore(t *testing.T) {
	db := storetest.NewMigratedDB(t)
	st, err := store.NewRouteStore(db.Cfg, store.RouteStoreOptions{
		FenceShards: placement.DefaultRouteFenceShards,
		Writer:      store.NewRouteWriterID("route_tool_it", "localhost"),
	})
	require.NoError(t, err)
	t.Cleanup(func() { _ = st.Close() })

	mr := miniredis.RunT(t)
	rd := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})
	setKeys(t, mr, map[string]string{
		"player:zone:9001":      "1",
		"player:placement:9001": "1:1",
		"player:zone:9002":      "2",
		"player:placement:9003": "3:1",
		"player:zone:9004":      "4",
		"player:maprev:9004":    "3",
		"merge:merged_into:7":   `{"dst":8,"run_id":"7-8-20261012T000000Z-0123abcd"}`,
	})
	ctx := context.Background()

	importOnce := func(dryRun bool) (ImportReport, int) {
		t.Helper()
		var out, errOut bytes.Buffer
		rep, err := Import(ctx, rd, st, ImportOptions{DryRun: dryRun, Batch: 2, MaxReport: 100, Stdout: &out, Stderr: &errOut})
		code := ImportExitCode(rep, err)
		t.Logf("import dry_run=%t exit=%d stdout=%s stderr=%s", dryRun, code, out.String(), errOut.String())
		return rep, code
	}
	auditOnce := func() (AuditReport, int) {
		t.Helper()
		var out, errOut bytes.Buffer
		rep, err := Audit(ctx, rd, st, AuditOptions{Batch: 2, MaxReport: 100, MaxRecheck: 100, Stdout: &out, Stderr: &errOut})
		code := AuditExitCode(rep, err)
		t.Logf("audit exit=%d stdout=%s stderr=%s", code, out.String(), errOut.String())
		return rep, code
	}

	// 1. dry-run:统计照出,库里零写入。
	rep, code := importOnce(true)
	require.Equal(t, ExitClean, code)
	assert.Equal(t, 4, rep.Inserted)
	assert.Equal(t, 1, rep.MergedIntoInserted)
	assert.Equal(t, int64(0), db.Count(t, store.PlayerRouteTableName, ""))
	assert.Equal(t, int64(0), db.Count(t, store.ZoneMergedIntoTableName, ""))

	// 真导入 → audit 为 0。
	rep, code = importOnce(false)
	require.Equal(t, ExitClean, code)
	assert.Equal(t, 4, rep.Inserted)
	assert.Equal(t, 1, rep.MergedIntoInserted)
	got, err := st.GetRoutes(ctx, []uint64{9003, 9004})
	require.NoError(t, err)
	assert.Equal(t, uint64(3), got[9004].Rev, "新行 rev = maprev")
	assert.Equal(t, uint64(1), got[9003].Rev, "无 maprev 的新行 rev = 1")
	assert.Equal(t, uint32(0), got[9003].HomeZone, "只有落点的玩家导入后 home 仍为 NULL")

	audit, code := auditOnce()
	assert.Equal(t, ExitClean, code)
	assert.Equal(t, 0, audit.Differences())
	assert.Equal(t, 4, audit.DBRows)
	assert.Equal(t, 4, audit.RedisPlayers)

	// 再导一次:全部 matched,零写入(幂等)。
	rep, code = importOnce(false)
	require.Equal(t, ExitClean, code)
	assert.Equal(t, 0, rep.Inserted)
	assert.Equal(t, 4, rep.Matched)
	assert.Equal(t, 1, rep.MergedIntoMatched)

	// 2. 改一个 Redis 值 → value_mismatch。
	require.NoError(t, mr.Set("player:zone:9002", "5"))
	audit, code = auditOnce()
	assert.Equal(t, ExitAttention, code)
	assert.Equal(t, 1, audit.ValueMismatch)
	assert.Equal(t, 1, audit.Differences())
	require.NoError(t, mr.Set("player:zone:9002", "2"))

	// 3. 库里有行、缓存三键全无 → db_only_unpublished(W1 已提交未发布的孤儿行就是这个形状)。
	// route_shard = 9005 % 64 = 45(9005 = 140×64 + 45;聚簇主键首列,chk_route_shard 钉住)。
	_, err = db.Raw.Exec("INSERT INTO player_route (route_shard, player_id, home_zone, storage_id, placement_version, frozen_run_id,"+
		" rev, updated_by, created_at_ms, updated_at_ms) VALUES (?, 9005, 1, NULL, NULL, NULL, 1, 'it-orphan', 1, 1)",
		uint64(placement.RouteRowShard(9005)))
	require.NoError(t, err)
	audit, code = auditOnce()
	assert.Equal(t, ExitAttention, code)
	assert.Equal(t, 1, audit.DBOnlyUnpublished)
	assert.Equal(t, 1, audit.Differences())
}
