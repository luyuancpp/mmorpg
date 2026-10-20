package logic

import (
	"context"
	"testing"

	"data_service/internal/config"
	"data_service/internal/constants"
	"data_service/internal/routing"
	"data_service/internal/svc"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/logx/logtest"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// ── 路由失败:in-band 入口一律回 ErrCodeRedis(故障),原因只在 [DataRoute] 日志里 ──────────────
// 裁决见 docs/design/data_service_role_and_scope.md「已澄清」:4 不在 FaultCodeSet 里,改成 4 会让路由故障哑掉,
// 还会和快照的「无数据」4 混在一起;账本读回 Unavailable、绝不 NotFound。

// newRegionsTestSvcCtx:Regions 模式(不配 DevRedis),ClientForPlayer 必须先查映射。只配 zone 1,集群地址指回同一个
// miniredis(写法同 rollback_recall_test.go):路由失败的用例不该碰到它,万一碰到,写进去的键会被 mr.Keys() 抓住。
func newRegionsTestSvcCtx(t *testing.T) (*svc.ServiceContext, *miniredis.Miniredis) {
	t.Helper()
	mr := miniredis.RunT(t)
	c := config.Config{
		MappingRedis:     redis.RedisConf{Host: mr.Addr(), Type: "node"},
		Regions:          []config.RegionConfig{{Id: 1, Zones: []uint32{1}, Redis: config.RedisClusterConfig{Addrs: []string{mr.Addr()}}}},
		PlayerLockTTLSec: 3,
	}
	r := routing.NewRouter(c)
	t.Cleanup(r.Close)
	return &svc.ServiceContext{Config: c, Router: r}, mr
}

// 映射缺席:五个 in-band 入口都回 ErrCodeRedis(故障),绝不 NotFound;错误链保留 ErrHomeZoneNotMapped;
// 零副作用;每个入口各有一行带原因的 [DataRoute] 日志。
func TestRouteFailure_UnmappedPlayerIsFaultWithoutSideEffects(t *testing.T) {
	const pid uint64 = 9101
	logs := logtest.NewCollector(t)
	svcCtx, mr := newRegionsTestSvcCtx(t)
	ctx := context.Background()

	for _, fields := range [][]string{nil, {"gold"}} {
		resp, err := LoadPlayerData(ctx, svcCtx, &LoadPlayerDataReq{PlayerID: pid, Fields: fields})
		require.ErrorIs(t, err, routing.ErrHomeZoneNotMapped, "fields=%v", fields)
		assert.Equal(t, constants.ErrCodeRedis, resp.ErrorCode, "fields=%v", fields)
	}
	saveResp, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{PlayerID: pid, Data: map[string][]byte{"gold": []byte("1")}})
	require.ErrorIs(t, err, routing.ErrHomeZoneNotMapped)
	assert.Equal(t, constants.ErrCodeRedis, saveResp.ErrorCode)
	setResp, err := SetPlayerField(ctx, svcCtx, pid, "gold", []byte("1"), 0)
	require.ErrorIs(t, err, routing.ErrHomeZoneNotMapped)
	assert.Equal(t, constants.ErrCodeRedis, setResp.ErrorCode)
	delResp, err := DeletePlayerData(ctx, svcCtx, &DeletePlayerDataReq{PlayerID: pid})
	require.ErrorIs(t, err, routing.ErrHomeZoneNotMapped)
	assert.Equal(t, constants.ErrCodeRedis, delResp.ErrorCode)
	_, err = GetPlayerField(ctx, svcCtx, pid, "gold")
	require.ErrorIs(t, err, routing.ErrHomeZoneNotMapped)

	assert.Empty(t, mr.Keys(), "路由失败在拿锁之前,零副作用")
	out := logs.String()
	for _, op := range []string{"LoadPlayerData", "SavePlayerData", "SetPlayerField", "DeletePlayerData", "GetPlayerField"} {
		assert.Contains(t, out, routeFailureLogTag+" "+op+" player=9101:", "op=%s", op)
	}
	assert.Contains(t, out, "no home zone mapping")
}

