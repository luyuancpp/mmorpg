package svc

import (
	"flag"
	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
	clientv3 "go.etcd.io/etcd/client/v3"
	"login/internal/config"
	"login/pb/game"
)

var dbConfigFile = flag.String("db_rpc_client", "etc/db_client.yaml", "the config file")

type ServiceContext struct {
	Config     config.Config
	Redis      *redis.Client
	DbClient   *zrpc.Client
	SnowFlake  *snowflake.Node
	NodeInfo   game.NodeInfo
	EtcdClient *clientv3.Client
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*dbConfigFile, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)
	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints: c.Etcd.Hosts,
	})

	if err != nil {
		logx.Error(err)
	}
	return &ServiceContext{
		Config:     c,
		Redis:      redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DbClient:   &dbClient,
		EtcdClient: etcdClient,
	}
}

func (c *ServiceContext) SetNodeId(nodeId int64) {
	node, err := snowflake.NewNode(nodeId)
	if err != nil {
		return
	}

	snowflake.Epoch = config.SnowFlakeConfig.Epoch
	snowflake.NodeBits = config.SnowFlakeConfig.NodeBits
	snowflake.StepBits = config.SnowFlakeConfig.NodeBits

	c.SnowFlake = node
}
