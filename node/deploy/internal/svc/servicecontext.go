package svc

import (
	"deploy/internal/config"
	"github.com/redis/go-redis/v9"
)

type ServiceContext struct {
	Config config.Config
	Redis  *redis.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config: c,
		Redis:  redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
	}
}
