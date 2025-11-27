package _config

import (
	"log"
	"os"
	"path/filepath"
	"reflect"
	"strings"

	"gopkg.in/yaml.v3"
)

// Config 代码生成器全局配置
type Config struct {
	Paths          Paths              `yaml:"paths"`
	FileExtensions FileExtensions     `yaml:"file_extensions"`
	Naming         Naming             `yaml:"naming"`
	PathLists      PathLists          `yaml:"path_lists"`
	Generators     Generators         `yaml:"generators"`
	Mappings       map[string]Mapping `yaml:"mappings"`
	Parser         Parser             `yaml:"parser"`
	Log            LogConfig          `yaml:"log"`
}

// Paths 路径配置（对应YAML中的paths节点）
type Paths struct {
	OutputRoot                  string `yaml:"output_root"`
	ProtoDir                    string `yaml:"proto_dir"`
	ProtoParentIncludePath      string `yaml:"proto_parent_include_path"`
	ThirdPartyDir               string `yaml:"third_party_dir"`
	GrpcDir                     string `yaml:"grpc_dir"`
	ProtobufDir                 string `yaml:"protobuf_dir"`
	NodeGoDir                   string `yaml:"node_go_dir"`
	NodeCppDir                  string `yaml:"node_cpp_dir"`
	NodePkgDir                  string `yaml:"node_pkg_dir"`
	NodeLibsEngineDir           string `yaml:"node_libs_engine_dir"`
	NodeLibGameDir              string `yaml:"node_lib_game_dir"`
	NodePkgLogicSceneNodeDir    string `yaml:"node_pkg_logic_scene_node_dir"`
	RoomNodeDir                 string `yaml:"room_node_dir"`
	CentreNodeDir               string `yaml:"centre_node_dir"`
	GateNodeDir                 string `yaml:"gate_node_dir"`
	RobotDir                    string `yaml:"robot_dir"`
	GeneratedDir                string `yaml:"generated_dir"`
	ToolDir                     string `yaml:"tool_dir"`
	TempFileGenDir              string `yaml:"temp_file_gen_dir"`
	PbDescDir                   string `yaml:"pb_desc_dir"`
	AllInOneDesc                string `yaml:"all_in_one_desc"`
	GrpcTempDir                 string `yaml:"grpc_temp_dir"`
	PbcTempDir                  string `yaml:"pbc_temp_dir"`
	PlayerStorageTempDir        string `yaml:"player_storage_temp_dir"`
	PbcProtoOutputDir           string `yaml:"pbc_proto_output_dir"`
	PbcProtoOutputNoSuffixDir   string `yaml:"pbc_proto_output_no_suffix_dir"`
	GeneratedOutputDir          string `yaml:"generated_output_dir"`
	GrpcOutputDir               string `yaml:"grpc_output_dir"`
	GrpcProtoOutputDir          string `yaml:"grpc_proto_output_dir"`
	RobotGoOutputDir            string `yaml:"robot_go_output_dir"`
	RobotGoGamePbDir            string `yaml:"robot_go_game_pb_dir"`
	RobotGoGenDir               string `yaml:"robot_go_gen_dir"`
	GoGeneratorDir              string `yaml:"go_generator_dir"`
	GeneratedRpcDir             string `yaml:"generated_rpc_dir"`
	ServiceInfoDir              string `yaml:"service_info_dir"`
	PbcLuaDir                   string `yaml:"pbc_lua_dir"`
	ClientLuaDir                string `yaml:"client_lua_dir"`
	CppGenGrpcDir               string `yaml:"cpp_gen_grpc_dir"`
	UtilGeneratorDir            string `yaml:"util_generator_dir"`
	GeneratorDir                string `yaml:"generator_dir"`
	GeneratorProtoDir           string `yaml:"generator_proto_dir"`
	ProjectGeneratedDir         string `yaml:"project_generated_dir"`
	TableGeneratorDir           string `yaml:"table_generator_dir"`
	GameRpcProtoPath            string `yaml:"game_rpc_proto_path"`
	ToolsDir                    string `yaml:"tools_dir"`
	ServiceIdFile               string `yaml:"service_id_file"`
	RobotMessageIdFile          string `yaml:"robot_message_id_file"`
	ServiceCppFile              string `yaml:"service_cpp_file"`
	ServiceHeaderFile           string `yaml:"service_header_file"`
	LuaServiceFile              string `yaml:"lua_service_file"`
	ClientLuaServiceFile        string `yaml:"client_lua_service_file"`
	GrpcInitCppFile             string `yaml:"grpc_init_cpp_file"`
	GrpcInitHeadFile            string `yaml:"grpc_init_head_file"`
	GenUtilCppFile              string `yaml:"gen_util_cpp_file"`
	GenUtilHeadFile             string `yaml:"gen_util_head_file"`
	RobotMessageBodyHandlerFile string `yaml:"robot_message_body_handler_file"`
	PlayerStorageSystemDir      string `yaml:"player_storage_system_dir"`
	PlayerDataLoaderFile        string `yaml:"player_data_loader_file"`
	ProtocPath                  string `yaml:"protoc_path"`
}

