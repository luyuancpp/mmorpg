package svc

import (
	"deploy/internal/config"
	node_id_etcd "deploy/internal/logic/pkg/node_id_etcd"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
)

type ServiceContext struct {
	Config         config.Config
	Redis          *redis.Client
	NodeEtcdClient *clientv3.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	etcdClient, err := node_id_etcd.InitEtcdClient()
	if err != nil {
		logx.Error("Error initializing Etcd client: ", err)
	}
	return &ServiceContext{
		Config:         c,
		Redis:          redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		NodeEtcdClient: etcdClient,
	}
}
