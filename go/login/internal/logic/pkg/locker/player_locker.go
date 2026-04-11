package locker

import (
	"context"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

type RedisLocker struct {
	client *redis.Client
}

func NewRedisLocker(client *redis.Client) *RedisLocker {
	return &RedisLocker{client: client}
}

// TryLockWithRetry attempts to acquire a lock with retries.
// Returns the Lock and nil on success, or nil and an error if all retries fail.
func (l *RedisLocker) TryLockWithRetry(ctx context.Context, key string, ttl time.Duration, maxRetries int, retryInterval time.Duration) (*Lock, error) {
	for attempt := 1; attempt <= maxRetries; attempt++ {
		lock, err := l.TryLock(ctx, key, ttl)
		if err == nil && lock.IsLocked() {
			return lock, nil
		}
		if attempt < maxRetries {
			logx.Infof("lock retry %d/%d for key=%s (err=%v)", attempt, maxRetries, key, err)
			time.Sleep(retryInterval)
		}
	}
	return nil, fmt.Errorf("failed to acquire lock %s after %d retries", key, maxRetries)
}

// Lock represents a Redis-based distributed lock
type Lock struct {
	Key    string
	Value  string
	TTL    time.Duration
	locked bool
	client *redis.Client
}

// TryLock tries to acquire the lock
func (l *RedisLocker) TryLock(ctx context.Context, key string, ttl time.Duration) (*Lock, error) {
	lockValue := uuid.NewString()
	ok, err := l.client.SetNX(ctx, key, lockValue, ttl).Result()
	if err != nil {
		return nil, err
	}

	return &Lock{
		Key:    key,
		Value:  lockValue,
		TTL:    ttl,
		locked: ok,
		client: l.client,
	}, nil
}

// IsLocked returns whether the lock was successfully acquired
func (l *Lock) IsLocked() bool {
	return l.locked
}

// Release releases the lock if the current value matches (safe delete)
func (l *Lock) Release(ctx context.Context) (bool, error) {
	const luaScript = `
if redis.call("get", KEYS[1]) == ARGV[1] then
	return redis.call("del", KEYS[1])
else
	return 0
end
`
	res, err := l.client.Eval(ctx, luaScript, []string{l.Key}, l.Value).Result()
	if err != nil {
		return false, err
	}

	return res.(int64) == 1, nil
}