// FileExtensions 文件扩展名配置
type FileExtensions struct {
	Proto              string `yaml:"proto"`
	PbCc               string `yaml:"pb_cc"`
	PbH                string `yaml:"pb_h"`
	GrpcPbCc           string `yaml:"grpc_pb_cc"`
	GrpcPbH            string `yaml:"grpc_pb_h"`
	ProtoDirName       string `yaml:"proto_dir_name"`
	GoZeroProtoDirName string `yaml:"go_zero_proto_dir_name"`
	Header             string `yaml:"header"`
	Cpp                string `yaml:"cpp"`
	LuaCpp             string `yaml:"lua_cpp"`
	HandlerH           string `yaml:"handler_h"`
	HandlerCpp         string `yaml:"handler_cpp"`
	RepliedHandlerH    string `yaml:"replied_handler_h"`
	RepliedHandlerCpp  string `yaml:"replied_handler_cpp"`
	CppSol2            string `yaml:"cpp_sol2"`
	GrpcClient         string `yaml:"grpc_client"`
	GrpcClientH        string `yaml:"grpc_client_h"`
	GrpcClientCpp      string `yaml:"grpc_client_cpp"`
	ModelSql           string `yaml:"model_sql"`
	LoaderCpp          string `yaml:"loader_cpp"`
}

// Naming 命名规则配置
type Naming struct {
	MessageId            string `yaml:"message_id"`
	MethodIndex          string `yaml:"method_index"`
	HandlerFile          string `yaml:"handler_file"`
	RepliedHandlerFile   string `yaml:"replied_handler_file"`
	EventHandlerBase     string `yaml:"event_handler_base"`
	EmptyResponse        string `yaml:"empty_response"`
	MysqlName            string `yaml:"mysql_name"`
	PlayerService        string `yaml:"player_service"`
	PlayerRepliedService string `yaml:"player_replied_service"`
	CompleteQueue        string `yaml:"complete_queue"`
	DatabasePrefix       string `yaml:"database_prefix"`
	DbProtoFile          string `yaml:"db_proto_file"`
	DbTableFile          string `yaml:"db_table_file"`
	DbTableListJson      string `yaml:"db_table_list_json"`
	GameRpcProto         string `yaml:"game_rpc_proto"`
	GrpcName             string `yaml:"grpc_name"`
}

// PathLists 路径列表配置
type PathLists struct {
	ProtoDirectories         []string          `yaml:"proto_directories"`
	MethodHandlerDirectories MethodHandlerDirs `yaml:"method_handler_directories"`
	GrpcLanguages            []string          `yaml:"grpc_languages"`
}

