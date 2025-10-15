package config

import "github.com/zeromicro/go-zero/zrpc"

// Config 全局配置结构体（包含 RPC 服务配置和自定义服务配置）
type Config struct {
	zrpc.RpcServerConf
	ServerConfig ServerConfig `json:"ServerConfig"`
}

// ServerConfig 服务核心配置（新增 Kafka 配置）
type ServerConfig struct {
	Database        DatabaseConfig `json:"Database"`
	RedisClient     RedisConfig    `json:"RedisClient"`
	QueueShardCount uint64         `json:"QueueShardCount"` // 复用为 Kafka 分区数
	JsonPath        string         `json:"JsonPath"`
	// 👇 新增 Kafka 配置（与 yaml 中的 ServerConfig.Kafka 对应）
	Kafka KafkaConfig `json:"Kafka"`
}

// DatabaseConfig 数据库配置（原有不变）
type DatabaseConfig struct {
	Hosts       string `json:"Hosts"`
	User        string `json:"User"`
	Passwd      string `json:"Passwd"`
	DBName      string `json:"DBName"`
	MaxOpenConn int    `json:"MaxOpenConn"`
	MaxIdleConn int    `json:"MaxIdleConn"`
	Net         string `json:"Net"`
}

// RedisConfig Redis 配置（原有不变）
type RedisConfig struct {
	Hosts             string `json:"Hosts"`
	DefaultTTLSeconds int    `json:"DefaultTTLSeconds"`
	Password          string `json:"Password"`
	DB                int    `json:"DB"`
}

// 👇 新增 Kafka 配置结构体（存储 Kafka 连接信息）
type KafkaConfig struct {
	Brokers       string `json:"Brokers"`       // Kafka 集群地址（多个用逗号分隔）
	ConsumerGroup string `json:"ConsumerGroup"` // 消费者组 ID
}

var AppConfig Config // 全局配置实例（保持不变）
