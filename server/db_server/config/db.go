package config

import (
	"log"
	"os"
)

// 定义你的YAML结构
type Config struct {
	RoutineNum       int    `yaml:"RoutineNum"`
	ChannelBufferNum uint64 `yaml:"ChannelBufferNum"`
}

var DBConfig Config

func Load() {
	var yamlFile, err = os.Open("config.yaml")
	if err != nil {
		log.Fatalf("error: %v", err)
	}

	err = yaml.Unmarshal(yamlFile, &DBConfig)
	if err != nil {
		log.Fatalf("error: %v", err)
	}

}
