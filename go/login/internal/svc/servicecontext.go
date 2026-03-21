package svc

import (
	"context"
	"fmt"
	"strconv"

	"github.com/bwmarrin/snowflake"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/protobuf/proto"

	kafkapb "login/contracts/kafka"
	game "login/generated/pb/game"
	"login/internal/config"
	"login/internal/kafka"
	"login/internal/logic/pkg/taskmanager"
	login_proto "login/proto/common/base"
	plpb "login/proto/player_locator"
	"time"
)

type ServiceContext struct {
	RedisClient         *redis.Client
	SnowFlake           *snowflake.Node
	NodeInfo            login_proto.NodeInfo
	KafkaClient         *kafka.KeyOrderedKafkaProducer
	TaskExecutor        *taskmanager.TaskExecutor
	ExpandMonitor       *kafka.ExpandMonitor
	PlayerLocatorClient plpb.PlayerLocatorClient
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

	// 初始化 player_locator gRPC 客户端 (通过 etcd 发现)
	plConn := zrpc.MustNewClient(config.AppConfig.PlayerLocatorRpc)
	plClient := plpb.NewPlayerLocatorClient(plConn.Conn())

	// 返回 ServiceContext 实例
	return &ServiceContext{
		RedisClient:         redisClient,
		KafkaClient:         kafkaClient,
		TaskExecutor:        taskExecutor,
		ExpandMonitor:       monitor,
		PlayerLocatorClient: plClient,
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

// KickSessionOnGate sends a KickPlayer command to the target Gate via Kafka.
func (s *ServiceContext) KickSessionOnGate(gateID string, gateInstanceID string, sessionID uint64) error {
	eventId := uint32(game.ContractsKafkaKickPlayerEventEventId)

	cmd := &kafkapb.GateCommand{
		SessionId:        sessionID,
		TargetInstanceId: gateInstanceID,
		EventId:          &eventId,
	}

	gateNodeID, err := strconv.ParseUint(gateID, 10, 32)
	if err != nil {
		return fmt.Errorf("parse gate id %q: %w", gateID, err)
	}
	cmd.TargetGateId = uint32(gateNodeID)

	data, err := proto.Marshal(cmd)
	if err != nil {
		return fmt.Errorf("marshal gate kick command: %w", err)
	}

	topic := fmt.Sprintf("gate-%s", gateID)
	if err := s.KafkaClient.SendToTopic(topic, data); err != nil {
		return fmt.Errorf("send gate kick command to %s: %w", topic, err)
	}

	return nil
}

func (s *ServiceContext) Stop() {
	s.ExpandMonitor.Stop()
}
