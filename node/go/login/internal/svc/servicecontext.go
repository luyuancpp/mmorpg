package svc

import (
	"flag"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
	"login/internal/config"
)

var configFile = flag.String("dbClient", "etc/dbconfig.json", "the config file")

type ServiceContext struct {
	Config config.Config
	Rdb    *redis.Client
	DBCli  *zrpc.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	var cRpc zrpc.RpcClientConf
	conf.MustLoad(*configFile, &cRpc)
	client := zrpc.MustNewClient(cRpc)

	return &ServiceContext{
		Config: c,
		Rdb:    redis.NewClient(&redis.Options{Addr: "127.0.0.1:6379", DB: 0}),
		DBCli:  &client,
	}
}
