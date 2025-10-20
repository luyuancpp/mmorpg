package config

import (
	"github.com/zeromicro/go-zero/zrpc"
	"time"
)

type Config struct {
	zrpc.RpcServerConf
	Node      NodeConfig    `json:"Node"`
	Snowflake SnowflakeConf `json:"Snowflake"`
	Locker    LockerConf    `json:"Locker"`
	Account   AccountConf   `json:"Account"`
	Registry  RegistryConf  `json:"Registry"`
	Timeouts  TimeoutConf   `json:"Timeouts"`
	Kafka     KafkaConfig   `json:"Kafka"` // 合并后的Kafka配置（包含生产者参数）
}

type RedisConf struct {
	Host              string `json:"Host"`
	Password          string `json:"Password"`
	DB                uint32 `json:"DB"`
	PoolSize          uint32 `json:"PoolSize"`
	DefaultTTLSeconds uint32 `json:"DefaultTTLSeconds"`
}

type AccountConf struct {
	MaxDevicesPerAccount int64 `json:"MaxDevicesPerAccount"`
	CacheExpireHours     int   `json:"CacheExpireHours"` // 单位：小时
}

type NodeConfig struct {
	ZoneId           uint32    `json:"ZoneId"`
	SessionExpireMin uint32    `json:"SessionExpireMin"`
	MaxLoginDevices  uint32    `json:"MaxLoginDevices"`
	RedisClient      RedisConf `json:"RedisClient"`
	LeaseTTL         int64     `json:"LeaseTTL"`
	QueueShardCount  uint64    `json:"QueueShardCount"`
}

// KafkaConfig 合并后的Kafka配置（包含原KafkaConfig和ProducerConfig的所有字段）
type KafkaConfig struct {
	// 基础配置
	BootstrapServers string `json:"Brokers"`      // Kafka集群地址（逗号分隔）
	GroupID          string `json:"GroupID"`      // 消费者组ID
	Topic            string `json:"Topic"`        // 主题名（生产/消费共用）
	PartitionCnt     int32  `json:"PartitionCnt"` // 主题分区数量

	// 生产者专用配置（时间字段使用time.Duration）
	InitialPartition int           `json:"InitialPartition"` // 初始分区数
	DialTimeout      time.Duration `json:"DialTimeout"`      // 网络拨号超时（支持10s、500ms等格式）
	ReadTimeout      time.Duration `json:"ReadTimeout"`      // 读取超时
	WriteTimeout     time.Duration `json:"WriteTimeout"`     // 写入超时
	RetryMax         int           `json:"RetryMax"`         // 最大重试次数
	RetryBackoff     time.Duration `json:"RetryBackoff"`     // 重试间隔
	ChannelBuffer    int           `json:"ChannelBuffer"`    // 通道缓冲大小
	SyncInterval     time.Duration `json:"SyncInterval"`     // 分区同步间隔
	StatsInterval    time.Duration `json:"StatsInterval"`    // 统计打印间隔
	CompressionType  int           `json:"CompressionType"`  // 压缩类型（snappy/gzip/none）
	Idempotent       bool          `json:"Idempotent"`       // 是否启用幂等性
	MaxOpenRequests  int           `json:"MaxOpenRequests"`
}

type SnowflakeConf struct {
	Epoch    int64  `json:"Epoch"`
	NodeBits uint32 `json:"NodeBits"`
	StepBits uint32 `json:"StepBits"`
}

type LockerConf struct {
	AccountLockTTL uint32 `json:"AccountLockTTL"` // 单位：秒
	PlayerLockTTL  uint32 `json:"PlayerLockTTL"`  // 单位：秒
}

type RegistryConf struct {
	Etcd EtcdRegistryConf `json:"Etcd"`
}

type EtcdRegistryConf struct {
	Hosts         []string `json:"Hosts"`
	Key           string   `json:"Key"`
	DialTimeoutMS int      `json:"DialTimeoutMS"`
}

type TimeoutConf struct {
	EtcdDialTimeoutMS         int `json:"EtcdDialTimeoutMS"`
	ServiceDiscoveryTimeoutMS int `json:"ServiceDiscoveryTimeoutMS"`
	TaskWaitTimeoutSec        int `json:"TaskWaitTimeoutSec"`
	AccountCacheExpireHours   int `json:"AccountCacheExpireHours"`
	RoleCacheExpireHours      int `json:"RoleCacheExpireHours"`
}

// 用于存储配置数据的全局变量
var AppConfig Config
