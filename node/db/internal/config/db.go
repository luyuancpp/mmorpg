package config

import (
	"github.com/go-yaml/yaml"
	"io"
	"log"
	"os"
)

type DBConf struct {
	RoutineNum       int    `yaml:"RoutineNum"`
	ChannelBufferNum uint64 `yaml:"ChannelBufferNum"`
	Net              string `yaml:"Net"`
	Addr             string `yaml:"Addr"`
	User             string `yaml:"User"`
	Passwd           string `yaml:"Passwd"`
	DBName           string `yaml:"DBName"`
	MaxOpenConn      int    `yaml:"MaxOpenConn"`
	MaxIdleConn      int    `yaml:"MaxIdleConn"`
}

var DBConfig DBConf

func init() {
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
