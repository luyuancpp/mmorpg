package svc

import (
	"context"
	"db_server/internal/config"
	"github.com/redis/go-redis/v9"
)

type ServiceContext struct {
	Config config.Config
	Rdb    *redis.Client
	Ctx    context.Context
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config: c,
		Rdb: redis.NewClient(&redis.Options{
			Addr:     "127.0.0.1:6379",
			PoolSize: 10,
			Ctx:      context.Background(),
		}),
	}
}
