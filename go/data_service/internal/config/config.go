package config

import (
	"github.com/zeromicro/go-zero/core/stores/redis"
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	MappingRedis     redis.RedisConf
	Regions          []RegionConfig
	DevRedis         DevRedisConfig `json:",optional"`
	PlayerLockTTLSec int            `json:",default=3"`

	// Snapshot MySQL (for rollback persistence)
	SnapshotMySQL SnapshotMySQLConfig `json:",optional"`

	// Login admin gRPC client (for orphan account cleanup during rollback)
	LoginAdminRpc zrpc.RpcClientConf `json:",optional"`
}

type DevRedisConfig struct {
	Host     string `json:",optional"`
	Password string `json:",optional"`
	DB       int    `json:",default=0"`
}

type RegionConfig struct {
	Id    uint32
	Zones []uint32
	Redis RedisClusterConfig
}

type RedisClusterConfig struct {
	Addrs    []string
	Password string `json:",optional"`
}

type SnapshotMySQLConfig struct {
	Host        string `json:",default=127.0.0.1:3306"`
	User        string `json:",default=appuser"`
	Password    string `json:",default=apppass123"`
	DBName      string `json:",default=testdb"`
	MaxOpenConn int    `json:",default=5"`
	MaxIdleConn int    `json:",default=2"`
}