// MethodHandlerDirs 方法处理器目录映射
type MethodHandlerDirs struct {
	Robot                 string `yaml:"robot"`
	RoomNode              string `yaml:"room_node"`
	RoomNodePlayer        string `yaml:"room_node_player"`
	RoomNodeReplied       string `yaml:"room_node_replied"`
	RoomNodePlayerReplied string `yaml:"room_node_player_replied"`
	CentreNode            string `yaml:"centre_node"`
	CentreNodePlayer      string `yaml:"centre_node_player"`
	CentreReplied         string `yaml:"centre_replied"`
	CentrePlayerReplied   string `yaml:"centre_player_replied"`
	GateNode              string `yaml:"gate_node"`
	GateNodeReplied       string `yaml:"gate_node_replied"`
	GateNodePlayer        string `yaml:"gate_node_player"`
	GateNodePlayerReplied string `yaml:"gate_node_player_replied"`
}

// Generators 生成器开关配置
type Generators struct {
	EnableCpp         bool `yaml:"enable_cpp"`
	EnableGo          bool `yaml:"enable_go"`
	EnableHandler     bool `yaml:"enable_handler"`
	EnableRpcResponse bool `yaml:"enable_rpc_response"`
}

// Mapping 模板映射配置
type Mapping struct {
	Path     string `yaml:"path"`
	Template string `yaml:"template"`
	Lang     string `yaml:"lang"`
}

// Parser Proto解析器配置
type Parser struct {
	IncludePaths []string `yaml:"include_paths"`
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
	// 确定配置文件路径
	filePath := os.Getenv("PROTO_GEN_CONFIG_PATH")
	if filePath == "" {
		// 优先尝试当前目录，再尝试etc目录
		if _, err := os.Stat("proto_gen.yaml"); err == nil {
			filePath = "proto_gen.yaml"
		} else {
			filePath = "etc/proto_gen.yaml"
		}
	}

	// 读取配置文件
	data, err := os.ReadFile(filePath)
	if err != nil {
		log.Fatalf("读取配置文件失败: %w", err)
	}

	// 解析YAML
	if err := yaml.Unmarshal(data, &Global); err != nil {
		log.Fatalf("解析配置文件失败: %w", err)
	}

	// 处理路径中的变量替换（支持嵌套变量）
	if err := resolvePathVariables(); err != nil {
		log.Fatalf("路径变量替换失败: %w", err)
	}

	// 转换为绝对路径
	if err := resolveAbsolutePaths(); err != nil {
		log.Fatalf("绝对路径转换失败: %w", err)
	}

	// 设置默认值
	setDefaults()

	// 验证配置
	if err := validateConfig(); err != nil {
		log.Fatalf("配置验证失败: %w", err)
	}

	return nil
}

// resolvePathVariables 处理路径中的变量替换（支持嵌套变量）
func resolvePathVariables() error {
	// 反射获取Paths结构体的所有字段，用于变量替换
	pathsVal := reflect.ValueOf(&Global.Paths).Elem()
	pathsType := pathsVal.Type()

	// 收集所有可替换的变量（字段名 -> 字段值）
	vars := make(map[string]string)
	for i := 0; i < pathsType.NumField(); i++ {
		field := pathsType.Field(i)
		yamlTag := field.Tag.Get("yaml")
		if yamlTag == "" {
			continue
		}
		vars["{{"+yamlTag+"}}"] = pathsVal.Field(i).String()
	}

	// 循环替换Paths结构体中的变量，直到没有变量可替换
	changed := true
	maxIterations := 10 // 防止无限循环
	iterations := 0

	for changed && iterations < maxIterations {
		changed = false
		iterations++

		for i := 0; i < pathsType.NumField(); i++ {
			field := pathsVal.Field(i)
			if field.Kind() != reflect.String {
				continue
			}

			original := field.String()
			resolved := original

			for k, v := range vars {
				if strings.Contains(resolved, k) {
					resolved = strings.ReplaceAll(resolved, k, v)
					changed = true
				}
			}

			if resolved != original {
				field.SetString(resolved)
				// 更新vars中的值，用于后续嵌套变量替换
				yamlTag := pathsType.Field(i).Tag.Get("yaml")
				if yamlTag != "" {
					vars["{{"+yamlTag+"}}"] = resolved
				}
			}
		}
	}

	if iterations >= maxIterations && changed {
		log.Fatalf("变量替换超过最大迭代次数，可能存在循环引用")
	}

	// 对MethodHandlerDirectories执行同样的循环替换
	handlerDirsVal := reflect.ValueOf(&Global.PathLists.MethodHandlerDirectories).Elem()
	handlerDirsType := handlerDirsVal.Type()

	changed = true
	iterations = 0

	for changed && iterations < maxIterations {
		changed = false
		iterations++

		for i := 0; i < handlerDirsType.NumField(); i++ {
			field := handlerDirsVal.Field(i)
			if field.Kind() != reflect.String {
				continue
			}

			original := field.String()
			resolved := original

			for k, v := range vars {
				if strings.Contains(resolved, k) {
					resolved = strings.ReplaceAll(resolved, k, v)
					changed = true
				}
			}

			if resolved != original {
				field.SetString(resolved)
			}
		}
	}

	if iterations >= maxIterations && changed {
		log.Fatalf("处理器目录变量替换超过最大迭代次数")
	}

	return nil
}

