package config

import "github.com/zeromicro/go-zero/zrpc"

// Config 全局配置结构体
type Config struct {
	zrpc.RpcServerConf
	ServerConfig ServerConfig `json:"ServerConfig"`
}

// ServerConfig 服务核心配置
type ServerConfig struct {
	Database    DatabaseConfig `json:"Database"`
	RedisClient RedisConfig    `json:"RedisClient"`
	JsonPath    string         `json:"JsonPath"`
	Kafka       KafkaConfig    `json:"Kafka"` // Kafka配置
}

// DatabaseConfig 数据库配置
type DatabaseConfig struct {
	Hosts       string `json:"Hosts"`
	User        string `json:"User"`
	Passwd      string `json:"Passwd"`
	DBName      string `json:"DBName"`
	MaxOpenConn int    `json:"MaxOpenConn"`
	MaxIdleConn int    `json:"MaxIdleConn"`
	Net         string `json:"Net"`
}

// RedisConfig Redis配置
type RedisConfig struct {
	Hosts             string `json:"Hosts"`
	DefaultTTLSeconds int    `json:"DefaultTTLSeconds"`
	Password          string `json:"Password"`
	DB                int    `json:"DB"`
}

// KafkaConfig Kafka配置结构体（包含分区数）
type KafkaConfig struct {
	Brokers         string `json:"Brokers"`         // Kafka集群地址（逗号分隔）
	GroupID         string `json:"GroupID"`         // 消费者组ID
	Topic           string `json:"Topic"`           // 消费主题名
	PartitionCnt    int32  `json:"PartitionCnt"`    // 主题分区数量（新增）
	IsOfflineExpand bool   `json:"IsOfflineExpand"` // 停服扩容开关：true=停服模式
}

var AppConfig Config // 全局配置实例
