package svc

import (
	"github.com/redis/go-redis/v9"
	"player_locator/internal/config"
)

type ServiceContext struct {
	Config       config.Config
	RedisCluster *redis.ClusterClient
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config:       c,
		RedisCluster: NewRedisClusterClient(c.Node.Redis),
	}
}

func NewRedisClusterClient(cfg config.RedisClusterConf) *redis.ClusterClient {
	return redis.NewClusterClient(&redis.ClusterOptions{
		Addrs:    cfg.Cluster.Hosts,
		Password: cfg.Cluster.Pass,
	})
}
