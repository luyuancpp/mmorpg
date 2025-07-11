package fsmstore

import (
	"context"
	"fmt"

	"github.com/looplab/fsm"
	"github.com/redis/go-redis/v9"
)

// RedisKeyFormat 返回 Redis 中保存 FSM 状态的 key
func redisKey(account, roleId string) string {
	return fmt.Sprintf("fsm_state:%s:%s", account, roleId)
}

// SaveFSMState saves the current FSM state to Redis
func SaveFSMState(ctx context.Context, redisClient *redis.Client, f *fsm.FSM, account, roleId string) error {
	key := redisKey(account, roleId)
	state := f.Current()
	return redisClient.Set(ctx, key, state, 0).Err()
}

// LoadFSMState loads FSM state from Redis and applies it to the FSM
func LoadFSMState(ctx context.Context, redisClient *redis.Client, f *fsm.FSM, account, roleId string) error {
	key := redisKey(account, roleId)
	state, err := redisClient.Get(ctx, key).Result()
	if err == redis.Nil {
		// 没有存过状态，忽略即可（可能是第一次登录）
		return nil
	}
	if err != nil {
		return fmt.Errorf("redis get failed: %w", err)
	}

	f.SetState(state)

	return nil
}

func DeleteFSMState(ctx context.Context, redis *redis.Client, account string, suffix string) error {
	key := redisKey(account, suffix)
	return redis.Del(ctx, key).Err()
}
