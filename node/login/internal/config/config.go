package config

import (
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf `yaml:",inline"`

	DbClient  zrpc.RpcClientConf `json:"DbClient"`
	Node      NodeConfig         `yaml:"Node"`
	Snowflake SnowflakeConf      `yaml:"Snowflake"`
}

type RedisConf struct {
	Host     string `yaml:"Host"`
	Password string `yaml:"Password"`
	DB       int    `yaml:"DB"`
	PoolSize int    `yaml:"PoolSize"`
}

type NodeConfig struct {
	ZoneId           uint32    `yaml:"zoneid"`
	SessionExpireMin int       `yaml:"session_expire_minutes"`
	FsmExpireMin     int       `yaml:"fsm_expire_minutes"`
	MaxLoginDevices  int       `yaml:"max_login_devices"`
	Redis            RedisConf `yaml:"Redis"`
}

type SnowflakeConf struct {
	Epoch    int64 `yaml:"Epoch"`
	NodeBits int   `yaml:"NodeBits"`
	StepBits int   `yaml:"StepBits"`
}

// 用于存储配置数据的全局变量
var AppConfig Config