// Regions 漏配 zone(配置漂移):故障 1,不留锁键,日志带出是哪个 zone。
func TestRouteFailure_ZoneWithoutClusterIsFault(t *testing.T) {
	const pid uint64 = 9102
	logs := logtest.NewCollector(t)
	svcCtx, mr := newRegionsTestSvcCtx(t)
	ctx := context.Background()
	require.NoError(t, svcCtx.Router.RegisterPlayerZone(ctx, pid, 99, 0)) // zone 99 不在 Regions 里

	resp, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{PlayerID: pid, Data: map[string][]byte{"gold": []byte("1")}})
	require.Error(t, err)
	assert.NotErrorIs(t, err, routing.ErrHomeZoneNotMapped)
	assert.Equal(t, constants.ErrCodeRedis, resp.ErrorCode)
	assert.False(t, mr.Exists(routing.PlayerDataLockKey(pid)))
	assert.False(t, mr.Exists("lock:player:9102"), "mapping 锁不得残留(routing 包未导出的 mappingPlayerLockKey)")
	assert.Contains(t, logs.String(), routeFailureLogTag+" SavePlayerData player=9102:")
	assert.Contains(t, logs.String(), "no Redis cluster configured for zone 99")
}

// mapping Redis 故障:故障 1,且错误链里没有 ErrHomeZoneNotMapped(故障不是缺席),日志带出 Redis 错误文本。
func TestRouteFailure_MappingRedisErrorIsFaultNotAbsence(t *testing.T) {
	logs := logtest.NewCollector(t)
	svcCtx, mr := newRegionsTestSvcCtx(t)
	mr.SetError("injected mapping failure")
	t.Cleanup(func() { mr.SetError("") })

	resp, err := LoadPlayerData(context.Background(), svcCtx, &LoadPlayerDataReq{PlayerID: 9103})
	require.Error(t, err)
	assert.NotErrorIs(t, err, routing.ErrHomeZoneNotMapped, "映射库故障不是映射缺席")
	assert.Equal(t, constants.ErrCodeRedis, resp.ErrorCode)
	assert.Contains(t, logs.String(), routeFailureLogTag+" LoadPlayerData player=9103:")
	assert.Contains(t, logs.String(), "injected mapping failure")
}

// DevRedis 下今天可达的那一条:有数据、无映射 → CreatePlayerSnapshot 第 3 步失败,回 1、不写快照、日志带原因。
func TestCreatePlayerSnapshot_UnmappedPlayerLogsRouteCause(t *testing.T) {
	const pid uint64 = 9104
	logs := logtest.NewCollector(t)
	svcCtx, _ := newTestSvcCtx(t)
	snapshots := &fakeSnapshotStore{}
	svcCtx.SnapshotStore = snapshots
	ctx := context.Background()
	saveResp, err := SavePlayerData(ctx, svcCtx, &SavePlayerDataReq{PlayerID: pid, Data: map[string][]byte{"gold": []byte("1")}})
	require.NoError(t, err)
	require.Equal(t, constants.ErrCodeOK, saveResp.ErrorCode)
	assert.NotContains(t, logs.String(), routeFailureLogTag, "dev 模式下成功路径不记路由失败")

	resp, err := CreatePlayerSnapshot(ctx, svcCtx, &CreateSnapshotReq{PlayerID: pid, Operator: "test"})
	require.ErrorIs(t, err, routing.ErrHomeZoneNotMapped)
	assert.Equal(t, constants.ErrCodeRedis, resp.ErrorCode)
	assert.Zero(t, snapshots.insertCalls, "归属查不到就不写快照")
	assert.Contains(t, logs.String(), routeFailureLogTag+" CreatePlayerSnapshot player=9104:")
	assert.Contains(t, logs.String(), "no home zone mapping")
}

// 账本 SQL 证据读的路由失败:Unavailable(绝不 NotFound),message 带原因。
func TestGetSQLCommittedAssetLedger_RouteFailureKeepsUnavailableWithCause(t *testing.T) {
	svcCtx, _ := newRegionsTestSvcCtx(t)
	_, found, err := GetSQLCommittedAssetLedger(context.Background(), svcCtx, 9105)
	assert.False(t, found)
	assert.Equal(t, codes.Unavailable, status.Code(err), "err=%v", err)
	assert.Contains(t, status.Convert(err).Message(), "no home zone mapping")
}
