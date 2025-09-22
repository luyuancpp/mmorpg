package svc

import (
	"context"
	"fmt"
	"github.com/bwmarrin/snowflake"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"login/internal/config"
	"login/internal/logic/pkg/centre"
	"login/internal/logic/pkg/taskmanager"
	login_proto "login/proto/common"
	"sync/atomic"
)

type ServiceContext struct {
	RedisClient *redis.Client
	SnowFlake   *snowflake.Node
	NodeInfo    login_proto.NodeInfo
	// 使用 atomic.Value 安全存储 CentreClient
	centreClient atomic.Value // 类型为 *centre.CentreClient
	AsynqClient  *asynq.Client
	TaskExecutor *taskmanager.TaskExecutor
}

func NewServiceContext() *ServiceContext {
	ctx := context.Background()

	// 初始化 Redis 客户端配置
	redisHost := config.AppConfig.Node.RedisClient.Host
	redisPassword := config.AppConfig.Node.RedisClient.Password
	redisDB := int(config.AppConfig.Node.RedisClient.DB)

	redisOpt := asynq.RedisClientOpt{
		Addr:     redisHost,
		Password: redisPassword,
		DB:       redisDB,
	}

	redisClient := redis.NewClient(&redis.Options{
		Addr:     redisHost,
		Password: redisPassword,
		DB:       redisDB,
	})

	if err := redisClient.Ping(ctx).Err(); err != nil {
		panic(fmt.Errorf("failed to connect Redis: %w", err))
	}

	// 初始化 TaskExecutor
	taskExecutor, err := taskmanager.NewTaskExecutor(100, redisClient)
	if err != nil {
		panic(fmt.Errorf("failed to init TaskExecutor: %w", err))
	}

	// 返回 ServiceContext 实例
	return &ServiceContext{
		RedisClient:  redisClient,
		AsynqClient:  asynq.NewClient(redisOpt),
		TaskExecutor: taskExecutor,
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
