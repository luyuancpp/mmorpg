package svc

import (
	"flag"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
	"login/internal/config"
)

var configFile = flag.String("dbClient", "etc/database.json", "the config file")

type ServiceContext struct {
	Config   config.Config
	Rdb      *redis.Client
	DBClient *zrpc.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	var cRpc zrpc.RpcClientConf
	conf.MustLoad(*configFile, &cRpc)
	client := zrpc.MustNewClient(cRpc)

	return &ServiceContext{
		Config:   c,
		Rdb:      redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DBClient: &client,
	}
}
