package config

import (
	"gopkg.in/yaml.v3"
	"log"
	"os"
)

// ServerConfig 服务器配置（单个后端服务地址）
type ServerConfig struct {
	Address string `yaml:"address"` // 服务器地址（格式：ip:port）
}

// Robots 机器人集群配置（新增 MaxConcurrent 控制并发）
type Robots struct {
	Count         int   `yaml:"count"`          // 机器人总数量
	Tick          int64 `yaml:"tick"`           // 行为树 Tick 间隔（毫秒）
	LoginInterval int64 `yaml:"login_interval"` // 机器人登录间隔（毫秒，避免服务器瞬时压力）
	MaxConcurrent int   `yaml:"max_concurrent"` // 最大并发数（适配 ants 池，建议 = 总数量 × 2）
}

// Config 全局配置根结构
type Config struct {
	Servers  []ServerConfig `yaml:"servers"`  // 后端服务器列表（支持轮询分配）
	Robots   Robots         `yaml:"robots"`   // 机器人配置
	LogLevel int            `yaml:"loglevel"` // 日志级别（zap 级别：-1=Debug, 0=Info, 1=Warn, 2=Error）
}

// AppConfig 包级全局配置变量（业务层直接访问）
var AppConfig Config

// LoadConfig 加载 YAML 配置文件
func LoadConfig(filePath string) {
	// 检查文件是否存在
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		log.Fatalf("configuration file not found: %s (err: %v)", filePath, err)
	}

	// 读取配置文件
	data, err := os.ReadFile(filePath)
	if err != nil {
		log.Fatalf("failed to read config file: %v", err)
	}

	// 解析 YAML 到结构体
	err = yaml.Unmarshal(data, &AppConfig)
	if err != nil {
		log.Fatalf("failed to parse config file: %v", err)
	}

	// 配置默认值（避免未配置导致程序异常）
	setDefaultConfig()
}

// setDefaultConfig 为未配置的字段设置默认值
func setDefaultConfig() {
	// 机器人配置默认值
	if AppConfig.Robots.Count <= 0 {
		AppConfig.Robots.Count = 10 // 默认10个机器人
	}
	if AppConfig.Robots.Tick <= 0 {
		AppConfig.Robots.Tick = 500 // 默认500毫秒 Tick 间隔
	}
	if AppConfig.Robots.LoginInterval <= 0 {
		AppConfig.Robots.LoginInterval = 100 // 默认100毫秒登录间隔
	}
	if AppConfig.Robots.MaxConcurrent <= 0 {
		// 默认并发数 = 机器人数量 × 2（适配每个机器人2个核心任务）
		AppConfig.Robots.MaxConcurrent = AppConfig.Robots.Count * 2
	}

	// 日志级别默认值（默认 Info 级别）
	if AppConfig.LogLevel < -1 || AppConfig.LogLevel > 2 {
		AppConfig.LogLevel = 0
	}
}

// 初始化加载配置（程序启动时自动执行）
func init() {
	LoadConfig("etc/robot_config.yaml")
}
