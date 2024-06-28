package svc

import (
	"flag"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
	"login/client/dbservice/playercentredbservice"
	"login/client/dbservice/playerdbservice"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
)

var configFileDB = flag.String("dbClient", "etc/dbclient.json", "the config file")

type ServiceContext struct {
	Config                config.Config
	Redis                 *redis.Client
	DBClient              *zrpc.Client
	CentreClient          *centre.Client
	DBPlayerService       playerdbservice.PlayerDBService
	DBPlayerCentreService playercentredbservice.PlayerCentreDBService
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*configFileDB, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)

	return &ServiceContext{
		Config:                c,
		Redis:                 redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DBClient:              &dbClient,
		CentreClient:          centre.NewCentreClient(config.CentreClientConf.Ip, config.CentreClientConf.Port),
		DBPlayerService:       playerdbservice.NewPlayerDBService(dbClient),
		DBPlayerCentreService: playercentredbservice.NewPlayerCentreDBService(dbClient),
	}
}
