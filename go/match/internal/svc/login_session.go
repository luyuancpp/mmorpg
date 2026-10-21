package svc

import (
	"errors"
	"os"
	"strings"
	"time"

	"github.com/redis/go-redis/v9"
	"match/internal/config"
)

// 与login同一全局命名空间，独立只读池，不回落到匹配缓存或zone在线投影。
func NewLoginSessionClient(c config.Config) (*redis.Client, error) {
	if !c.GlobalLoginSessions || strings.TrimSpace(c.LoginRedis.Host) == "" || c.LoginRedis.DB < 0 || c.LoginRedis.PoolSize < 1 || c.LoginRedis.PoolSize > 128 {
		return nil, errors.New("匹配必须显式配置全局login Redis实例/DB/池上限")
	}
	return redis.NewClient(&redis.Options{Addr: c.LoginRedis.Host, Password: os.Getenv(c.LoginRedis.PasswordEnv), DB: c.LoginRedis.DB,
		PoolSize: c.LoginRedis.PoolSize, MaxActiveConns: c.LoginRedis.PoolSize, MaxRetries: -1, DialerRetries: 1,
		DialTimeout: time.Second, ReadTimeout: 300 * time.Millisecond, WriteTimeout: 300 * time.Millisecond, PoolTimeout: 300 * time.Millisecond,
		ContextTimeoutEnabled: true}), nil
}
