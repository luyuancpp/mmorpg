package _config

import (
	"fmt"
	"os"
	"path/filepath"

	"gopkg.in/yaml.v3"
)

// Config 代码生成器全局配置
type Config struct {
	Paths      Paths              `yaml:"paths"`
	Generators Generators         `yaml:"generators"`
	Mappings   map[string]Mapping `yaml:"mappings"`
	Parser     Parser             `yaml:"parser"`
	Log        LogConfig          `yaml:"log"`
}

// Paths 路径配置
type Paths struct {
	ProtoRoot   string            `yaml:"proto_root"`
	OutputRoot  string            `yaml:"output_root"`
	TemplateDir string            `yaml:"template_dir"`
	TempDir     string            `yaml:"temp_dir"`
	ProtocPath  string            `yaml:"protoc_path"`
	GrpcPlugins map[string]string `yaml:"grpc_plugins"` // key: 语言(cpp/go), value: 插件路径
}

// Generators 生成器开关
type Generators struct {
	EnableCpp         bool `yaml:"enable_cpp"`
	EnableGo          bool `yaml:"enable_go"`
	EnableHandler     bool `yaml:"enable_handler"`
	EnableRpcResponse bool `yaml:"enable_rpc_response"`
}

// Mapping 模板映射配置
type Mapping struct {
	Path     string `yaml:"path"`     // 生成文件路径模板
	Template string `yaml:"template"` // 模板文件路径
	Lang     string `yaml:"lang"`     // 目标语言
}

// Parser proto解析器配置
type Parser struct {
	IncludePaths []string `yaml:"include_paths"`
	AllInOneDesc string   `yaml:"all_in_one_desc"`
	IgnoreFiles  []string `yaml:"ignore_files"`
}

// LogConfig 日志配置
type LogConfig struct {
	Level    string `yaml:"level"`
	Output   string `yaml:"output"`
	FilePath string `yaml:"file_path"`
}

// 全局配置实例
var Global Config

// Load 加载配置文件
func Load() error {
	filePath := os.Getenv("PROTO_GEN_CONFIG_PATH")
	if filePath == "" {
		// 优先尝试当前目录，再尝试上级etc目录
		if _, err := os.Stat(".yaml"); err == nil {
			filePath = "proto_gen.yaml"
		} else {
			filePath = "etc/proto_gen.yaml"
		}
	}

	// 读取文件
	data, err := os.ReadFile(filePath)
	if err != nil {
		return fmt.Errorf("读取配置文件失败: %w", err)
	}

	// 解析YAML
	if err := yaml.Unmarshal(data, &Global); err != nil {
		return fmt.Errorf("解析配置文件失败: %w", err)
	}

	// 处理路径（转为绝对路径，确保跨平台兼容）
	if err := resolvePaths(); err != nil {
		return fmt.Errorf("路径处理失败: %w", err)
	}

	// 设置默认值（兼容未配置的字段）
	setDefaults()

	return nil
}

// 处理路径为绝对路径
func resolvePaths() error {
	absProtoRoot, err := filepath.Abs(Global.Paths.ProtoRoot)
	if err != nil {
		return err
	}
	Global.Paths.ProtoRoot = absProtoRoot

	absOutputRoot, err := filepath.Abs(Global.Paths.OutputRoot)
	if err != nil {
		return err
	}
	Global.Paths.OutputRoot = absOutputRoot

	// 其他路径同理处理...
	return nil
}

// 设置默认值
func setDefaults() {
	if Global.Paths.ProtocPath == "" {
		Global.Paths.ProtocPath = "protoc" // 默认从环境变量查找
	}
	if Global.Log.Level == "" {
		Global.Log.Level = "info"
	}
	if Global.Parser.AllInOneDesc == "" {
		Global.Parser.AllInOneDesc = "all_in_one.desc"
	}
}
