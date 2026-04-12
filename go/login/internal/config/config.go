package config

import (
	"fmt"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/zrpc"
)

// Config is the top-level configuration.
type Config struct {
	zrpc.RpcServerConf                    // Embedded go-zero RPC config (ListenOn, Etcd, etc.)
	Node               NodeConfig         `json:"Node"`
	Snowflake          SnowflakeConf      `json:"Snowflake"`
	Locker             LockerConf         `json:"Locker"`
	Account            AccountConf        `json:"Account"`
	Registry           RegistryConf       `json:"Registry"`
	Timeouts           TimeoutConf        `json:"Timeouts"`
	Kafka              KafkaConfig        `json:"Kafka"`
	PlayerLocatorRpc   zrpc.RpcClientConf `json:"PlayerLocatorRpc"`   // player_locator gRPC client
	SceneManagerRpc    zrpc.RpcClientConf `json:"SceneManagerRpc"`    // scene_manager gRPC client
	GateTokenSecret    string             `json:"GateTokenSecret"`    // HMAC secret for gate connection tokens
}

// NodeConfig holds node-level settings including login duration limits.
type NodeConfig struct {
	ZoneId           uint32        `json:"ZoneId"`
	SessionExpireMin uint32        `json:"SessionExpireMin"` // Session idle timeout in minutes
	MaxLoginDevices  uint32        `json:"MaxLoginDevices"`
	RedisClient      RedisConf     `json:"RedisClient"`
	LeaseTTL         int64         `json:"LeaseTTL"` // Lease TTL in seconds
	QueueShardCount  uint64        `json:"QueueShardCount"`
	MaxLoginDuration time.Duration `json:"MaxLoginDuration"` // Max online duration per login (e.g. 24h, force logout on expiry)
	LogoutGraceTime  time.Duration `json:"LogoutGraceTime"`  // Grace period before forced logout (e.g. 5m, for player warning)
}

// RedisConf holds Redis client settings.
type RedisConf struct {
	Host         string        `json:"Host"`
	Password     string        `json:"Password"`
	DB           uint32        `json:"DB"`
	DefaultTTL   time.Duration `json:"DefaultTTL"`
	DialTimeout  time.Duration `json:"DialTimeout"`
	ReadTimeout  time.Duration `json:"ReadTimeout"`
	WriteTimeout time.Duration `json:"WriteTimeout"`
}

// KafkaConfig holds Kafka producer and consumer settings.
type KafkaConfig struct {
	Brokers          []string                `json:"Brokers"`
	GroupID          string                  `json:"GroupID"`
	Topic            string                  `json:"Topic,optional"` // Derived from Node.ZoneId at startup
	PartitionCnt     int32                   `json:"PartitionCnt"`
	InitialPartition int                     `json:"InitialPartition"` // Should match PartitionCnt
	DialTimeout      time.Duration           `json:"DialTimeout"`
	ReadTimeout      time.Duration           `json:"ReadTimeout"`
	WriteTimeout     time.Duration           `json:"WriteTimeout"`
	RetryMax         int                     `json:"RetryMax"`
	RetryBackoff     time.Duration           `json:"RetryBackoff"`
	ChannelBuffer    int                     `json:"ChannelBuffer"`
	SyncInterval     time.Duration           `json:"SyncInterval"`
	StatsInterval    time.Duration           `json:"StatsInterval"`
	CompressionType  sarama.CompressionCodec `json:"CompressionType"` // none/gzip/snappy
	Idempotent       bool                    `json:"Idempotent"`
	MaxOpenRequests  int                     `json:"MaxOpenRequests"` // Must be 1 when idempotent
	RetentionMs      int64                   `json:"RetentionMs,default=300000"` // Topic retention in ms (default 5 min)
}

// SnowflakeConf holds snowflake ID generator settings.
type SnowflakeConf struct {
	Epoch    int64  `json:"Epoch"` // Epoch timestamp in milliseconds
	NodeBits uint32 `json:"NodeBits"`
	StepBits uint32 `json:"StepBits"`
}

// LockerConf holds distributed lock settings.
type LockerConf struct {
	AccountLockTTL uint32 `json:"AccountLockTTL"` // Account lock TTL in seconds
	PlayerLockTTL  uint32 `json:"PlayerLockTTL"`  // Player lock TTL in seconds
}

// AccountConf holds account-related settings.
type AccountConf struct {
	MaxDevicesPerAccount int64         `json:"MaxDevicesPerAccount"`
	MaxPlayersPerAccount int           `json:"MaxPlayersPerAccount,default=5"`
	CacheExpire          time.Duration `json:"CacheExpire"`
}

// RegistryConf holds service registry/discovery settings.
type RegistryConf struct {
	Etcd EtcdRegistryConf `json:"Etcd"`
}

// EtcdRegistryConf holds etcd registry settings.
type EtcdRegistryConf struct {
	Hosts       []string      `json:"Hosts"`
	Key         string        `json:"Key"`
	DialTimeout time.Duration `json:"DialTimeout"`
}

// TimeoutConf holds various timeout settings.
type TimeoutConf struct {
	EtcdDialTimeout         time.Duration `json:"EtcdDialTimeout"`
	ServiceDiscoveryTimeout time.Duration `json:"ServiceDiscoveryTimeout"`
	TaskWaitTimeout         time.Duration `json:"TaskWaitTimeout"`
	LoginTotalTimeout       time.Duration `json:"LoginTotalTimeout"` // Total login timeout including Redis/DB
	RoleCacheExpire         time.Duration `json:"RoleCacheExpire"`
}

var AppConfig Config

// DbTaskTopic returns the zone-specific Kafka topic for DB tasks.
func DbTaskTopic(zoneId uint32) string {
	return fmt.Sprintf("db_task_zone_%d", zoneId)
}
