package svc

import (
	"flag"
	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
	"login/pb/game"
	"sync/atomic"
)

var dbConfigFile = flag.String("db_rpc_client", "etc/db_client.yaml", "the config file")

type ServiceContext struct {
	Config    config.Config
	Redis     *redis.Client
	DbClient  *zrpc.Client
	SnowFlake *snowflake.Node
	NodeInfo  game.NodeInfo
	// 使用 atomic.Value 安全存储 CentreClient
	centreClient atomic.Value // 类型为 *centre.CentreClient
}

func NewServiceContext(c config.Config) *ServiceContext {
	var dbRpc zrpc.RpcClientConf
	conf.MustLoad(*dbConfigFile, &dbRpc)
	dbClient := zrpc.MustNewClient(dbRpc)

	return &ServiceContext{
		Config:   c,
		Redis:    redis.NewClient(&redis.Options{Addr: config.RedisConfig.Addr}),
		DbClient: &dbClient,
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

// SetCentreClient 设置中心节点客户端
func (s *ServiceContext) SetCentreClient(c *centre.Client) {
	s.centreClient.Store(c)
}

// GetCentreClient 获取当前中心节点客户端
func (s *ServiceContext) GetCentreClient() *centre.Client {
	value := s.centreClient.Load()
	if value == nil {
		return nil
	}
	return value.(*centre.Client)
}
