package svc

import (
	"flag"
	"github.com/luyuancpp/muduoclient/muduo"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
	"login/internal/config"
)

var configFileDB = flag.String("dbClient", "etc/dbclient.json", "the config file")

type ServiceContext struct {
	Config       config.Config
	Rdb          *redis.Client
	DBClient     *zrpc.Client
	CentreClient *muduo.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*configFileDB, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)

	centreClient, _ := muduo.NewClient(config.CentreClientConf.Ip, config.CentreClientConf.Port)

	return &ServiceContext{
		Config:       c,
		Rdb:          redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DBClient:     &dbClient,
		CentreClient: centreClient,
	}
}
