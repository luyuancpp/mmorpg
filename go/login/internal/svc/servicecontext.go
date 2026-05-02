package svc

import (
	"context"
	"fmt"
	"strconv"

	"github.com/bwmarrin/snowflake"
	"github.com/panjf2000/ants/v2"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/protobuf/proto"

	game "login/generated/pb/game"
	"login/internal/config"
	"login/internal/kafka"
	"login/internal/logic/pkg/node"
	"login/internal/logic/pkg/token"
	login_proto "proto/common/base"
	kafkapb "proto/contracts/kafka"
	plpb "proto/player_locator"
	smpb "proto/scene_manager"
	"shared/generated/table"
	"time"
)

type ServiceContext struct {
	RedisClient         *redis.Client
	SnowFlake           *snowflake.Node
	NodeInfo            login_proto.NodeInfo
	KafkaClient         *kafka.KeyOrderedKafkaProducer
	ExpandMonitor       *kafka.ExpandMonitor
	PlayerLocatorClient plpb.PlayerLocatorClient
	SceneManagerClient  smpb.SceneManagerClient
	GateWatcher         *node.NodeWatcher
	TokenManager        *token.Manager

	// PreloadPool runs background tasks (e.g. Kafka DB-preload) without spawning
	// an unbounded number of goroutines per login. Configured non-blocking so
	// Submit returns ErrPoolOverload immediately when full instead of stalling
	// the caller (which would defeat the whole point of going async).
	PreloadPool *ants.Pool
}

func NewServiceContext() *ServiceContext {
	ctx := context.Background()

	table.LoadTables(config.AppConfig.TableDir, false)

	// Register auth providers based on config
	InitAuthProviders()

	// Initialize Redis client
	redisHost := config.AppConfig.Node.RedisClient.Host
	redisPassword := config.AppConfig.Node.RedisClient.Password
	redisDB := int(config.AppConfig.Node.RedisClient.DB)

	redisClient := redis.NewClient(&redis.Options{
		Addr:            redisHost,
		Password:        redisPassword,
		DB:              redisDB,
		DisableIndentity: true, // suppress CLIENT SETINFO on Redis < 7.2
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

	// Initialize player_locator gRPC client (discovered via etcd)
	plConn := zrpc.MustNewClient(config.AppConfig.PlayerLocatorRpc)
	plClient := plpb.NewPlayerLocatorClient(plConn.Conn())

	// Initialize scene_manager gRPC client (discovered via etcd)
	logx.Infof("[config-debug] SceneManagerRpc.Timeout=%d, PlayerLocatorRpc.Timeout=%d, ServerTimeout=%d",
		config.AppConfig.SceneManagerRpc.Timeout, config.AppConfig.PlayerLocatorRpc.Timeout, config.AppConfig.Timeout)
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

	// Initialize token manager for access/refresh tokens
	tokenMgr := token.NewManager(redisClient, token.Config{
		AccessTokenTTL:  config.AppConfig.TokenConfig.AccessTokenTTL,
		RefreshTokenTTL: config.AppConfig.TokenConfig.RefreshTokenTTL,
	})

	// Register access_token auth provider (requires TokenManager)
	RegisterAccessTokenProvider(tokenMgr)

	// Bounded background-task pool. Size 256 is generous: Kafka SyncProducer is
	// internally serialized by a mutex, so >32 concurrent workers buys nothing,
	// but a larger ceiling absorbs short bursts. Non-blocking + pre-alloc keep
	// Submit cost O(1) and fail fast under overload.
	preloadPool, err := ants.NewPool(
		256,
		ants.WithNonblocking(true),
		ants.WithPreAlloc(true),
		ants.WithPanicHandler(func(p any) {
			logx.Errorf("preload pool task panic: %v", p)
		}),
	)
	if err != nil {
		panic(fmt.Errorf("failed to create preload goroutine pool: %w", err))
	}

	return &ServiceContext{
		RedisClient:         redisClient,
		KafkaClient:         kafkaClient,
		ExpandMonitor:       monitor,
		PlayerLocatorClient: plClient,
		SceneManagerClient:  smClient,
		GateWatcher:         gateWatcher,
		TokenManager:        tokenMgr,
		PreloadPool:         preloadPool,
	}
}

func (s *ServiceContext) Start() {
	s.ExpandMonitor.Start()
}

func (c *ServiceContext) SetNodeId(nodeId int64) {
	// Set package-level vars BEFORE creating the node — NewNode reads these at creation time.
	snowflake.Epoch = config.AppConfig.Snowflake.Epoch
	snowflake.NodeBits = uint8(config.AppConfig.Snowflake.NodeBits)
	snowflake.StepBits = uint8(config.AppConfig.Snowflake.StepBits)

	node, err := snowflake.NewNode(nodeId)
	if err != nil {
		logx.Errorf("Failed to create snowflake node (nodeId=%d): %v", nodeId, err)
		panic(fmt.Errorf("snowflake.NewNode(%d): %w", nodeId, err))
	}

	c.SnowFlake = node
}

// SendBindSessionToGate sends a BindSession command to the target Gate via Kafka.
// This replaces Centre's BindSessionToGate RPC.
func (s *ServiceContext) SendBindSessionToGate(gateID string, gateInstanceID string,
	sessionID uint32, playerID uint64, sessionVersion uint32, enterGsType uint32) error {

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
	partitionKey := strconv.FormatUint(playerID, 10)
	if err := s.KafkaClient.SendToTopic(topic, data, partitionKey); err != nil {
		return fmt.Errorf("send gate bind session command to %s: %w", topic, err)
	}

	return nil
}

// KickSessionOnGate sends a KickPlayer command to the target Gate via Kafka.
func (s *ServiceContext) KickSessionOnGate(gateID string, gateInstanceID string, sessionID uint32, playerID uint64) error {
	eventId := uint32(game.ContractsKafkaKickPlayerEventEventId)

	cmd := &kafkapb.GateCommand{
		SessionId:        sessionID,
		PlayerId:         playerID,
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
	partitionKey := strconv.FormatUint(cmd.PlayerId, 10)
	if err := s.KafkaClient.SendToTopic(topic, data, partitionKey); err != nil {
		return fmt.Errorf("send gate kick command to %s: %w", topic, err)
	}

	return nil
}

func (s *ServiceContext) Stop() {
	s.ExpandMonitor.Stop()
	if s.PreloadPool != nil {
		s.PreloadPool.Release()
	}
}
