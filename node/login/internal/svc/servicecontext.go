package svc

import (
	"github.com/bwmarrin/snowflake"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
	"login/pb/game"
	"sync/atomic"
)

type ServiceContext struct {
	RedisClient *redis.Client
	SnowFlake   *snowflake.Node
	NodeInfo    game.NodeInfo
	// 使用 atomic.Value 安全存储 CentreClient
	centreClient atomic.Value // 类型为 *centre.CentreClient
	AsynqClient  *asynq.Client
}

func NewServiceContext() *ServiceContext {
	return &ServiceContext{
		RedisClient: redis.NewClient(&redis.Options{Addr: config.AppConfig.Node.RedisClient.Host}),
		AsynqClient: asynq.NewClient(asynq.RedisClientOpt{
			Addr:     config.AppConfig.Node.RedisClient.Host,
			Password: config.AppConfig.Node.RedisClient.Password,
			DB:       int(config.AppConfig.Node.RedisClient.DB),
		}),
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
