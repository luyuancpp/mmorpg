package config

import (
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	Node      NodeConfig         `json:"Node"`
	Snowflake SnowflakeConf      `json:"Snowflake"`
	DbClient  zrpc.RpcClientConf `json:"DbClient"`
}

type RedisConf struct {
	Host     string `json:"Host"`
	Password string `json:"Password"`
	DB       int    `json:"DB"`
	PoolSize int    `json:"PoolSize"`
}

type NodeConfig struct {
	ZoneId           uint32    `json:"zoneid"`
	SessionExpireMin int       `json:"session_expire_minutes"`
	FsmExpireMin     int       `json:"fsm_expire_minutes"`
	MaxLoginDevices  int       `json:"max_login_devices"`
	Redis            RedisConf `json:"Redis"`
}

type SnowflakeConf struct {
	Epoch    int64 `json:"Epoch"`
	NodeBits int   `json:"NodeBits"`
	StepBits int   `json:"StepBits"`
}

// 用于存储配置数据的全局变量
var AppConfig Config
