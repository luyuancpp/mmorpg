package config

import (
	"fmt"

	"github.com/zeromicro/go-zero/zrpc"
)

// Config is the global configuration.
type Config struct {
	zrpc.RpcServerConf
	ZoneId       uint32       `json:"ZoneId"`
	ServerConfig ServerConfig `json:"ServerConfig"`
}

// ServerConfig holds core service settings.
type ServerConfig struct {
	Database    DatabaseConfig `json:"Database"`
	RedisClient RedisConfig    `json:"RedisClient"`
	JsonPath    string         `json:"JsonPath"`
	Kafka       KafkaConfig    `json:"Kafka"`
}

// DatabaseConfig holds database connection settings.
type DatabaseConfig struct {
	Hosts       string `json:"Hosts"`
	User        string `json:"User"`
	Passwd      string `json:"Passwd"`
	DBName      string `json:"DBName"`
	MaxOpenConn int    `json:"MaxOpenConn"`
	MaxIdleConn int    `json:"MaxIdleConn"`
	Net         string `json:"Net"`
}

// RedisConfig holds Redis connection settings.
type RedisConfig struct {
	Hosts             string `json:"Hosts"`
	DefaultTTLSeconds int    `json:"DefaultTTLSeconds"`
	Password          string `json:"Password"`
	DB                int    `json:"DB"`
}

// KafkaConfig holds Kafka consumer settings.
type KafkaConfig struct {
	Brokers         []string `json:"Brokers"`         // Broker addresses
	GroupID         string   `json:"GroupID"`         // Consumer group ID
	Topic           string   `json:"Topic"`           // Topic name
	PartitionCnt    int32    `json:"PartitionCnt"`    // Partition count
	IsOfflineExpand bool     `json:"IsOfflineExpand"` // Offline expansion: true = maintenance mode
}

var AppConfig Config

// DbTaskTopic returns the zone-specific Kafka topic for DB tasks.
func DbTaskTopic(zoneId uint32) string {
	return fmt.Sprintf("db_task_zone_%d", zoneId)
}

// ZoneDBName returns the zone-specific MySQL database name.
func ZoneDBName(zoneId uint32) string {
	return fmt.Sprintf("zone_%d_db", zoneId)
}
