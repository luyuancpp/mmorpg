package config

import (
	"github.com/zeromicro/go-zero/core/logx"
	"gopkg.in/yaml.v3"
	"os"
)

type RedisConf struct {
	Addr string `yaml:"addr"`
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
