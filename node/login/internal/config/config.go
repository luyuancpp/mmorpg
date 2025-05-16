package config

import (
	"github.com/zeromicro/go-zero/zrpc"
	"gopkg.in/yaml.v3"
	"log"
	"os"
)

type Config struct {
	zrpc.RpcServerConf
}

type NodeConfig struct {
	ZoneID uint32 `yaml:"zoneid"`
}

// 用于存储配置数据的全局变量
var AppConfig NodeConfig // 现在使用 AppConfig 来存储配置数据

// init 函数会在程序启动时自动执行
func init() {
	// 自动读取配置文件
	config, err := ReadConfig("etc/config.yaml")
	if err != nil {
		log.Fatalf("Error reading config file: %v", err)
	}

	// 将配置存储到全局变量
	AppConfig = *config
}

// ReadConfig 从 YAML 文件中读取配置
func ReadConfig(filename string) (*NodeConfig, error) {
	// 打开 YAML 文件
	file, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	// 解析 YAML 文件
	var config NodeConfig
	decoder := yaml.NewDecoder(file)
	if err := decoder.Decode(&config); err != nil {
		return nil, err
	}

	return &config, nil
}
