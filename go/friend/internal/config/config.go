package config

import (
	"time"

	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	RedisClient RedisConf    `json:"RedisClient"`
	MySQL       MySQLConf    `json:"MySQL"`
	Node        NodeConf     `json:"Node"`
	Registry    RegistryConf `json:"Registry"`
	Cache       CacheConf    `json:"Cache"`
	Kafka       KafkaConf    `json:"Kafka"`
}

type RedisConf struct {
	Host     string `json:"Host"`
	Password string `json:"Password"`
	DB       int    `json:"DB"`
}

type MySQLConf struct {
	DataSource string `json:"DataSource"`
}

type NodeConf struct {
	ZoneId   uint32 `json:"ZoneId"`
	LeaseTTL int64  `json:"LeaseTTL"`
}

type RegistryConf struct {
	Etcd EtcdConf `json:"Etcd"`
}

type EtcdConf struct {
	Hosts       []string      `json:"Hosts"`
	DialTimeout time.Duration `json:"DialTimeout"`
}

type CacheConf struct {
	DefaultTTL         time.Duration `json:"DefaultTTL"`
	MaxFriends         uint32        `json:"MaxFriends"`
	MaxPendingRequests uint32        `json:"MaxPendingRequests"`
}

type KafkaConf struct {
	Brokers []string `json:"Brokers"`
}

var AppConfig Config
