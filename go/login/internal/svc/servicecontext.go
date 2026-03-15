package svc

import (
	"context"
	"fmt"
	"strconv"

	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	kafkapb "login/contracts/kafka"
	"login/internal/config"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"
	login_proto "login/proto/common"
	"time"
)

type ServiceContext struct {
	RedisClient   *redis.Client
	SnowFlake     *snowflake.Node
	NodeInfo      login_proto.NodeInfo
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

	kafkaClient, err := kafka.NewKeyOrderedKafkaProducer(config.AppConfig.Kafka)

	if err != nil {
		logx.Error(err)
		panic(err)
	}

	monitor, err := kafka.NewExpandMonitor(
		config.AppConfig.Kafka.BootstrapServers, // Kafka broker地址，配置文件中新增
		config.AppConfig.Kafka.Topic,            // 消费者组ID，配置文件中新增
		redisClient,
		kafkaClient,
		1*time.Second,
	)

	if err != nil {
		logx.Error(err)
		panic(err)
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

// KickOldSession sends a KickPlayer command to the old Gate via Kafka.
func (s *ServiceContext) KickOldSession(ctx context.Context, gateID string, gateInstanceID string, sessionID uint64, sessionVersion uint64) {
	cmd := &kafkapb.GateCommand{
		CommandType:      kafkapb.GateCommand_KickPlayer,
		SessionId:        sessionID,
		TargetInstanceId: gateInstanceID,
	}
	gateNodeID, _ := strconv.ParseUint(gateID, 10, 32)
	cmd.TargetGateId = uint32(gateNodeID)

	data, err := proto.Marshal(cmd)
	if err != nil {
		logx.Errorf("KickOldSession marshal failed: %v", err)
		return
	}

	topic := fmt.Sprintf("gate-%s", gateID)
	if err := s.KafkaClient.SendToTopic(topic, data); err != nil {
		logx.Errorf("KickOldSession send to %s failed: %v", topic, err)
	}
}

func (s *ServiceContext) Stop() {
	s.ExpandMonitor.Stop()
}
