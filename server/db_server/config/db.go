package config

import (
	"github.com/go-yaml/yaml"
	"io"
	"log"
	"os"
)

type Config struct {
	RoutineNum       int    `yaml:"RoutineNum"`
	ChannelBufferNum uint64 `yaml:"ChannelBufferNum"`
}

var DBConfig Config

func Load() {
	var yamlFile, err = os.Open("etc/db.yaml")
	if err != nil {
		log.Fatalf("error: %v", err)
	}

	data, err := io.ReadAll(yamlFile)
	if err != nil {
		log.Fatalf("error: %v", err)
	}

	err = yaml.Unmarshal(data, &DBConfig)
	if err != nil {
		log.Fatalf("error: %v", err)
	}

}
