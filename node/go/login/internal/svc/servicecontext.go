package svc

import (
	"flag"
	"github.com/luyuancpp/muduoclient/muduo"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
	"login/client/dbservice/playerdbservice"
	"login/internal/config"
)

var configFileDB = flag.String("dbClient", "etc/dbclient.json", "the config file")

type ServiceContext struct {
	Config          config.Config
	Redis           *redis.Client
	DBClient        *zrpc.Client
	CentreClient    *muduo.Client
	DBPlayerService playerdbservice.PlayerDBService
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*configFileDB, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)

	centreClient, _ := muduo.NewClient(config.CentreClientConf.Ip, config.CentreClientConf.Port)

	return &ServiceContext{
		Config:          c,
		Redis:           redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DBClient:        &dbClient,
		CentreClient:    centreClient,
		DBPlayerService: playerdbservice.NewPlayerDBService(dbClient),
	}
}
