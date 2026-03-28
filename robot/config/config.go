package config

import (
	"os"

	"gopkg.in/yaml.v3"
)

// Config holds the robot load-test configuration.
type Config struct {
	GateAddr   string `yaml:"gate_addr"`   // e.g. "127.0.0.1:6000"
	RobotCount int    `yaml:"robot_count"` // number of concurrent robots
	AccountFmt string `yaml:"account_fmt"` // printf pattern, e.g. "robot_%04d"
	Password   string `yaml:"password"`    // shared password for all robots
}

func Load(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	cfg := &Config{
		GateAddr:   "127.0.0.1:6000",
		RobotCount: 1,
		AccountFmt: "robot_%04d",
		Password:   "123456",
	}
	if err := yaml.Unmarshal(data, cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}
