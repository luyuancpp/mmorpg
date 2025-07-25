package fsmstore

import (
	"context"
	"testing"
	"time"

	"github.com/looplab/fsm"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
)

func newTestFSM() *fsm.FSM {
	return fsm.NewFSM(
		"start",
		fsm.Events{
			{Name: "to_middle", Src: []string{"start"}, Dst: "middle"},
			{Name: "to_end", Src: []string{"middle"}, Dst: "end"},
		},
		fsm.Callbacks{},
	)
}

func TestFSMStore_SaveAndLoad(t *testing.T) {
	ctx := context.Background()

	// 创建本地 RedisClient 测试连接（默认 127.0.0.1:6379）
	rdb := redis.NewClient(&redis.Options{
		Addr: "127.0.0.1:6379",
		DB:   1, // 用测试库，避免污染主库
	})

	account := "testAccount"
	roleId := "role1"
	key := redisKey(account, roleId)

	// 清理开始
	defer func() {
		_ = rdb.Del(ctx, key).Err()
	}()

	// 初始化 FSM
	f := newTestFSM()

	// 执行状态迁移
	err := f.Event(ctx, "to_middle")
	assert.NoError(t, err)
	assert.Equal(t, "middle", f.Current())

	// 保存状态
	err = SaveFSMState(ctx, rdb, f, account, roleId)
	assert.NoError(t, err)

	// 创建新的 FSM 模拟服务重启
	newFSM := newTestFSM()
	assert.Equal(t, "start", newFSM.Current())

	// 恢复状态
	err = LoadFSMState(ctx, rdb, newFSM, account, roleId)
	assert.NoError(t, err)
	assert.Equal(t, "middle", newFSM.Current())

	// 再次迁移
	err = newFSM.Event("to_end")
	assert.NoError(t, err)
	assert.Equal(t, "end", newFSM.Current())

	// 覆盖保存新状态
	err = SaveFSMState(ctx, rdb, newFSM, account, roleId)
	assert.NoError(t, err)

	// 验证 RedisClient 中状态
	val, err := rdb.Get(ctx, key).Result()
	assert.NoError(t, err)
	assert.Equal(t, "end", val)
}