// resolveAbsolutePaths 将所有路径转换为绝对路径，统一使用/并保留末尾斜杠
func resolveAbsolutePaths() error {
	pathsVal := reflect.ValueOf(&Global.Paths).Elem()
	pathsType := pathsVal.Type()

	// 转换Paths中的所有路径为绝对路径
	for i := 0; i < pathsType.NumField(); i++ {
		field := pathsVal.Field(i)
		if field.Kind() != reflect.String {
			continue
		}
		path := field.String()
		if path == "" {
			continue
		}
		absPath, err := filepath.Abs(path)
		if err != nil {
			log.Fatalf("路径 '%s' 转换失败: %w", path, err)
		}
		// 统一替换为/并保留末尾斜杠
		absPath = formatPathWithSlash(absPath, path)
		field.SetString(absPath)
	}

	// 转换MethodHandlerDirectories中的路径为绝对路径
	handlerDirsVal := reflect.ValueOf(&Global.PathLists.MethodHandlerDirectories).Elem()
	handlerDirsType := handlerDirsVal.Type()
	for i := 0; i < handlerDirsType.NumField(); i++ {
		field := handlerDirsVal.Field(i)
		if field.Kind() != reflect.String {
			continue
		}
		path := field.String()
		if path == "" {
			continue
		}
		absPath, err := filepath.Abs(path)
		if err != nil {
			log.Fatalf("处理器目录 '%s' 转换失败: %w", path, err)
		}
		// 统一替换为/并保留末尾斜杠
		absPath = formatPathWithSlash(absPath, path)
		field.SetString(absPath)
	}

	return nil
}

// formatPathWithSlash 统一路径分隔符为/，并根据原始路径保留末尾斜杠
func formatPathWithSlash(absPath, originalPath string) string {
	// 将系统分隔符替换为/
	absPath = strings.ReplaceAll(absPath, string(filepath.Separator), "/")
	// 检查原始路径是否以/或系统分隔符结尾
	hasTrailingSlash := strings.HasSuffix(originalPath, "/") ||
		strings.HasSuffix(originalPath, string(filepath.Separator))
	// 若原始路径有末尾斜杠且当前路径没有，则补充
	if hasTrailingSlash && !strings.HasSuffix(absPath, "/") {
		absPath += "/"
	}
	return absPath
}

