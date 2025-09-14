package config

import "github.com/zeromicro/go-zero/zrpc"

type Config struct {
	zrpc.RpcServerConf
	ServerConfig ServerConfig `json:"ServerConfig"`
}

type ServerConfig struct {
	Database        DatabaseConfig `json:"Database"`
	RedisClient     RedisConfig    `json:"RedisClient"`
	QueueShardCount uint64         `json:"QueueShardCount"`
	JsonPath        string         `json:"JsonPath"`
}

type DatabaseConfig struct {
	Hosts       string `json:"Hosts"`
	User        string `json:"User"`
	Passwd      string `json:"Passwd"`
	DBName      string `json:"DBName"`
	MaxOpenConn int    `json:"MaxOpenConn"`
	MaxIdleConn int    `json:"MaxIdleConn"`
	Net         string `json:"Net"`
}

type RedisConfig struct {
	Hosts             string `json:"Hosts"`
	DefaultTTLSeconds int    `json:"DefaultTTLSeconds"`
	Password          string `json:"Password"`
	DB                int    `json:"DB"`
}

var AppConfig Config
