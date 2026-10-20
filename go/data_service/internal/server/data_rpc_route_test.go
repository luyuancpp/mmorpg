package server

import (
	"context"
	"testing"

	"data_service/internal/config"
	"data_service/internal/constants"
	"data_service/internal/routing"
	"data_service/internal/svc"
	"proto/data_service"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// ── 玩家数据 RPC 的线上形状:路由失败回 in-band 1,合服围栏回 in-band 22,gRPC 层都是 OK ──────────────

// Regions 模式下映射缺席:五个 in-band 数据 RPC 都回 ErrCodeRedis(1,在 FaultCodeSet 里),不是 4。
func TestInbandDataRPCs_RouteFailureAnswersErrCodeRedis(t *testing.T) {
	mr := miniredis.RunT(t)
	c := config.Config{
		MappingRedis:     redis.RedisConf{Host: mr.Addr(), Type: "node"},
		Regions:          []config.RegionConfig{{Id: 1, Zones: []uint32{1}, Redis: config.RedisClusterConfig{Addrs: []string{mr.Addr()}}}},
		PlayerLockTTLSec: 3,
	}
	r := routing.NewRouter(c)
	t.Cleanup(r.Close)
	s := NewDataServiceServer(&svc.ServiceContext{Config: c, Router: r})
	ctx := context.Background()
	const pid uint64 = 9201

	load, err := s.LoadPlayerData(ctx, &data_service.LoadPlayerDataRequest{PlayerId: pid})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeRedis, load.GetErrorCode())
	save, err := s.SavePlayerData(ctx, &data_service.SavePlayerDataRequest{PlayerId: pid, Data: map[string][]byte{"gold": []byte("1")}})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeRedis, save.GetErrorCode())
	get, err := s.GetPlayerField(ctx, &data_service.GetPlayerFieldRequest{PlayerId: pid, Field: "gold"})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeRedis, get.GetErrorCode())
	set, err := s.SetPlayerField(ctx, &data_service.SetPlayerFieldRequest{PlayerId: pid, Field: "gold", Value: []byte("1")})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeRedis, set.GetErrorCode())
	del, err := s.DeletePlayerData(ctx, &data_service.DeletePlayerDataRequest{PlayerId: pid})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeRedis, del.GetErrorCode())
}

// 合服围栏内的写:三个写 RPC 都是 gRPC OK + in-band 22(规则拒绝,不进 FaultCodeSet)。
func TestWritesMergeFenceIsInBand(t *testing.T) {
	s, mr := newHomeZoneTestServer(t)
	ctx := context.Background()
	const pid uint64 = 5301
	require.NoError(t, s.svcCtx.Router.RegisterPlayerZone(ctx, pid, 20, 0))
	require.NoError(t, mr.Set(routing.MergeFenceKey(20), "{}"))

	save, err := s.SavePlayerData(ctx, &data_service.SavePlayerDataRequest{PlayerId: pid, Data: map[string][]byte{"gold": []byte("1")}})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeZoneMergeInProgress, save.GetErrorCode())
	set, err := s.SetPlayerField(ctx, &data_service.SetPlayerFieldRequest{PlayerId: pid, Field: "gold", Value: []byte("1")})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeZoneMergeInProgress, set.GetErrorCode())
	del, err := s.DeletePlayerData(ctx, &data_service.DeletePlayerDataRequest{PlayerId: pid})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeZoneMergeInProgress, del.GetErrorCode())
	assert.False(t, mr.Exists(routing.PlayerDataLockKey(pid)))
}
