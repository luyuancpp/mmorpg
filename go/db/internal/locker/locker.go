package locker

import (
	"context"
	"fmt"
	"time"

	"github.com/google/uuid" // 用于生成唯一锁标识，避免误释放他人锁
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// RedisLocker 基于Redis的分布式锁实现
type RedisLocker struct {
	redisClient redis.Cmdable // Redis客户端（支持各种Redis操作）
	prefix      string        // 锁key前缀，避免不同业务锁key冲突
}

// TryLockResult 锁获取结果：封装“是否成功”“锁标识”“释放方法”
type TryLockResult struct {
	locked    bool          // 是否成功获取锁
	lockKey   string        // 锁的Redis Key
	lockValue string        // 锁的唯一标识（避免误释放）
	redis     redis.Cmdable // Redis客户端（用于释放锁）
}

// NewRedisLocker 创建Redis分布式锁实例
// 参数说明：
// - redisClient：Redis客户端（如 redis.NewClient 生成的实例）
// - prefix：可选，锁key前缀（如“login:lock:”），默认“distributed:lock:”
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

// TryLock 非阻塞获取分布式锁
// 参数说明：
// - ctx：上下文（用于超时控制、取消操作）
// - key：业务key（如“player:123”，将与prefix拼接成最终锁key）
// - ttl：锁自动过期时间（避免死锁，建议大于业务处理耗时）
// 返回值：TryLockResult（含释放锁方法）、error（Redis操作异常）
func (rl *RedisLocker) TryLock(ctx context.Context, key string, ttl time.Duration) (*TryLockResult, error) {
	// 1. 构造最终锁key（前缀+业务key，避免冲突）
	finalLockKey := rl.prefix + key
	// 2. 生成唯一锁标识（UUID，用于释放时验证“是否是自己的锁”）
	lockValue := uuid.NewString()

	// 3. 核心：用Redis SET NX EX命令获取锁（原子操作）
	// - NX：只有key不存在时才设置成功（保证互斥）
	// - EX：设置过期时间（避免死锁）
	setCmd := rl.redisClient.SetNX(
		ctx,
		finalLockKey,
		lockValue,
		ttl,
	)
	success, err := setCmd.Result()
	if err != nil {
		// Redis操作异常（如网络问题），返回错误
		logx.Errorf("Redis TryLock failed | key=%s | err=%v", finalLockKey, err)
		return &TryLockResult{locked: false}, fmt.Errorf("redis setnx failed: %w", err)
	}

	// 4. 返回锁结果（成功则携带锁标识，失败则locked=false）
	return &TryLockResult{
		locked:    success,
		lockKey:   finalLockKey,
		lockValue: lockValue,
		redis:     rl.redisClient,
	}, nil
}

// IsLocked 判断是否成功获取锁（适配之前代码中 tryLocker.IsLocked() 调用）
func (tlr *TryLockResult) IsLocked() bool {
	return tlr.locked
}

// Release 释放分布式锁（支持自动判断“是否是自己的锁”，避免误释放）
// 返回值：bool（是否成功释放）、error（Redis操作异常）
func (tlr *TryLockResult) Release(ctx context.Context) (bool, error) {
	// 1. 未获取锁，直接返回失败
	if !tlr.locked {
		logx.Errorf("Release lock failed: not holding lock | key=%s", tlr.lockKey)
		return false, nil
	}

	// 2. 用Lua脚本释放锁（原子操作：先验证锁标识，再删除，避免误释放）
	// Lua脚本逻辑：
	// - 1. 获取锁当前值，判断是否等于自己的lockValue
	// - 2. 等于则删除锁（释放成功），不等于则忽略（不是自己的锁）
	luaScript := `
		local currentValue = redis.call('GET', KEYS[1])
		if currentValue == ARGV[1] then
			return redis.call('DEL', KEYS[1])
		else
			return 0
		end
	`
	// 执行Lua脚本：KEYS[1] = 锁key，ARGV[1] = 锁标识
	delResult, err := tlr.redis.
		Eval(ctx, luaScript, []string{tlr.lockKey}, tlr.lockValue).
		Int64()
	if err != nil {
		// Redis操作异常（如网络问题）
		logx.Errorf("Release lock Redis error | key=%s | err=%v", tlr.lockKey, err)
		return false, fmt.Errorf("redis eval release script failed: %w", err)
	}

	// 3. 解析结果：delResult=1 → 释放成功；delResult=0 → 锁已过期或不是自己的
	if delResult == 1 {
		logx.Debugf("Release lock success | key=%s", tlr.lockKey)
		tlr.locked = false // 释放后标记为未持有锁，避免重复释放
		return true, nil
	} else {
		// 锁已过期（被Redis自动删除）或被他人持有，释放失败
		logx.Errorf("Release lock failed: lock expired or not owned | key=%s", tlr.lockKey)
		tlr.locked = false
		return false, nil
	}
}

// Extend 延长锁过期时间（可选，用于长耗时业务避免锁提前过期）
// 参数说明：
// - ctx：上下文
// - extendTTL：延长的过期时间（如 3*time.Second）
// 返回值：bool（是否延长成功）、error（Redis操作异常）
func (tlr *TryLockResult) Extend(ctx context.Context, extendTTL time.Duration) (bool, error) {
	// 1. 未获取锁，直接返回失败
	if !tlr.locked {
		logx.Errorf("Extend lock failed: not holding lock | key=%s", tlr.lockKey)
		return false, nil
	}

	// 2. 用Lua脚本延长锁（先验证标识，再延长，避免延长他人锁）
	luaScript := `
		local currentValue = redis.call('GET', KEYS[1])
		if currentValue == ARGV[1] then
			return redis.call('EXPIRE', KEYS[1], ARGV[2])
		else
			return 0
		end
	`
	// 执行Lua脚本：ARGV[2] = 延长的过期时间（秒）
	expireResult, err := tlr.redis.
		Eval(ctx, luaScript, []string{tlr.lockKey}, tlr.lockValue, int(extendTTL.Seconds())).
		Int64()
	if err != nil {
		logx.Errorf("Extend lock Redis error | key=%s | err=%v", tlr.lockKey, err)
		return false, fmt.Errorf("redis eval extend script failed: %w", err)
	}

	return expireResult == 1, nil
}
