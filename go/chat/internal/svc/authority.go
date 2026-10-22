package svc

import (
	"context"
	"database/sql"
	"errors"
	"os"
	"time"

	"chat/internal/authority"
	"chat/internal/config"
	"shared/authoritysql"

	"github.com/redis/go-redis/v9"
)

func OpenAuthority(ctx context.Context, c config.Config, migrate bool) (*sql.DB, *authority.Store, error) {
	if !c.RequireTiDB {
		return nil, nil, errors.New("聊天权威存储只允许TiDB")
	}
	var db *sql.DB
	var err error
	if migrate {
		db, err = authoritysql.OpenForMigration(ctx, os.Getenv(c.DSNEnv), authority.DatabaseName, true)
	} else {
		db, err = authoritysql.Open(ctx, os.Getenv(c.DSNEnv), authority.DatabaseName, true, c.MaxOpenConns)
	}
	if err != nil {
		return nil, nil, err
	}
	if migrate {
		if err = authority.Migrate(ctx, db); err != nil {
			db.Close()
			return nil, nil, err
		}
		return db, nil, nil
	}
	store, err := authority.New(ctx, db, c.AccountRoleSchema, authority.Rules{MaxContentBytes: c.MaxContentBytes, RateLimitPerSecond: c.RateLimitPerSecond, HistoryMaxEntries: c.HistoryMaxEntries, HistoryTTLSeconds: c.HistoryTTLSeconds, HistoryDefaultLimit: c.HistoryDefaultLimit, HistoryMaxLimit: c.HistoryMaxLimit, SensitiveWords: c.SensitiveWords})
	if err != nil {
		db.Close()
		return nil, nil, err
	}
	return db, store, nil
}

func openLoginSessions(ctx context.Context, c config.Config) (*redis.Client, error) {
	if !c.GlobalLoginSessions || c.LoginRedis.Host == "" || c.LoginRedis.DB < 0 || c.LoginRedis.PoolSize < 1 || c.LoginRedis.PoolSize > 128 {
		return nil, errors.New("聊天全局原登录态依赖缺失")
	}
	r := redis.NewClient(&redis.Options{Addr: c.LoginRedis.Host, Password: os.Getenv(c.LoginRedis.PasswordEnv), DB: c.LoginRedis.DB, PoolSize: c.LoginRedis.PoolSize, MaxActiveConns: c.LoginRedis.PoolSize, MaxRetries: -1, DialerRetries: 1, ContextTimeoutEnabled: true, DialTimeout: time.Second, ReadTimeout: time.Second, WriteTimeout: time.Second, PoolTimeout: time.Second, DisableIndentity: true})
	if err := r.Ping(ctx).Err(); err != nil {
		r.Close()
		return nil, errors.New("聊天全局原登录态不可用")
	}
	return r, nil
}