// setDefaults 设置默认值（处理未配置的字段）
func setDefaults() {
	// 处理器路径默认值补充
	if Global.PathLists.MethodHandlerDirectories.Robot == "" {
		Global.PathLists.MethodHandlerDirectories.Robot = filepath.Join(Global.Paths.OutputRoot, Global.Paths.RobotDir, "logic/handler/")
	}

	// protoc路径默认值
	if Global.Paths.ProtocPath == "" {
		Global.Paths.ProtocPath = "protoc"
	}

	// 日志默认值
	if Global.Log.Level == "" {
		Global.Log.Level = "info"
	}
	if Global.Log.Output == "" {
		Global.Log.Output = "stdout"
	}
	if Global.Log.FilePath == "" && Global.Log.Output == "file" {
		Global.Log.FilePath = "pbgen.log"
	}

	// 解析器默认值
	if Global.Paths.AllInOneDesc == "" {
		Global.Paths.AllInOneDesc = filepath.Join(Global.Paths.PbDescDir, "all_in_one.desc")
	}
}

// validateConfig 验证配置的完整性和正确性
func validateConfig() error {
	// 检查关键路径是否存在未替换的变量
	if err := validatePaths(); err != nil {
		return err
	}

	// 检查必要的目录是否配置
	if Global.Paths.OutputRoot == "" {
		log.Fatalf("output_root 未配置")
	}

	if Global.Paths.ProtoDir == "" {
		log.Fatalf("proto_dir 未配置")
	}

	// 检查日志配置
	if Global.Log.Output == "file" && Global.Log.FilePath == "" {
		log.Fatalf("日志输出为file时，file_path不能为空")
	}

	return nil
}

// validatePaths 验证路径中是否包含未替换的变量
func validatePaths() error {
	pathsVal := reflect.ValueOf(&Global.Paths).Elem()
	pathsType := pathsVal.Type()

	for i := 0; i < pathsType.NumField(); i++ {
		field := pathsType.Field(i)
		value := pathsVal.Field(i).String()

		if strings.Contains(value, "{{") || strings.Contains(value, "}}") {
			log.Fatalf("路径字段 '%s' 包含未替换的变量: %s", field.Name, value)
		}
	}

	// 检查处理器目录
	handlerDirsVal := reflect.ValueOf(&Global.PathLists.MethodHandlerDirectories).Elem()
	handlerDirsType := handlerDirsVal.Type()

	for i := 0; i < handlerDirsType.NumField(); i++ {
		field := handlerDirsType.Field(i)
		value := handlerDirsVal.Field(i).String()

		if strings.Contains(value, "{{") || strings.Contains(value, "}}") {
			log.Fatalf("处理器目录字段 '%s' 包含未替换的变量: %s", field.Name, value)
		}
	}

	return nil
}

// GetProtoFullPaths 获取完整的Proto文件目录路径
func (c *Config) GetProtoFullPaths() []string {
	var fullPaths []string
	for _, dir := range c.PathLists.ProtoDirectories {
		fullPath := filepath.Join(c.Paths.ProtoDir, dir)
		fullPath = formatPathWithSlash(fullPath, fullPath)
		fullPaths = append(fullPaths, fullPath)
	}
	return fullPaths
}

// GetIncludePaths 获取完整的Include路径
func (c *Config) GetIncludePaths() []string {
	var includePaths []string

	// 添加配置中的include paths
	for _, path := range c.Parser.IncludePaths {
		if strings.Contains(path, "{{") {
			// 如果还有变量，尝试替换
			for k, v := range map[string]string{
				"{{output_root}}": c.Paths.OutputRoot,
				"{{proto_dir}}":   c.Paths.ProtoDir,
			} {
				path = strings.ReplaceAll(path, k, v)
			}
		}

		absPath, err := filepath.Abs(path)
		if err == nil {
			includePaths = append(includePaths, absPath)
		} else {
			includePaths = append(includePaths, path)
		}
	}

	// 添加默认的include路径
	if c.Paths.ProtoParentIncludePath != "" {
		includePaths = append(includePaths, c.Paths.ProtoParentIncludePath)
	}

	if c.Paths.ProtoDir != "" {
		includePaths = append(includePaths, c.Paths.ProtoDir)
	}

	return includePaths
}
