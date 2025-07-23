package config

import "github.com/zeromicro/go-zero/zrpc"

type Config struct {
	zrpc.RpcServerConf
	ServerConfig ServerConfig `json:"ServerConfig"`
}

type ServerConfig struct {
	RoutineNum       int            `json:"RoutineNum"`
	ChannelBufferNum uint64         `json:"ChannelBufferNum"`
	Database         DatabaseConfig `json:"Database"`
	Redis            RedisConfig    `json:"Redis"`
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
}

var AppConfig Config
