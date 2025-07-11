package locker

import (
	"context"
	"fmt"
	"github.com/redis/go-redis/v9"
	"time"
)

type PlayerLocker struct {
	redisClient *redis.Client
	lockTTL     time.Duration
}

func NewPlayerLocker(redisClient *redis.Client, ttl time.Duration) *PlayerLocker {
	return &PlayerLocker{
		redisClient: redisClient,
		lockTTL:     ttl,
	}
}

func (l *PlayerLocker) lockKey(playerId uint64) string {
	return fmt.Sprintf("player_lock:%d", playerId)
}

func (l *PlayerLocker) Acquire(ctx context.Context, playerId uint64) (bool, error) {
	key := l.lockKey(playerId)
	return l.redisClient.SetNX(ctx, key, "1", l.lockTTL).Result()
}

func (l *PlayerLocker) Release(ctx context.Context, playerId uint64) error {
	key := l.lockKey(playerId)
	return l.redisClient.Del(ctx, key).Err()
}
