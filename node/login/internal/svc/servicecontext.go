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

var dbConfigFile = flag.String("db_rpc_client", "etc/db_client.json", "the config file")
var deployConfigFile = flag.String("deploy_rpc_client", "etc/db_client.json", "the config file")

type ServiceContext struct {
	Config       config.Config
	Redis        *redis.Client
	DbClient     *zrpc.Client
	DeployClient *zrpc.Client
	CentreClient *centre.Client
	SnowFlake    *snowflake.Node
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*dbConfigFile, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)

	var deployRpc zrpc.RpcClientConf
	conf.MustLoad(*deployConfigFile, &deployRpc)
	deployClient := zrpc.MustNewClient(deployRpc)

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
		DbClient:     &dbClient,
		DeployClient: &deployClient,
		CentreClient: centre.NewCentreClient(config.CentreClientConf.Ip, config.CentreClientConf.Port),
		SnowFlake:    sn,
	}
}
