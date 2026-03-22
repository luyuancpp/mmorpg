package config

import (
	"time"

	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	RedisClient RedisConf    `json:"RedisClient"`
	Kafka       KafkaConf    `json:"Kafka"`
	Node        NodeConf     `json:"Node"`
	Registry    RegistryConf `json:"Registry"`
	Lease       LeaseConf    `json:"Lease"`
}

type RedisConf struct {
	Host     string `json:"Host"`
	Password string `json:"Password"`
	DB       int    `json:"DB"`
}

type KafkaConf struct {
	Brokers []string `json:"Brokers"`
}

type NodeConf struct {
	ZoneId   uint32 `json:"ZoneId"`
	LeaseTTL int64  `json:"LeaseTTL"` // etcd lease TTL (seconds)
}

type RegistryConf struct {
	Etcd EtcdConf `json:"Etcd"`
}

type EtcdConf struct {
	Hosts       []string      `json:"Hosts"`
	DialTimeout time.Duration `json:"DialTimeout"`
}

type LeaseConf struct {
	DefaultTTLSeconds uint32        `json:"DefaultTTLSeconds"` // disconnect lease, default 30
	PollInterval      time.Duration `json:"PollInterval"`      // lease monitor poll interval, default 1s
	BatchSize         int           `json:"BatchSize"`         // max expired leases per tick, default 100
}

var AppConfig Config
