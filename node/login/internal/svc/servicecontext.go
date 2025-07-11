package svc

import (
	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/zrpc"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
	"login/pb/game"
	"sync/atomic"
)

type ServiceContext struct {
	Redis     *redis.Client
	DbClient  *zrpc.Client
	SnowFlake *snowflake.Node
	NodeInfo  game.NodeInfo
	// 使用 atomic.Value 安全存储 CentreClient
	centreClient atomic.Value // 类型为 *centre.CentreClient
}

func NewServiceContext() *ServiceContext {
	dbClient := zrpc.MustNewClient(config.AppConfig.DbClient)
	return &ServiceContext{
		Redis:    redis.NewClient(&redis.Options{Addr: config.AppConfig.Node.Redis.Host}),
		DbClient: &dbClient,
	}
}

func (c *ServiceContext) SetNodeId(nodeId int64) {
	node, err := snowflake.NewNode(nodeId)
	if err != nil {
		return
	}

	snowflake.Epoch = config.AppConfig.Snowflake.Epoch
	snowflake.NodeBits = uint8(config.AppConfig.Snowflake.NodeBits)
	snowflake.StepBits = uint8(config.AppConfig.Snowflake.StepBits)

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
