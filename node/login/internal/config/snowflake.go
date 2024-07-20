package config

import (
	"github.com/zeromicro/go-zero/core/logx"
	"gopkg.in/yaml.v3"
	"os"
)

type SnowFlakeConf struct {
	Epoch    int64 `yaml:"Epoch"`
	NodeBits uint8 `yaml:"NodeBits"`
	StepBits uint8 `yaml:"StepBits"`
}

var SnowFlakeConfig SnowFlakeConf

func init() {
	content, err := os.ReadFile("etc/snowflake.yaml")
	if err != nil {
		logx.Error(err)
	}
	err = yaml.Unmarshal(content, &SnowFlakeConfig)
	if err != nil {
		logx.Error(err)
	}
}
