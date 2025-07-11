package locker

import (
	"context"
	"fmt"
	"github.com/redis/go-redis/v9"
	"time"
)

type LoginLocker struct {
	redisClient *redis.Client
	lockTTL     time.Duration
}

func NewLoginLocker(redisClient *redis.Client, ttl time.Duration) *LoginLocker {
	return &LoginLocker{
		redisClient: redisClient,
		lockTTL:     ttl,
	}
}

func (l *LoginLocker) lockKey(account string) string {
	return fmt.Sprintf("login_lock:%s", account)
}

// Acquire 尝试获取登录锁
func (l *LoginLocker) Acquire(ctx context.Context, account string) (bool, error) {
	key := l.lockKey(account)
	ok, err := l.redisClient.SetNX(ctx, key, "1", l.lockTTL).Result()
	return ok, err
}

// Release 释放登录锁
func (l *LoginLocker) Release(ctx context.Context, account string) error {
	key := l.lockKey(account)
	return l.redisClient.Del(ctx, key).Err()
}
