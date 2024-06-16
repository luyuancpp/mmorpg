package svc

import (
	"db/internal/config"
	"github.com/redis/go-redis/v9"
)

type ServiceContext struct {
	Config config.Config
	Rdb    *redis.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config: c,
		Rdb:    redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
	}
}
