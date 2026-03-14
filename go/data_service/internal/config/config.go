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
