package svc

import (
	"db_server/internal/config"
	"github.com/redis/go-redis/v9"
)

type ServiceContext struct {
	Config config.Config
	Rdb    *redis.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config: c,
		Rdb: redis.NewClient(&redis.Options{
			Addr: "127.0.0.1:6379",
		}),
	}
}
