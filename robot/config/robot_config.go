package config

import (
	"gopkg.in/yaml.v3"
	"io/ioutil"
	"log"
)

// Config 用于映射 YAML 文件中的数据
type Config struct {
	Server struct {
		Address string `yaml:"address"`
		Ip      string `yaml:"ip"`
		Port    int    `yaml:"port"`
	} `yaml:"server"`
	Robots struct {
		Count int `yaml:"count"`
	} `yaml:"robots"`
}

// 包变量
var AppConfig Config

// 初始化配置
func init() {
	data, err := ioutil.ReadFile("etc/robot_config.yaml")
	if err != nil {
		log.Fatalf("error reading YAML file: %v", err)
	}

	err = yaml.Unmarshal(data, &AppConfig)
	if err != nil {
		log.Fatalf("error parsing YAML data: %v", err)
	}
}
