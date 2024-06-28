package config

import (
	"github.com/zeromicro/go-zero/core/logx"
	"os"
)
import "gopkg.in/yaml.v3"

type RedisConf struct {
	Addr string `yaml:"Addr"`
}

var RedisConfig RedisConf

func init() {
	content, err := os.ReadFile("etc/redis.yaml")
	if err != nil {
		logx.Error(err)
	}
	err = yaml.Unmarshal(content, &RedisConfig)
	if err != nil {
		logx.Error(err)
	}
}
