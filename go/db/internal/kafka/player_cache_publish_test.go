package kafka

import (
	"context"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

type cacheRaceHook struct {
	once      bool
	afterRead func()
}

func (h *cacheRaceHook) DialHook(next redis.DialHook) redis.DialHook { return next }
func (h *cacheRaceHook) ProcessPipelineHook(next redis.ProcessPipelineHook) redis.ProcessPipelineHook {
	return next
}
func (h *cacheRaceHook) ProcessHook(next redis.ProcessHook) redis.ProcessHook {
	return func(ctx context.Context, cmd redis.Cmder) error {
		err := next(ctx, cmd)
		if err == nil && cmd.Name() == "get" && !h.once {
			h.once = true
			h.afterRead()
		}
		return err
	}
}

func TestPlayerCachePublicationCannotOverwriteNewerVersion(t *testing.T) {
	server := miniredis.RunT(t)
	client := redis.NewClient(&redis.Options{Addr: server.Addr()})
	t.Cleanup(func() { _ = client.Close() })
	ctx := context.Background()
	key := "player_database:42"
	old := fenceRow(1)
	old.FenceOwnerEpoch = 1
	old.FenceSaveSeq = 1
	newer := fenceRow(9)
	newer.FenceOwnerEpoch = 2
	newer.FenceSaveSeq = 1
	oldRaw, err := proto.Marshal(old)
	require.NoError(t, err)
	newRaw, err := proto.Marshal(newer)
	require.NoError(t, err)
	require.NoError(t, server.Set(key, string(oldRaw)))
	// 在旧发布者完成 GET 后，另一个实例已发布新 SQL 行；旧 CAS 必须失败并重读。
	client.AddHook(&cacheRaceHook{afterRead: func() { require.NoError(t, server.Set(key, string(newRaw))) }})
	old.FenceSaveSeq = 2
	data, err := proto.Marshal(old)
	require.NoError(t, err)
	handled, err := publishPlayerCache(ctx, client, key, old, data, time.Hour)
	require.NoError(t, err)
	require.True(t, handled)
	actual, err := server.Get(key)
	require.NoError(t, err)
	require.Equal(t, string(newRaw), actual)
}

func TestPlayerCachePublicationRejectsSameVersionConflict(t *testing.T) {
	server := miniredis.RunT(t)
	client := redis.NewClient(&redis.Options{Addr: server.Addr()})
	t.Cleanup(func() { _ = client.Close() })
	old := fenceRow(1)
	old.FenceOwnerEpoch = 1
	old.FenceSaveSeq = 1
	data, err := proto.Marshal(old)
	require.NoError(t, err)
	require.NoError(t, server.Set("player_database:42", string(data)))
	old.Currency.Values[1] = 99
	changed, err := proto.Marshal(old)
	require.NoError(t, err)
	_, err = publishPlayerCache(context.Background(), client, "player_database:42", old, changed, time.Hour)
	require.Error(t, err)
	actual, err := server.Get("player_database:42")
	require.NoError(t, err)
	require.Equal(t, string(data), actual)
}
