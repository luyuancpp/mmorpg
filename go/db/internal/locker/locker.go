package locker

import (
	"context"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// RedisLocker is a Redis-based distributed lock.
type RedisLocker struct {
	redisClient redis.Cmdable
	prefix      string // lock key prefix
}

// TryLockResult holds the result of a lock attempt.
type TryLockResult struct {
	locked    bool
	lockKey   string
	lockValue string // unique value to prevent releasing others' locks
	redis     redis.Cmdable
}


func NewRedisLocker(redisClient redis.Cmdable, prefix ...string) *RedisLocker {
	lockPrefix := "distributed:lock:"
	if len(prefix) > 0 && prefix[0] != "" {
		lockPrefix = prefix[0]
	}
	return &RedisLocker{
		redisClient: redisClient,
		prefix:      lockPrefix,
	}
}

// TryLock attempts to acquire the lock without blocking.
func (rl *RedisLocker) TryLock(ctx context.Context, key string, ttl time.Duration) (*TryLockResult, error) {
	finalLockKey := rl.prefix + key
	lockValue := uuid.NewString()

	setCmd := rl.redisClient.SetNX(
		ctx,
		finalLockKey,
		lockValue,
		ttl,
	)
	success, err := setCmd.Result()
	if err != nil {
		logx.Errorf("Redis TryLock failed | key=%s | err=%v", finalLockKey, err)
		return &TryLockResult{locked: false}, fmt.Errorf("redis setnx failed: %w", err)
	}

	return &TryLockResult{
		locked:    success,
		lockKey:   finalLockKey,
		lockValue: lockValue,
		redis:     rl.redisClient,
	}, nil
}

// IsLocked returns whether the lock was acquired.
func (tlr *TryLockResult) IsLocked() bool {
	return tlr.locked
}

// Release releases the lock if still held by this owner.
func (tlr *TryLockResult) Release(ctx context.Context) (bool, error) {
	if !tlr.locked {
		logx.Errorf("Release lock failed: not holding lock | key=%s", tlr.lockKey)
		return false, nil
	}

	// Lua script: atomically verify owner then delete
	luaScript := `
		local currentValue = redis.call('GET', KEYS[1])
		if currentValue == ARGV[1] then
			return redis.call('DEL', KEYS[1])
		else
			return 0
		end
	`
	delResult, err := tlr.redis.
		Eval(ctx, luaScript, []string{tlr.lockKey}, tlr.lockValue).
		Int64()
	if err != nil {
		logx.Errorf("Release lock Redis error | key=%s | err=%v", tlr.lockKey, err)
		return false, fmt.Errorf("redis eval release script failed: %w", err)
	}

	if delResult == 1 {
		logx.Debugf("Release lock success | key=%s", tlr.lockKey)
		tlr.locked = false // mark as released
		return true, nil
	} else {
		// Lock expired or held by another owner
		logx.Errorf("Release lock failed: lock expired or not owned | key=%s", tlr.lockKey)
		tlr.locked = false
		return false, nil
	}
}

// Extend renews the lock TTL if still held by this owner.
func (tlr *TryLockResult) Extend(ctx context.Context, extendTTL time.Duration) (bool, error) {
	if !tlr.locked {
		logx.Errorf("Extend lock failed: not holding lock | key=%s", tlr.lockKey)
		return false, nil
	}

	// Lua script: atomically verify owner then extend TTL
	luaScript := `
		local currentValue = redis.call('GET', KEYS[1])
		if currentValue == ARGV[1] then
			return redis.call('EXPIRE', KEYS[1], ARGV[2])
		else
			return 0
		end
	`
	expireResult, err := tlr.redis.
		Eval(ctx, luaScript, []string{tlr.lockKey}, tlr.lockValue, int(extendTTL.Seconds())).
		Int64()
	if err != nil {
		logx.Errorf("Extend lock Redis error | key=%s | err=%v", tlr.lockKey, err)
		return false, fmt.Errorf("redis eval extend script failed: %w", err)
	}

	return expireResult == 1, nil
}
