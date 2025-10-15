package config

import (
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	Node      NodeConfig    `json:"Node"`
	Snowflake SnowflakeConf `json:"Snowflake"`
	Locker    LockerConf    `json:"Locker"`
	Account   AccountConf   `json:"Account"`
	Registry  RegistryConf  `json:"Registry"`
	Timeouts  TimeoutConf   `json:"Timeouts"`
	Kafka     KafkaConfig   `json:"Kafka"` // Kafka配置
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

// KafkaConfig Kafka配置结构体（包含分区数）
type KafkaConfig struct {
	Brokers      string `json:"Brokers"`      // Kafka集群地址（逗号分隔）
	GroupID      string `json:"GroupID"`      // 消费者组ID
	Topic        string `json:"Topic"`        // 消费主题名
	PartitionCnt int32  `json:"PartitionCnt"` // 主题分区数量（新增）
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
