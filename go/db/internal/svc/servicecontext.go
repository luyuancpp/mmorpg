package svc

import (
	"db/internal/config"
	"github.com/redis/go-redis/v9"
)

type ServiceContext struct {
	Config      config.Config
	RedisClient *redis.Client
}

func NewServiceContext() *ServiceContext {
	redisCfg := config.AppConfig.ServerConfig.RedisClient

	return &ServiceContext{
		Config: config.AppConfig,
		RedisClient: redis.NewClient(&redis.Options{
			Addr:     redisCfg.Hosts,
			Password: redisCfg.Password, // 如果有密码
			DB:       redisCfg.DB,       // 默认 0
		}),
	}
}
