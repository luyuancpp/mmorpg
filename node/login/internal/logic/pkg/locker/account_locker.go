// login/internal/logic/pkg/locker/account_locker.go

package locker

import (
	"context"
	"fmt"
	"github.com/redis/go-redis/v9"
	"time"
)

type AccountLocker struct {
	redisClient *redis.Client
	lockTTL     time.Duration
}

// 构造器
func NewAccountLocker(redisClient *redis.Client, ttl time.Duration) *AccountLocker {
	return &AccountLocker{
		redisClient: redisClient,
		lockTTL:     ttl,
	}
}

// 构造通用锁 key（例如 account_lock:login:my_account）
func (l *AccountLocker) lockKey(account string, lockType string) string {
	return fmt.Sprintf("account_lock:%s:%s", lockType, account)
}

// 通用加锁方法
func (l *AccountLocker) Acquire(ctx context.Context, account string, lockType string) (bool, error) {
	key := l.lockKey(account, lockType)
	return l.redisClient.SetNX(ctx, key, "1", l.lockTTL).Result()
}

// 通用释放方法
func (l *AccountLocker) Release(ctx context.Context, account string, lockType string) error {
	key := l.lockKey(account, lockType)
	return l.redisClient.Del(ctx, key).Err()
}

// 可选：封装简洁调用
func (l *AccountLocker) AcquireLogin(ctx context.Context, account string) (bool, error) {
	return l.Acquire(ctx, account, "login")
}

func (l *AccountLocker) ReleaseLogin(ctx context.Context, account string) error {
	return l.Release(ctx, account, "login")
}

func (l *AccountLocker) AcquireCreate(ctx context.Context, account string) (bool, error) {
	return l.Acquire(ctx, account, "create")
}

func (l *AccountLocker) ReleaseCreate(ctx context.Context, account string) error {
	return l.Release(ctx, account, "create")
}
