package svc

import (
	"context"

	"db/internal/config"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

type ServiceContext struct {
	Config      config.Config
	RedisClient *redis.Client
}

func NewServiceContext() *ServiceContext {
	redisCfg := config.AppConfig.ServerConfig.RedisClient

	redisClient := redis.NewClient(&redis.Options{
		Addr:            redisCfg.Hosts,
		Password:        redisCfg.Password,
		DB:              redisCfg.DB,
		DisableIndentity: true, // suppress CLIENT SETINFO on Redis < 7.2
	})

	if err := redisClient.Ping(context.Background()).Err(); err != nil {
		logx.Errorf("Failed to connect to Redis at %s: %v", redisCfg.Hosts, err)
	}

	return &ServiceContext{
		Config:      config.AppConfig,
		RedisClient: redisClient,
	}
}
