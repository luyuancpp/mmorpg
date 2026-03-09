package svc

import (
	"sync"

	"scene_manager/internal/config"

	"github.com/zeromicro/go-zero/core/stores/redis"
)

type ServiceContext struct {
	Config      config.Config
	Redis       *redis.Redis
	GateStreams sync.Map // map[string]scene_manager.SceneManager_GateConnectServer
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config:      c,
		Redis:       redis.MustNewRedis(c.Redis),
		GateStreams: sync.Map{},
	}
}
