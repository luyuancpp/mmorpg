package config

import (
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	Node NodeConfig `json:"Node"`
}

type NodeConfig struct {
	Redis RedisClusterConf `json:"Redis"`
}

type RedisClusterConf struct {
	Cluster struct {
		Hosts []string `json:"Hosts"`
		Type  string   `json:"Type"`
		Pass  string   `json:"Pass"`
	} `json:"Cluster"`
}
