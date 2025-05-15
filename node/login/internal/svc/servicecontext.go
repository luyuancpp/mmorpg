package svc

import (
	"flag"
	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
)

var dbConfigFile = flag.String("db_rpc_client", "etc/db_client.yaml", "the config file")

type ServiceContext struct {
	Config       config.Config
	Redis        *redis.Client
	DbClient     *zrpc.Client
	CentreClient *centre.Client
	SnowFlake    *snowflake.Node
	NodeLeaseID  uint64
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*dbConfigFile, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)

	return &ServiceContext{
		Config:   c,
		Redis:    redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DbClient: &dbClient,
		//CentreClient: centre.NewCentreClient(config.CentreClientConf.Ip, config.CentreClientConf.Port),
	}
}

func (c *ServiceContext) SetNodeId(nodeId uint64) {
	node, err := snowflake.NewNode(int64(nodeId))
	if err != nil {
		return
	}

	snowflake.Epoch = config.SnowFlakeConfig.Epoch
	snowflake.NodeBits = config.SnowFlakeConfig.NodeBits
	snowflake.StepBits = config.SnowFlakeConfig.NodeBits

	c.SnowFlake = node
}
