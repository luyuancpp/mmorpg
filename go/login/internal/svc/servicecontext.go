package svc

import (
	"context"
	"fmt"
	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"login/internal/config"
	"login/internal/kafka"
	"login/internal/logic/pkg/centre"
	"login/internal/logic/pkg/taskmanager"
	login_proto "login/proto/common"
	"sync/atomic"
	"time"
)

type ServiceContext struct {
	RedisClient *redis.Client
	SnowFlake   *snowflake.Node
	NodeInfo    login_proto.NodeInfo
	// 使用 atomic.Value 安全存储 CentreClient
	centreClient  atomic.Value // 类型为 *centre.CentreClient
	KafkaClient   *kafka.KeyOrderedKafkaProducer
	TaskExecutor  *taskmanager.TaskExecutor
	ExpandMonitor *kafka.ExpandMonitor
}

func NewServiceContext() *ServiceContext {
	ctx := context.Background()

	// 初始化 Redis 客户端配置
	redisHost := config.AppConfig.Node.RedisClient.Host
	redisPassword := config.AppConfig.Node.RedisClient.Password
	redisDB := int(config.AppConfig.Node.RedisClient.DB)

	redisClient := redis.NewClient(&redis.Options{
		Addr:     redisHost,
		Password: redisPassword,
		DB:       redisDB,
	})

	if err := redisClient.Ping(ctx).Err(); err != nil {
		panic(fmt.Errorf("failed to connect Redis: %w", err))
	}

	kafkaClient, err := kafka.NewKeyOrderedKafkaProducer(
		config.AppConfig.Kafka.Brokers,           // Kafka broker地址，配置文件中新增
		config.AppConfig.Kafka.Topic,             // 消费者组ID，配置文件中新增
		int(config.AppConfig.Kafka.PartitionCnt), // 分区数，与原分片数保持一致
	)

	if err != nil {
		logx.Error(err)
		panic(err)
		return nil
	}

	monitor, err := kafka.NewExpandMonitor(
		config.AppConfig.Kafka.Brokers, // Kafka broker地址，配置文件中新增
		config.AppConfig.Kafka.Topic,   // 消费者组ID，配置文件中新增
		redisClient,
		kafkaClient,
		1*time.Second,
	)

	if err != nil {
		logx.Error(err)
		panic(err)
		return nil
	}

	// 初始化 TaskExecutor
	taskExecutor, err := taskmanager.NewTaskExecutor(100, redisClient)
	if err != nil {
		panic(fmt.Errorf("failed to init TaskExecutor: %w", err))
	}

	// 返回 ServiceContext 实例
	return &ServiceContext{
		RedisClient:   redisClient,
		KafkaClient:   kafkaClient,
		TaskExecutor:  taskExecutor,
		ExpandMonitor: monitor,
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

func (s *ServiceContext) Stop() {
	s.ExpandMonitor.Stop()
}
