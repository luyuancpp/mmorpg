package svc

import (
	"db/internal/config"
	"github.com/redis/go-redis/v9"
)

type ServiceContext struct {
	Config config.Config
	Redis  *redis.Client
}

func NewServiceContext() *ServiceContext {
	return &ServiceContext{
		Config: config.AppConfig,
		Redis:  redis.NewClient(&redis.Options{Addr: config.AppConfig.ServerConfig.Redis.Hosts}),
	}
}
