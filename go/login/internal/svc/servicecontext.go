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

	game "login/generated/pb/game"
	"login/internal/config"
	"login/internal/kafka"
	"login/internal/logic/pkg/node"
	"login/internal/logic/pkg/taskmanager"
	smpb "login/proto/scene_manager"
	login_proto "proto/common/base"
	kafkapb "proto/contracts/kafka"
	plpb "proto/player_locator"
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
	SceneManagerClient  smpb.SceneManagerClient
	GateWatcher         *node.NodeWatcher
}

func NewServiceContext() *ServiceContext {
	ctx := context.Background()

	// Initialize Redis client
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
		config.AppConfig.Kafka.Brokers, // Kafka broker addresses
		config.AppConfig.Kafka.Topic,   // Consumer group ID
		redisClient,
		kafkaClient,
		1*time.Second,
	)

	if err != nil {
		logx.Error(err)
		panic(err)
	}

	// Initialize TaskExecutor
	taskExecutor, err := taskmanager.NewTaskExecutor(100, redisClient)
	if err != nil {
		panic(fmt.Errorf("failed to init TaskExecutor: %w", err))
	}

	// Initialize player_locator gRPC client (discovered via etcd)
	plConn := zrpc.MustNewClient(config.AppConfig.PlayerLocatorRpc)
	plClient := plpb.NewPlayerLocatorClient(plConn.Conn())

	// Initialize scene_manager gRPC client (discovered via etcd)
	smConn := zrpc.MustNewClient(config.AppConfig.SceneManagerRpc)
	smClient := smpb.NewSceneManagerClient(smConn.Conn())

	// Initialize GateWatcher for gate node discovery (load balancing)
	gateNodeType := uint32(login_proto.ENodeType_GateNodeService)
	gatePrefix := node.BuildRpcPrefix(
		node.GetRpcPrefix(gateNodeType),
		config.AppConfig.Node.ZoneId,
		gateNodeType,
	)
	etcdClient, err := node.NewEtcdClient()
	if err != nil {
		panic(fmt.Errorf("failed to create etcd client for GateWatcher: %w", err))
	}
	gateWatcher := node.NewNodeWatcher(etcdClient, gatePrefix)

	return &ServiceContext{
		RedisClient:         redisClient,
		KafkaClient:         kafkaClient,
		TaskExecutor:        taskExecutor,
		ExpandMonitor:       monitor,
		PlayerLocatorClient: plClient,
		SceneManagerClient:  smClient,
		GateWatcher:         gateWatcher,
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

// SendBindSessionToGate sends a BindSession command to the target Gate via Kafka.
// This replaces Centre's BindSessionToGate RPC.
func (s *ServiceContext) SendBindSessionToGate(gateID string, gateInstanceID string,
	sessionID uint64, playerID uint64, sessionVersion uint64, enterGsType uint32) error {

	eventId := uint32(game.ContractsKafkaBindSessionEventEventId)

	cmd := &kafkapb.GateCommand{
		SessionId:        sessionID,
		PlayerId:         playerID,
		TargetInstanceId: gateInstanceID,
		EventId:          eventId,
		EnterGsType:      enterGsType,
	}

	gateNodeID, err := strconv.ParseUint(gateID, 10, 32)
	if err != nil {
		return fmt.Errorf("parse gate id %q: %w", gateID, err)
	}
	cmd.TargetGateId = uint32(gateNodeID)

	data, err := proto.Marshal(cmd)
	if err != nil {
		return fmt.Errorf("marshal gate bind session command: %w", err)
	}

	topic := fmt.Sprintf("gate-%s", gateID)
	if err := s.KafkaClient.SendToTopic(topic, data); err != nil {
		return fmt.Errorf("send gate bind session command to %s: %w", topic, err)
	}

	return nil
}

// KickSessionOnGate sends a KickPlayer command to the target Gate via Kafka.
func (s *ServiceContext) KickSessionOnGate(gateID string, gateInstanceID string, sessionID uint64) error {
	eventId := uint32(game.ContractsKafkaKickPlayerEventEventId)

	cmd := &kafkapb.GateCommand{
		SessionId:        sessionID,
		TargetInstanceId: gateInstanceID,
		EventId:          eventId,
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
