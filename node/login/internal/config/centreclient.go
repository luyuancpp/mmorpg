package config

import (
	"github.com/go-yaml/yaml"
	"io"
	"log"
	"os"
)

type CentreClientConfig struct {
	Ip   string `yaml:"ip"`
	Port int    `yaml:"port"`
}

var CentreClientConf *CentreClientConfig

func init() {
	var yamlFile, err = os.Open("etc/centre_client.yaml")
	if err != nil {
		log.Fatalf("error: %v", err)
	}

	data, err := io.ReadAll(yamlFile)
	if err != nil {
		log.Fatalf("error: %v", err)
	}

	err = yaml.Unmarshal(data, &CentreClientConf)
	if err != nil {
		log.Fatalf("error: %v", err)
	}
}
