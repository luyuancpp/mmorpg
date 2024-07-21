package svc

import (
	"flag"
	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
)

var configFileDB = flag.String("dbClient", "etc/dbclient.json", "the config file")

type ServiceContext struct {
	Config       config.Config
	Redis        *redis.Client
	DBClient     *zrpc.Client
	CentreClient *centre.Client
	SnowFlake    *snowflake.Node
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*configFileDB, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)
	snowflake.Epoch = config.SnowFlakeConfig.Epoch
	snowflake.NodeBits = config.SnowFlakeConfig.NodeBits
	snowflake.StepBits = config.SnowFlakeConfig.NodeBits
	sn, err := snowflake.NewNode(0)
	if err != nil {
		logx.Error(err)
		return nil
	}
	return &ServiceContext{
		Config:       c,
		Redis:        redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DBClient:     &dbClient,
		CentreClient: centre.NewCentreClient(config.CentreClientConf.Ip, config.CentreClientConf.Port),
		SnowFlake:    sn,
	}
}
