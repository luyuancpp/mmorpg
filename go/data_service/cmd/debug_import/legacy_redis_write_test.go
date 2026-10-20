package main

import (
	"context"
	"testing"

	"data_service/internal/config"
	"data_service/internal/constants"
	"data_service/internal/logic"
	"data_service/internal/routing"
	"data_service/internal/svc"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// 玩家已经经 data_service 存过一次(字段索引戳 "1"),再用 player 模式导入、把版本重置回 "1":
// 导入必须同时删掉字段索引,否则戳与重置后的版本相等,索引被当成完整的,导入的字段 x 从全量读里消失,
// 下一次存盘还会把这个不完整的索引固化。
func TestWriteLegacyPlayerFields_DropsFieldIndexSoImportedFieldsStayVisible(t *testing.T) {
	ctx := context.Background()
	mr := miniredis.RunT(t)
	c := config.Config{
		MappingRedis:     redis.RedisConf{Host: mr.Addr(), Type: "node"},
		DevRedis:         config.DevRedisConfig{Host: mr.Addr()},
		PlayerLockTTLSec: 3,
	}
	r := routing.NewRouter(c)
	t.Cleanup(r.Close)
	svcCtx := &svc.ServiceContext{Config: c, Router: r}
	const pid uint64 = 9401
	require.NoError(t, r.RegisterPlayerZone(ctx, pid, 1, 0))

	saved, err := logic.SavePlayerData(ctx, svcCtx, &logic.SavePlayerDataReq{PlayerID: pid, Data: map[string][]byte{"a": []byte("1")}})
	require.NoError(t, err)
	require.Equal(t, uint64(1), saved.NewVersion)
	require.True(t, mr.Exists("player:{9401}:__fields"))

	client, err := r.ClientForZone(1)
	require.NoError(t, err)
	require.NoError(t, writeLegacyPlayerFields(ctx, client, pid, []string{"a", "x"},
		map[string][]byte{"a": []byte("imported"), "x": []byte("new")}))
	assert.False(t, mr.Exists("player:{9401}:__fields"), "导入必须删掉字段索引")
	ver, _ := mr.Get("player:{9401}:__version")
	assert.Equal(t, "1", ver)

	loadAll := func() map[string][]byte {
		t.Helper()
		resp, err := logic.LoadPlayerData(ctx, svcCtx, &logic.LoadPlayerDataReq{PlayerID: pid})
		require.NoError(t, err)
		require.Equal(t, constants.ErrCodeOK, resp.ErrorCode)
		return resp.Data
	}
	assert.Equal(t, map[string][]byte{"a": []byte("imported"), "x": []byte("new")}, loadAll())

	_, err = logic.SavePlayerData(ctx, svcCtx, &logic.SavePlayerDataReq{PlayerID: pid, Data: map[string][]byte{"a": []byte("2")}})
	require.NoError(t, err)
	assert.Equal(t, map[string][]byte{"a": []byte("2"), "x": []byte("new")}, loadAll(), "导入之后的存盘不得把 x 藏掉")
}
