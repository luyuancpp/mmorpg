package cache

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
)

// singleflightGroup provides in-process dedup without requiring golang.org/x/sync.
var (
	sfMu      sync.Mutex
	sfCalls   = map[string]*sfCall{}
)

type sfCall struct {
	wg  sync.WaitGroup
	val any
	err error
}

func singleflightDo(key string, fn func() (any, error)) (any, error) {
	sfMu.Lock()
	if c, ok := sfCalls[key]; ok {
		sfMu.Unlock()
		c.wg.Wait()
		return c.val, c.err
	}
	c := &sfCall{}
	c.wg.Add(1)
	sfCalls[key] = c
	sfMu.Unlock()

	c.val, c.err = fn()
	c.wg.Done()

	sfMu.Lock()
	delete(sfCalls, key)
	sfMu.Unlock()

	return c.val, c.err
}

// LoadOrCache implements the cache-aside pattern with in-process dedup.
// It tries Redis first, falls back to dbLoader on cache miss, and writes
// the result back to Redis with the given TTL.
func LoadOrCache[T any](
	ctx context.Context,
	rdb *redis.Client,
	cacheKey string,
	sfKey string,
	ttl time.Duration,
	dbLoader func(ctx context.Context) (T, error),
) (T, error) {
	var zero T

	// 1. Try Redis
	data, err := rdb.Get(ctx, cacheKey).Bytes()
	if err == nil {
		var result T
		if err := json.Unmarshal(data, &result); err != nil {
			return zero, fmt.Errorf("cache unmarshal %s: %w", cacheKey, err)
		}
		return result, nil
	}
	if err != redis.Nil {
		return zero, fmt.Errorf("redis get %s: %w", cacheKey, err)
	}

	// 2. Cache miss -> singleflight -> DB
	raw, err := singleflightDo(sfKey, func() (any, error) {
		value, err := dbLoader(ctx)
		if err != nil {
			return nil, err
		}
		// Write back to cache
		if bs, jsonErr := json.Marshal(value); jsonErr == nil {
			rdb.Set(ctx, cacheKey, bs, ttl)
		}
		return value, nil
	})
	if err != nil {
		return zero, err
	}
	return raw.(T), nil
}
