package routing

import (
	"context"
	"testing"
	"time"

	"data_service/internal/config"
	"data_service/internal/store"

	"shared/placement"

	"github.com/alicebob/miniredis/v2"
	goredis "github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// ── 写门禁 AcquirePlayerWriteLock(合服围栏预检 + 数据锁之后复核)──────────────────

func requireNoPlayerLocks(t *testing.T, mr *miniredis.Miniredis, playerID uint64) {
	t.Helper()
	assert.False(t, mr.Exists(mappingPlayerLockKey(playerID)), "mapping-side lock must not be held (player %d)", playerID)
	assert.False(t, mr.Exists(PlayerDataLockKey(playerID)), "data-side lock must not be held (player %d)", playerID)
}

func devClientOf(t *testing.T, r *Router) *goredis.Client {
	t.Helper()
	client, err := r.ClientForZone(0)
	require.NoError(t, err)
	base, ok := client.(*goredis.Client)
	require.Truef(t, ok, "dev 模式下 ClientForZone 应返回 *goredis.Client,实际 %T", client)
	return base
}

// actOnDataLockClient 在数据侧 SETNX 成功之后执行一次 act:模拟「数据锁到手、复核之前」合服立围栏或改映射。
type actOnDataLockClient struct {
	*goredis.Client
	act func()
}

func (c *actOnDataLockClient) SetNX(ctx context.Context, key string, value interface{}, expiration time.Duration) *goredis.BoolCmd {
	cmd := c.Client.SetNX(ctx, key, value, expiration)
	if cmd.Val() && c.act != nil {
		act := c.act
		c.act = nil
		act()
	}
	return cmd
}

func TestAcquirePlayerWriteLock_NoFenceHoldsBothLocks(t *testing.T) {
	r, mr := newTestRouter(t)
	ctx := context.Background()
	require.NoError(t, r.RegisterPlayerZone(ctx, 8801, 12, 0))

	client, token, locked, err := r.AcquirePlayerWriteLock(ctx, 8801)
	require.NoError(t, err)
	require.True(t, locked)
	require.NotEmpty(t, token)
	require.NotNil(t, client)
	assert.True(t, mr.Exists(mappingPlayerLockKey(8801)))
	assert.True(t, mr.Exists(PlayerDataLockKey(8801)))

	require.NoError(t, r.ReleasePlayerLock(ctx, client, 8801, token))
	requireNoPlayerLocks(t, mr, 8801)
}

// 合服的 src 与 dst 两个 zone 都打标;围栏只按 home 判,别的 zone 的玩家照常可写。预检被拒时一把锁都不碰。
func TestAcquirePlayerWriteLock_FencedBeforeLockTouchesNoLock(t *testing.T) {
	r, mr := newTestRouter(t)
	ctx := context.Background()
	require.NoError(t, r.RegisterPlayerZone(ctx, 8802, 12, 0))
	require.NoError(t, r.RegisterPlayerZone(ctx, 8803, 13, 0))
	require.NoError(t, r.RegisterPlayerZone(ctx, 8804, 14, 0))
	require.NoError(t, mr.Set(MergeFenceKey(12), "{}"))
	require.NoError(t, mr.Set(MergeFenceKey(13), "{}"))

	for _, pid := range []uint64{8802, 8803} {
		_, token, locked, err := r.AcquirePlayerWriteLock(ctx, pid)
		require.ErrorIs(t, err, ErrZoneMergeInProgress, "player %d", pid)
		assert.Contains(t, err.Error(), "before lock")
		assert.False(t, locked)
		assert.Empty(t, token)
		requireNoPlayerLocks(t, mr, pid)
	}

	client, token, locked, err := r.AcquirePlayerWriteLock(ctx, 8804)
	require.NoError(t, err)
	require.True(t, locked)
	require.NoError(t, r.ReleasePlayerLock(ctx, client, 8804, token))
}

// dev 模式下映射缺席照常可写(数据位置不依赖映射,与引入写门禁之前一致);Regions 模式下映射缺席是错误、不碰锁。
func TestAcquirePlayerWriteLock_UnmappedDevWritesRegionsRefuses(t *testing.T) {
	ctx := context.Background()
	r, mr := newTestRouter(t)
	client, token, locked, err := r.AcquirePlayerWriteLock(ctx, 8805)
	require.NoError(t, err)
	require.True(t, locked)
	require.NoError(t, r.ReleasePlayerLock(ctx, client, 8805, token))
	requireNoPlayerLocks(t, mr, 8805)

	rmr := miniredis.RunT(t)
	regions := NewRouter(config.Config{
		MappingRedis:     redis.RedisConf{Host: rmr.Addr(), Type: "node"},
		Regions:          []config.RegionConfig{{Id: 1, Zones: []uint32{1}, Redis: config.RedisClusterConfig{Addrs: []string{rmr.Addr()}}}},
		PlayerLockTTLSec: 3,
	})
	t.Cleanup(regions.Close)
	_, _, locked, err = regions.AcquirePlayerWriteLock(ctx, 8805)
	require.ErrorIs(t, err, ErrHomeZoneNotMapped)
	assert.NotErrorIs(t, err, ErrZoneMergeInProgress)
	assert.False(t, locked)
	requireNoPlayerLocks(t, rmr, 8805)
}

// 钉住「复核晚于数据锁」:围栏在数据锁到手之后才立起,复核必须看见它并释放两把锁。
// 若实现把复核放在数据锁之前,这里会拿到 locked=true。
func TestLockAndRecheckRoute_FenceRaisedAfterDataLock(t *testing.T) {
	r, mr := newTestRouter(t)
	ctx := context.Background()
	require.NoError(t, r.RegisterPlayerZone(ctx, 8806, 12, 0))
	wrapped := &actOnDataLockClient{Client: devClientOf(t, r), act: func() {
		require.NoError(t, mr.Set(MergeFenceKey(12), "{}"))
	}}

	token, locked, err := r.lockAndRecheckRoute(ctx, wrapped, 8806, writeRoute{home: 12, mapped: true})
	require.ErrorIs(t, err, ErrZoneMergeInProgress)
	assert.Contains(t, err.Error(), "after lock")
	assert.False(t, locked)
	assert.Empty(t, token)
	requireNoPlayerLocks(t, mr, 8806)

	// 锁已立即释放,撤掉围栏后不用等 TTL 就能再拿。
	mr.Del(MergeFenceKey(12))
	client, token, locked, err := r.AcquirePlayerWriteLock(ctx, 8806)
	require.NoError(t, err)
	require.True(t, locked)
	require.NoError(t, r.ReleasePlayerLock(ctx, client, 8806, token))
}

// 数据锁到手之后映射被合服改写:复核发现 home 变了,按合服拒绝并释放两把锁。
// dev 模式下映射在这期间被删掉同样算 home 变化(12 -> unmapped)。
func TestLockAndRecheckRoute_HomeChangedAfterDataLock(t *testing.T) {
	r, mr := newTestRouter(t)
	ctx := context.Background()
	for pid, change := range map[uint64]struct {
		act  func()
		want string
	}{
		8807: {act: func() { require.NoError(t, mr.Set(mappingKey(8807), "13")) }, want: "12 -> 13"},
		8808: {act: func() { mr.Del(mappingKey(8808)) }, want: "12 -> unmapped"},
	} {
		require.NoError(t, r.RegisterPlayerZone(ctx, pid, 12, 0))
		wrapped := &actOnDataLockClient{Client: devClientOf(t, r), act: change.act}
		_, locked, err := r.lockAndRecheckRoute(ctx, wrapped, pid, writeRoute{home: 12, mapped: true})
		require.ErrorIs(t, err, ErrZoneMergeInProgress, "player %d", pid)
		assert.Contains(t, err.Error(), change.want)
		assert.False(t, locked)
		requireNoPlayerLocks(t, mr, pid)
	}
}

// ReadRequiresReady=true:缓存不可信时写门禁的预检与复核都回源库(home 与围栏同一条语句);库故障回 ErrRouteReadUnavailable、不碰锁。
func TestAcquirePlayerWriteLock_ReadyModeUsesSQLHomeAndFence(t *testing.T) {
	ctx := context.Background()
	reader := &readFakeReader{homeFence: map[uint64]store.RouteHomeFence{
		8811: {HomeZone: 9, Fenced: true},
		8812: {HomeZone: 9},
	}}
	r, mr, _ := newReadGateRouter(t, true, reader)
	mr.Del(placement.RouteReadyKey) // 标记缺席 → 缓存不可信 → 回源

	_, _, locked, err := r.AcquirePlayerWriteLock(ctx, 8811)
	require.ErrorIs(t, err, ErrZoneMergeInProgress)
	assert.False(t, locked)
	requireNoPlayerLocks(t, mr, 8811)

	client, token, locked, err := r.AcquirePlayerWriteLock(ctx, 8812)
	require.NoError(t, err)
	require.True(t, locked)
	assert.Equal(t, []uint64{8811, 8812, 8812}, reader.homeFenceCalls, "8812 预检与复核各回源一次")
	require.NoError(t, r.ReleasePlayerLock(ctx, client, 8812, token))

	broken, bmr, _ := newReadGateRouter(t, true, &readFakeReader{err: errReadBoom})
	bmr.Del(placement.RouteReadyKey)
	_, _, locked, err = broken.AcquirePlayerWriteLock(ctx, 8813)
	require.ErrorIs(t, err, ErrRouteReadUnavailable)
	assert.False(t, locked)
	requireNoPlayerLocks(t, bmr, 8813)
}
