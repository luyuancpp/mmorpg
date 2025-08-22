package locker

import (
	"context"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
)

type RedisLocker struct {
	client *redis.Client
}

func NewRedisLocker(client *redis.Client) *RedisLocker {
	return &RedisLocker{client: client}
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
