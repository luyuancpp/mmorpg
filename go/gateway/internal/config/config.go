package config

import (
	"time"

	"github.com/zeromicro/go-zero/rest"
)

type Config struct {
	RestConf rest.RestConf

	Etcd EtcdConf `json:"Etcd"`
	Gate GateConf `json:"Gate"`
}

type EtcdConf struct {
	Hosts       []string      `json:"Hosts"`
	DialTimeout time.Duration `json:"DialTimeout,default=5s"`
}

type GateConf struct {
	TokenSecret      string        `json:"TokenSecret"`
	DiscoveryTimeout time.Duration `json:"DiscoveryTimeout,default=5s"`
}

var AppConfig Config
