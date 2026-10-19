package saveoutbox

import (
	"context"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
)

// 门禁拒启:dual 宣告严格世代之后,同 zone 的 legacy 启动检查必须拒绝;标记不带 TTL、按 zone 隔离、重复宣告幂等。
func TestStrictMarkerRefusesLegacyStart(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	require.NoError(t, CheckLegacyAllowed(ctx, rc, 1), "没有任何严格世代证据的 zone 允许 legacy")
	require.NoError(t, AnnounceStrictGeneration(ctx, rc, 1))
	require.NoError(t, AnnounceStrictGeneration(ctx, rc, 1), "重复宣告必须幂等")
	require.ErrorIs(t, CheckLegacyAllowed(ctx, rc, 1), ErrStrictGeneration)
	require.NoError(t, CheckLegacyAllowed(ctx, rc, 2), "世代按 zone 判定")
	ttl, err := rc.Do(ctx, "TTL", GenerationKey(1)).Int64()
	require.NoError(t, err)
	require.Equal(t, int64(-1), ttl, "世代标记不得随 TTL 消失")
}

// 门禁拒启:没有标记但本 zone 补投桶里已有严格写者留下的意图,legacy 既不能 ACK 也不能补投,同样拒启。
func TestPendingIntentRefusesLegacyStart(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	require.NoError(t, rc.ZAdd(ctx, pendingIndexKey(1, 37), redis.Z{Score: 0, Member: "101"}).Err())
	require.ErrorIs(t, CheckLegacyAllowed(ctx, rc, 1), ErrStrictGeneration)
	require.NoError(t, CheckLegacyAllowed(ctx, rc, 3), "别的 zone 的桶不影响本 zone")
}

// 判不清世代一律拒启:标记或任一桶类型不对、Redis 不可达,都不能当作「没有证据」放 legacy 进来。
func TestLegacyGateFailsClosed(t *testing.T) {
	ctx := context.Background()
	t.Run("marker wrong type", func(t *testing.T) {
		rc := fixtureRedis(t)
		require.NoError(t, rc.HSet(ctx, GenerationKey(1), "x", "y").Err())
		err := CheckLegacyAllowed(ctx, rc, 1)
		require.Error(t, err)
		require.NotErrorIs(t, err, ErrStrictGeneration)
	})
	t.Run("bucket wrong type", func(t *testing.T) {
		rc := fixtureRedis(t)
		require.NoError(t, rc.Set(ctx, pendingIndexKey(1, 5), "错误类型", 0).Err())
		require.Error(t, CheckLegacyAllowed(ctx, rc, 1))
	})
	t.Run("redis unreachable", func(t *testing.T) {
		mr := miniredis.RunT(t)
		rc := redis.NewClient(&redis.Options{Addr: mr.Addr(), MaxRetries: -1})
		t.Cleanup(func() { _ = rc.Close() })
		mr.Close()
		require.Error(t, CheckLegacyAllowed(ctx, rc, 1))
		require.Error(t, AnnounceStrictGeneration(ctx, rc, 1), "dual 写不上标记也必须拒启")
	})
}

// 未知世代值与错误类型不得被 dual 静默覆盖:那会把别的协议版本的事实改写成 strict。
func TestAnnounceRejectsUnknownGeneration(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	require.NoError(t, rc.Set(ctx, GenerationKey(1), "strict-v9", 0).Err())
	require.Error(t, AnnounceStrictGeneration(ctx, rc, 1))
	require.Equal(t, "strict-v9", rc.Get(ctx, GenerationKey(1)).Val())
	require.NoError(t, rc.HSet(ctx, GenerationKey(2), "x", "y").Err())
	require.Error(t, AnnounceStrictGeneration(ctx, rc, 2))
	require.ErrorIs(t, CheckLegacyAllowed(ctx, rc, 1), ErrStrictGeneration, "未知值同样算严格世代证据")
}

// 运行中门禁:legacy 实例每拍续写存活标记;严格世代证据出现时恰好报告一次,之后仍续写存活标记(停机排空期间补投继续等)。
func TestLegacyWatcherReportsConflictOnceAndKeepsAliveMarker(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	w := &legacyWatcher{rc: rc, zone: 1}
	require.NoError(t, w.step(ctx))
	alive, err := legacyAlive(ctx, rc, 1)
	require.NoError(t, err)
	require.True(t, alive)
	ttl, err := rc.Do(ctx, "PTTL", legacyAliveKey(1)).Int64()
	require.NoError(t, err)
	require.Positive(t, ttl, "存活标记必须带 TTL,实例退出后自然消失")

	require.NoError(t, AnnounceStrictGeneration(ctx, rc, 1))
	require.ErrorIs(t, w.step(ctx), ErrStrictGeneration)
	require.NoError(t, rc.Del(ctx, legacyAliveKey(1)).Err())
	require.NoError(t, w.step(ctx), "冲突只报告一次")
	alive, err = legacyAlive(ctx, rc, 1)
	require.NoError(t, err)
	require.True(t, alive, "冲突之后仍续写存活标记")
}
