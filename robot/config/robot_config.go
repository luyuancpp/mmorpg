package config

import (
	"log"
	"os"

	"gopkg.in/yaml.v3"
)

// ServerConfig defines the structure for server configuration
type ServerConfig struct {
	Address string `yaml:"address"`
	IP      string `yaml:"ip"`
	Port    int    `yaml:"port"`
}

// RobotsConfig defines the structure for robots configuration
type RobotsConfig struct {
	Count int `yaml:"count"`
}

// Config holds the entire configuration
type Config struct {
	Servers  []ServerConfig `yaml:"servers"` // 支持多个 server
	Robots   RobotsConfig   `yaml:"robots"`
	LogLevel int            `yaml:"loglevel"`
}

// AppConfig is a package-level variable to hold the configuration
var AppConfig Config

// LoadConfig reads the YAML configuration file and populates AppConfig
func LoadConfig(filePath string) {
	data, err := os.ReadFile(filePath) // 使用 os.ReadFile 替代 ioutil.ReadFile
	if err != nil {
		log.Fatalf("error reading YAML file: %v", err)
	}

	err = yaml.Unmarshal(data, &AppConfig)
	if err != nil {
		log.Fatalf("error parsing YAML data: %v", err)
	}
}

func init() {
	LoadConfig("etc/robot_config.yaml")
}
