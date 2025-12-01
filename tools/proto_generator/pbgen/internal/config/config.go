package _config

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"sync"

	"gopkg.in/yaml.v3"
)

// Config 代码生成器全局配置
type Config struct {
	Paths          Paths              `yaml:"paths"`
	DirectoryNames DirectoryNames     `yaml:"directory_names"` // 新增：目录命名常量
	FileExtensions FileExtensions     `yaml:"file_extensions"`
	Naming         Naming             `yaml:"naming"`
	PathLists      PathLists          `yaml:"path_lists"`
	Generators     Generators         `yaml:"generators"`
	Mappings       map[string]Mapping `yaml:"mappings"`
	Parser         Parser             `yaml:"parser"`
	Log            LogConfig          `yaml:"log"`
}

// DirectoryNames 目录命名常量（对应YAML中的directory_names节点）
type DirectoryNames struct {
	GeneratedRpcName     string `yaml:"generated_rpc_name"`       // RPC生成目录名
	ServiceInfoName      string `yaml:"service_info_name"`        // 服务元数据目录名
	ProtoDirName         string `yaml:"proto_dir_name"`           // Proto目录名
	GoZeroProtoDirName   string `yaml:"go_zero_proto_dir_name"`   // Go-zero Proto目录名
	RobotProtoName       string `yaml:"robot_proto_name"`         // Robot Proto目录名常量
	RobotGoZeroProtoName string `yaml:"robot_go_zero_proto_name"` // Robot Go-zero Proto目录名常量
	ModelDirName         string `yaml:"model_dir_name"`           // 模型目录名
	ServiceIncludeDir    string `yaml:"service_include_dir"`      // 服务导入目录名
}

// Paths 路径配置（对应YAML中的paths节点）
type Paths struct {
	OutputRoot                      string `yaml:"output_root"`
	ProtoDir                        string `yaml:"proto_dir"`
	ProtoParentIncludePath          string `yaml:"proto_parent_include_path"`
	ThirdPartyDir                   string `yaml:"third_party_dir"`
	GrpcDir                         string `yaml:"grpc_dir"`
	ProtobufDir                     string `yaml:"protobuf_dir"`
	NodeGoDir                       string `yaml:"node_go_dir"`
	NodeCppDir                      string `yaml:"node_cpp_dir"`
	NodePkgDir                      string `yaml:"node_pkg_dir"`
	NodeLibsEngine                  string `yaml:"node_libs_engine"`
	NodeLibGame                     string `yaml:"node_lib_game"`
	NodePkgLogicRoom                string `yaml:"node_pkg_logic_room"`
	RoomNodeDir                     string `yaml:"room_node_dir"`
	CentreNodeDir                   string `yaml:"centre_node_dir"`
	GateNodeDir                     string `yaml:"gate_node_dir"`
	Robot                           string `yaml:"robot"`
	RobotGenerated                  string `yaml:"robot_generated"`
	RobotProto                      string `yaml:"robot_proto"`
	RobotGeneratedProto             string `yaml:"robot_generated_proto"`
	RobotGoZeroProto                string `yaml:"robot_go_zero_proto"`
	GeneratedDir                    string `yaml:"generated_dir"`
	ToolDir                         string `yaml:"tool_dir"`
	TempFileGenDir                  string `yaml:"temp_file_gen_dir"`
	ProtoBufDescDir                 string `yaml:"proto_buf_desc_dir"`
	AllInOneDesc                    string `yaml:"all_in_one_desc"`
	GrpcTempDir                     string `yaml:"grpc_temp_dir"`
	ProtoBufCTempDir                string `yaml:"proto_buf_c_temp_dir"`
	PlayerStorageTempDir            string `yaml:"player_storage_temp_dir"`
	ProtoBufCProtoOutputDir         string `yaml:"proto_buf_c_proto_output_dir"`
	ProtoBufCProtoOutputNoSuffixDir string `yaml:"proto_buf_c_proto_output_no_suffix_dir"`
	GeneratedOutputDir              string `yaml:"generated_output_dir"`
	GrpcOutputDir                   string `yaml:"grpc_output_dir"`
	GrpcProtoOutputDir              string `yaml:"grpc_proto_output_dir"`
	RobotGoOutputDir                string `yaml:"robot_go_output_dir"`
	RobotGeneratedOutputDir         string `yaml:"robot_generated_output_dir"`
	RobotGoGamePbDir                string `yaml:"robot_go_game_pb_dir"`
	RobotGoGenDir                   string `yaml:"robot_go_gen_dir"`
	GoGeneratorDir                  string `yaml:"go_generator_dir"`
	GeneratedRpcDir                 string `yaml:"generated_rpc_dir"`
	ServiceInfoDir                  string `yaml:"service_info_dir"`
	ProtoBufCLuaDir                 string `yaml:"proto_buf_c_lua_dir"`
	ClientLuaDir                    string `yaml:"client_lua_dir"`
	CppGenGrpcDir                   string `yaml:"cpp_gen_grpc_dir"`
	UtilGeneratorDir                string `yaml:"util_generator_dir"`
	GeneratorDir                    string `yaml:"generator_dir"`
	GeneratorProtoDir               string `yaml:"generator_proto_dir"`
	ProjectGeneratedDir             string `yaml:"project_generated_dir"`
	TableGeneratorDir               string `yaml:"table_generator_dir"`
	GameRpcProtoPath                string `yaml:"game_rpc_proto_path"`
	ToolsDir                        string `yaml:"tools_dir"`
	ServiceIdFile                   string `yaml:"service_id_file"`
	RobotMessageIdFile              string `yaml:"robot_message_id_file"`
	RobotProtoImportPath            string `yaml:"robot_proto_import_path"`
	ServiceCppFile                  string `yaml:"service_cpp_file"`
	ServiceHeaderFile               string `yaml:"service_header_file"`
	LuaServiceFile                  string `yaml:"lua_service_file"`
	ClientLuaServiceFile            string `yaml:"client_lua_service_file"`
	GrpcInitCppFile                 string `yaml:"grpc_init_cpp_file"`
	GrpcInitHeadFile                string `yaml:"grpc_init_head_file"`
	GenUtilCppFile                  string `yaml:"gen_util_cpp_file"`
	GenUtilHeadFile                 string `yaml:"gen_util_head_file"`
	RobotMsgBodyHandlerFile         string `yaml:"robot_msg_body_handler_file"`
	PlayerStorageSystemDir          string `yaml:"player_storage_system_dir"`
	PlayerDataLoaderFile            string `yaml:"player_data_loader_file"`
	ProtocPath                      string `yaml:"protoc_path"`

	// 原有配置中的路径
	EventHandlerSourceDirectory     string `yaml:"event_handler_source_directory"`
	RoomNodeEventHandlerDirectory   string `yaml:"room_node_event_handler_directory"`
	CentreNodeEventHandlerDirectory string `yaml:"centre_node_event_handler_directory"`
}

// FileExtensions 文件扩展名配置（仅存放文件后缀/扩展名）
type FileExtensions struct {
	Proto             string `yaml:"proto"`
	PbCc              string `yaml:"pb_cc"`
	PbH               string `yaml:"pb_h"`
	GrpcPbCc          string `yaml:"grpc_pb_cc"`
	GrpcPbH           string `yaml:"grpc_pb_h"`
	Header            string `yaml:"header"`
	Cpp               string `yaml:"cpp"`
	LuaCpp            string `yaml:"lua_cpp"`
	HandlerH          string `yaml:"handler_h"`
	HandlerCpp        string `yaml:"handler_cpp"`
	RepliedHandlerH   string `yaml:"replied_handler_h"`
	RepliedHandlerCpp string `yaml:"replied_handler_cpp"`
	CppSol2           string `yaml:"cpp_sol2"`
	GrpcClient        string `yaml:"grpc_client"`
	GrpcClientH       string `yaml:"grpc_client_h"`
	GrpcClientCpp     string `yaml:"grpc_client_cpp"`
	ModelSql          string `yaml:"model_sql"`
	LoaderCpp         string `yaml:"loader_cpp"`

	// 原有配置中的扩展名
	HandlerCppExtension                string `yaml:"handler_cpp_extension"`
	RepliedHandlerHeaderExtension      string `yaml:"replied_handler_header_extension"`
	CppRepliedHandlerEx                string `yaml:"cpp_replied_handler_ex"`
	ModelSqlExtension                  string `yaml:"model_sql_extension"`
	ProtoGoPackageSuffix               string `yaml:"proto_go_package_suffix"`
	MessageIdGoFile                    string `yaml:"message_id_go_file"`
	ServiceInfoExtension               string `yaml:"service_info_extension"`
	RegisterRepliedHandlerCppExtension string `yaml:"register_replied_handler_cpp_extension"`
	RegisterHandlerCppExtension        string `yaml:"register_handler_cpp_extension"`
}

// Naming 命名规则配置（业务逻辑命名常量）
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
	RobotRpcProto        string `yaml:"robot_rpc_proto"`
	GrpcName             string `yaml:"grpc_name"`

	// 原有配置中的命名常量
	TypePlayer                      string `yaml:"type_player"`
	ServiceIncludeName              string `yaml:"service_include_name"`
	PlayerServiceIncludeName        string `yaml:"player_service_include_name"`
	PlayerServiceRepliedIncludeName string `yaml:"player_service_replied_include_name"`
	MacroReturnIncludeName          string `yaml:"macro_return_include_name"`
	NodeEnumName                    string `yaml:"node_enum_name"`
	NodeServiceSuffix               string `yaml:"node_service_suffix"`
	GoPackage                       string `yaml:"go_package"`
	GoRobotPackage                  string `yaml:"go_robot_package"`
	GoogleMethodController          string `yaml:"google_method_controller"`
	PlayerMethodController          string `yaml:"player_method_controller"`
	IncludeEndLine                  string `yaml:"include_end_line"`
	IncludeBegin                    string `yaml:"include_begin"`
	YourCodeBegin                   string `yaml:"your_code_begin"`
	YourCodeEnd                     string `yaml:"your_code_end"`
	YourCodePair                    string `yaml:"your_code_pair"`
	Model                           string `yaml:"model"`
}

// PathLists 路径列表配置
type PathLists struct {
	ProtoDirectories         []string          `yaml:"proto_directories"`
	RobotProtoDirectories    []string          `yaml:"robot_proto_directories"`
	MethodHandlerDirectories MethodHandlerDirs `yaml:"method_handler_directories"`
	GrpcLanguages            []string          `yaml:"grpc_languages"`
	ProtoDirectoryIndexes    ProtoDirIndexes   `yaml:"proto_directory_indexes"`
	NodeTypes                NodeTypes         `yaml:"node_types"`
}

// ProtoDirIndexes Proto目录索引配置
type ProtoDirIndexes struct {
	CommonProtoDirIndex         int `yaml:"common_proto_dir_index"`
	LogicComponentProtoDirIndex int `yaml:"logic_component_proto_dir_index"`
	LogicEventProtoDirIndex     int `yaml:"logic_event_proto_dir_index"`
	LogicSharedProtoDirIndex    int `yaml:"logic_shared_proto_dir_index"`
	LogicProtoDirIndex          int `yaml:"logic_proto_dir_index"`
	PlayerLocatorDirIndex       int `yaml:"player_locator_dir_index"`
	ConstantsDirIndex           int `yaml:"constants_dir_index"`
	EtcdProtoDirIndex           int `yaml:"etcd_proto_dir_index"`
	LoginProtoDirIndex          int `yaml:"login_proto_dir_index"`
	DbProtoDirIndex             int `yaml:"db_proto_dir_index"`
	CenterProtoDirIndex         int `yaml:"center_proto_dir_index"`
	RoomProtoDirIndex           int `yaml:"room_proto_dir_index"`
	GateProtoDirIndex           int `yaml:"gate_proto_dir_index"`
	ChatProtoDirIndex           int `yaml:"chat_proto_dir_index"`
	TeamProtoDirIndex           int `yaml:"team_proto_dir_index"`
	MailProtoDirIndex           int `yaml:"mail_proto_dir_index"`
	RobotProtoDirIndex          int `yaml:"robot_proto_dir_index"`
}

// NodeTypes 节点类型配置
type NodeTypes struct {
	TcpNode  uint32 `yaml:"tcp_node"`
	GrpcNode uint32 `yaml:"grpc_node"`
	HttpNode uint32 `yaml:"http_node"`
}

// MethodHandlerDirs 方法处理器目录映射
type MethodHandlerDirs struct {
	Robot                   string `yaml:"robot"`
	RoomNode                string `yaml:"room_node"`
	RoomNodePlayer          string `yaml:"room_node_player"`
	RoomNodeReplied         string `yaml:"room_node_replied"`
	RoomNodePlayerReplied   string `yaml:"room_node_player_replied"`
	CentreNode              string `yaml:"centre_node"`
	CentreNodePlayer        string `yaml:"centre_node_player"`
	CentreNodeReplied       string `yaml:"centre_node_replied"`
	CentreNodePlayerReplied string `yaml:"centre_node_player_replied"`
	GateNode                string `yaml:"gate_node"`
	GateNodeReplied         string `yaml:"gate_node_replied"`
	GateNodePlayer          string `yaml:"gate_node_player"`
	GateNodePlayerReplied   string `yaml:"gate_node_player_replied"`
}

// Generators 生成器开关配置
type Generators struct {
	EnableCpp         bool `yaml:"enable_cpp"`
	EnableGo          bool `yaml:"enable_go"`
	EnableHandler     bool `yaml:"enable_handler"`
	EnableRpcResponse bool `yaml:"enable_rpc_response"`
	EnableRobotProto  bool `yaml:"enable_robot_proto"`
	EnableRobotGoZero bool `yaml:"enable_robot_go_zero"`
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

var (
	// 全局配置实例
	Global Config

	// 初始化锁
	initOnce sync.Once

	// 错误存储
	initError error
)

// Load 加载配置文件（线程安全）
func Load() error {
	initOnce.Do(func() {
		initError = loadConfig()
	})
	return initError
}

// loadConfig 实际的配置加载逻辑
func loadConfig() error {
	// 确定配置文件路径
	filePath := os.Getenv("PROTO_GEN_CONFIG_PATH")
	if filePath == "" {
		// 优先尝试当前目录，再尝试etc目录
		if _, err := os.Stat("proto_gen.yaml"); err == nil {
			filePath = "proto_gen.yaml"
		} else if _, err := os.Stat("etc/proto_gen.yaml"); err == nil {
			filePath = "etc/proto_gen.yaml"
		} else {
			return fmt.Errorf("配置文件未找到，当前目录和etc目录下均无proto_gen.yaml")
		}
	}

	// 读取配置文件
	data, err := os.ReadFile(filePath)
	if err != nil {
		return fmt.Errorf("读取配置文件失败: %w", err)
	}

	// 解析YAML
	if err := yaml.Unmarshal(data, &Global); err != nil {
		return fmt.Errorf("解析配置文件失败: %w", err)
	}

	// 处理路径中的变量替换（支持嵌套变量）
	if err := resolvePathVariables(); err != nil {
		return fmt.Errorf("路径变量替换失败: %w", err)
	}

	// 转换为绝对路径
	if err := resolveAbsolutePaths(); err != nil {
		return fmt.Errorf("绝对路径转换失败: %w", err)
	}

	// 设置默认值
	setDefaults()

	// 补充原有配置中的常量计算
	calculateDerivedConstants()

	// 验证配置
	if err := validateConfig(); err != nil {
		return fmt.Errorf("配置验证失败: %w", err)
	}

	// 创建必要的目录
	if err := createRequiredDirs(); err != nil {
		log.Printf("警告: 创建必要目录失败: %v", err)
	}

	return nil
}

// calculateDerivedConstants 计算派生的常量值
func calculateDerivedConstants() {
	// 计算YourCodePair
	if Global.Naming.YourCodePair == "" && Global.Naming.YourCodeBegin != "" && Global.Naming.YourCodeEnd != "" {
		Global.Naming.YourCodePair = Global.Naming.YourCodeBegin + "\n" + Global.Naming.YourCodeEnd + "\n"
	}

	// 计算注册处理器扩展名
	if Global.FileExtensions.RegisterRepliedHandlerCppExtension == "" {
		Global.FileExtensions.RegisterRepliedHandlerCppExtension = "register" + Global.FileExtensions.CppRepliedHandlerEx
	}
	if Global.FileExtensions.RegisterHandlerCppExtension == "" {
		Global.FileExtensions.RegisterHandlerCppExtension = "register" + Global.FileExtensions.HandlerCppExtension
	}
}

// resolvePathVariables 处理路径中的变量替换（支持嵌套变量）
func resolvePathVariables() error {
	// 收集所有可替换的变量
	vars := make(map[string]string)

	// 收集Paths的变量
	collectStructVariables(reflect.ValueOf(&Global.Paths).Elem(), vars)

	// 收集FileExtensions的变量
	collectStructVariables(reflect.ValueOf(&Global.FileExtensions).Elem(), vars)

	// 收集Naming的变量
	collectStructVariables(reflect.ValueOf(&Global.Naming).Elem(), vars)

	// 替换所有结构体中的变量
	if err := replaceVariablesInAllStructs(vars); err != nil {
		return err
	}

	// 替换切片中的变量
	if err := replaceVariablesInAllSlices(vars); err != nil {
		return err
	}

	return nil
}

// collectStructVariables 收集结构体中的变量
func collectStructVariables(val reflect.Value, vars map[string]string) {
	valType := val.Type()
	for i := 0; i < valType.NumField(); i++ {
		field := valType.Field(i)
		yamlTag := field.Tag.Get("yaml")
		if yamlTag == "" {
			continue
		}
		if val.Field(i).Kind() == reflect.String {
			vars["{{"+yamlTag+"}}"] = val.Field(i).String()
		}
	}
}

// replaceVariablesInAllStructs 替换所有结构体中的变量
func replaceVariablesInAllStructs(vars map[string]string) error {
	// 替换Paths中的变量
	if err := replaceVariablesInStruct(reflect.ValueOf(&Global.Paths).Elem(), vars); err != nil {
		return err
	}

	// 替换DirectoryNames中的变量（新增）
	if err := replaceVariablesInStruct(reflect.ValueOf(&Global.DirectoryNames).Elem(), vars); err != nil {
		return err
	}

	// 替换FileExtensions中的变量
	if err := replaceVariablesInStruct(reflect.ValueOf(&Global.FileExtensions).Elem(), vars); err != nil {
		return err
	}

	// 替换Naming中的变量
	if err := replaceVariablesInStruct(reflect.ValueOf(&Global.Naming).Elem(), vars); err != nil {
		return err
	}

	// 替换MethodHandlerDirectories中的变量
	if err := replaceVariablesInStruct(reflect.ValueOf(&Global.PathLists.MethodHandlerDirectories).Elem(), vars); err != nil {
		return err
	}

	return nil
}

// replaceVariablesInAllSlices 替换所有切片中的变量
func replaceVariablesInAllSlices(vars map[string]string) error {
	if err := replaceVariablesInSlice(reflect.ValueOf(&Global.PathLists.RobotProtoDirectories), vars); err != nil {
		return err
	}

	if err := replaceVariablesInSlice(reflect.ValueOf(&Global.Parser.IncludePaths), vars); err != nil {
		return err
	}

	if err := replaceVariablesInSlice(reflect.ValueOf(&Global.Parser.IgnoreFiles), vars); err != nil {
		return err
	}

	return nil
}

// replaceVariablesInStruct 替换结构体中的变量
func replaceVariablesInStruct(val reflect.Value, vars map[string]string) error {
	valType := val.Type()
	maxIterations := 10 // 防止无限循环

	for iter := 0; iter < maxIterations; iter++ {
		changed := false

		for i := 0; i < valType.NumField(); i++ {
			field := val.Field(i)
			if field.Kind() != reflect.String {
				continue
			}

			original := field.String()
			resolved := replaceVariables(original, vars)

			if resolved != original {
				field.SetString(resolved)
				changed = true

				// 更新vars中的值，用于后续嵌套变量替换
				yamlTag := valType.Field(i).Tag.Get("yaml")
				if yamlTag != "" {
					vars["{{"+yamlTag+"}}"] = resolved
				}
			}
		}

		if !changed {
			break
		}

		if iter == maxIterations-1 {
			return fmt.Errorf("变量替换超过最大迭代次数，可能存在循环引用")
		}
	}

	return nil
}

// replaceVariablesInSlice 替换字符串切片中的变量
func replaceVariablesInSlice(val reflect.Value, vars map[string]string) error {
	if val.Kind() != reflect.Ptr || val.Elem().Kind() != reflect.Slice {
		return nil
	}

	slice := val.Elem()
	for i := 0; i < slice.Len(); i++ {
		elem := slice.Index(i)
		if elem.Kind() != reflect.String {
			continue
		}

		original := elem.String()
		resolved := replaceVariables(original, vars)

		if resolved != original {
			elem.SetString(resolved)
		}
	}

	return nil
}

// replaceVariables 替换字符串中的变量
func replaceVariables(s string, vars map[string]string) string {
	for k, v := range vars {
		s = strings.ReplaceAll(s, k, v)
	}
	return s
}

// resolveAbsolutePaths 将所有路径转换为绝对路径，统一使用/并保留末尾斜杠
func resolveAbsolutePaths() error {
	// 转换Paths中的所有路径为绝对路径
	if err := resolveAbsolutePathsInStruct(reflect.ValueOf(&Global.Paths).Elem()); err != nil {
		return err
	}

	// 转换DirectoryNames中的路径为绝对路径（新增）
	if err := resolveAbsolutePathsInStruct(reflect.ValueOf(&Global.DirectoryNames).Elem()); err != nil {
		return err
	}

	// 转换MethodHandlerDirectories中的路径为绝对路径
	if err := resolveAbsolutePathsInStruct(reflect.ValueOf(&Global.PathLists.MethodHandlerDirectories).Elem()); err != nil {
		return err
	}

	// 转换切片中的路径为绝对路径
	if err := resolveAbsolutePathsInSlice(reflect.ValueOf(&Global.PathLists.RobotProtoDirectories)); err != nil {
		return err
	}

	if err := resolveAbsolutePathsInSlice(reflect.ValueOf(&Global.Parser.IncludePaths)); err != nil {
		return err
	}

	return nil
}

// resolveAbsolutePathsInStruct 转换结构体中的路径为绝对路径
func resolveAbsolutePathsInStruct(val reflect.Value) error {
	valType := val.Type()

	for i := 0; i < valType.NumField(); i++ {
		field := val.Field(i)
		if field.Kind() != reflect.String {
			continue
		}

		path := field.String()
		if path == "" {
			continue
		}

		absPath, err := filepath.Abs(path)
		if err != nil {
			return fmt.Errorf("路径 '%s' 转换失败: %w", path, err)
		}

		// 统一替换为/并保留末尾斜杠
		absPath = formatPathWithSlash(absPath, path)
		field.SetString(absPath)
	}

	return nil
}

// resolveAbsolutePathsInSlice 转换字符串切片中的路径为绝对路径
func resolveAbsolutePathsInSlice(val reflect.Value) error {
	if val.Kind() != reflect.Ptr || val.Elem().Kind() != reflect.Slice {
		return nil
	}

	slice := val.Elem()
	for i := 0; i < slice.Len(); i++ {
		elem := slice.Index(i)
		if elem.Kind() != reflect.String {
			continue
		}

		path := elem.String()
		if path == "" {
			continue
		}

		absPath, err := filepath.Abs(path)
		if err == nil {
			absPath = formatPathWithSlash(absPath, path)
			elem.SetString(absPath)
		}
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
		Global.PathLists.MethodHandlerDirectories.Robot = filepath.Join(Global.Paths.OutputRoot, Global.Paths.Robot, "logic/handler/")
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
	if Global.Paths.AllInOneDesc == "" && Global.Paths.ProtoBufDescDir != "" {
		Global.Paths.AllInOneDesc = filepath.Join(Global.Paths.ProtoBufDescDir, "all_in_one.desc")
	}

	// 设置DirectoryNames默认值（新增）
	if Global.DirectoryNames.GeneratedRpcName == "" {
		Global.DirectoryNames.GeneratedRpcName = "rpc/"
	}
	if Global.DirectoryNames.ServiceInfoName == "" {
		Global.DirectoryNames.ServiceInfoName = "service_metadata/"
	}
	if Global.DirectoryNames.ProtoDirName == "" {
		Global.DirectoryNames.ProtoDirName = "proto/"
	}
	if Global.DirectoryNames.GoZeroProtoDirName == "" {
		Global.DirectoryNames.GoZeroProtoDirName = "go-zero_proto/"
	}
	if Global.DirectoryNames.ModelDirName == "" {
		Global.DirectoryNames.ModelDirName = "model/"
	}
	if Global.DirectoryNames.ServiceIncludeDir == "" {
		Global.DirectoryNames.ServiceIncludeDir = "rpc/"
	}

	// Robot相关默认值
	if Global.Paths.Robot != "" {
		// 使用DirectoryNames中的目录名配置（修改）
		protoDirName := Global.DirectoryNames.ProtoDirName
		if protoDirName == "" {
			protoDirName = "proto/"
		}

		goZeroProtoDirName := Global.DirectoryNames.GoZeroProtoDirName
		if goZeroProtoDirName == "" {
			goZeroProtoDirName = "go-zero_proto/"
		}

		if Global.DirectoryNames.RobotProtoName == "" {
			Global.DirectoryNames.RobotProtoName = filepath.Join(Global.Paths.RobotGenerated, protoDirName)
		}
		if Global.DirectoryNames.RobotGoZeroProtoName == "" {
			Global.DirectoryNames.RobotGoZeroProtoName = filepath.Join(Global.DirectoryNames.RobotProtoName, goZeroProtoDirName)
		}

		if Global.Paths.RobotGenerated == "" {
			Global.Paths.RobotGenerated = filepath.Join(Global.Paths.OutputRoot, Global.Paths.Robot, "generated/")
		}
		if Global.Paths.RobotProto == "" {
			Global.Paths.RobotProto = Global.DirectoryNames.RobotProtoName
		}
		if Global.Paths.RobotGeneratedProto == "" {
			Global.Paths.RobotGeneratedProto = Global.DirectoryNames.RobotProtoName
		}
		if Global.Paths.RobotGoZeroProto == "" {
			Global.Paths.RobotGoZeroProto = Global.DirectoryNames.RobotGoZeroProtoName
		}
		if Global.Paths.RobotGeneratedOutputDir == "" {
			Global.Paths.RobotGeneratedOutputDir = filepath.Join(Global.Paths.OutputRoot, Global.Paths.Robot, "generated/")
		}
		if Global.Paths.RobotGoGenDir == "" {
			Global.Paths.RobotGoGenDir = filepath.Join(Global.Paths.OutputRoot, Global.Paths.RobotGenerated)
		}
		if Global.Paths.RobotProtoImportPath == "" {
			Global.Paths.RobotProtoImportPath = filepath.Join(Global.Paths.OutputRoot, Global.Paths.RobotProto)
		}
	}

	// 生成器开关默认值
	if !Global.Generators.EnableRobotProto && Global.Paths.Robot != "" {
		Global.Generators.EnableRobotProto = true
	}
	if !Global.Generators.EnableRobotGoZero && Global.Paths.Robot != "" {
		Global.Generators.EnableRobotGoZero = true
	}

	if Global.Paths.EventHandlerSourceDirectory == "" {
		Global.Paths.EventHandlerSourceDirectory = "handler/event/"
	}
	if Global.Paths.RoomNodeEventHandlerDirectory == "" && Global.Paths.RoomNodeDir != "" {
		Global.Paths.RoomNodeEventHandlerDirectory = filepath.Join(Global.Paths.RoomNodeDir, Global.Paths.EventHandlerSourceDirectory)
	}
	if Global.Paths.CentreNodeEventHandlerDirectory == "" && Global.Paths.CentreNodeDir != "" {
		Global.Paths.CentreNodeEventHandlerDirectory = filepath.Join(Global.Paths.CentreNodeDir, Global.Paths.EventHandlerSourceDirectory)
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
		return fmt.Errorf("output_root 未配置")
	}

	if Global.Paths.ProtoDir == "" {
		return fmt.Errorf("proto_dir 未配置")
	}

	// Robot相关配置检查
	if Global.Generators.EnableRobotProto && Global.Paths.Robot == "" {
		return fmt.Errorf("启用了robot proto生成，但robot_dir未配置")
	}

	// 检查日志配置
	if Global.Log.Output == "file" && Global.Log.FilePath == "" {
		return fmt.Errorf("日志输出为file时，file_path不能为空")
	}

	return nil
}

// validatePaths 验证路径中是否包含未替换的变量
func validatePaths() error {
	// 检查Paths结构体
	if err := validateStructPaths(reflect.ValueOf(&Global.Paths).Elem()); err != nil {
		return err
	}

	// 检查DirectoryNames结构体（新增）
	if err := validateStructPaths(reflect.ValueOf(&Global.DirectoryNames).Elem()); err != nil {
		return err
	}

	// 检查FileExtensions结构体
	if err := validateStructPaths(reflect.ValueOf(&Global.FileExtensions).Elem()); err != nil {
		return err
	}

	// 检查Naming结构体
	if err := validateStructPaths(reflect.ValueOf(&Global.Naming).Elem()); err != nil {
		return err
	}

	// 检查处理器目录
	if err := validateStructPaths(reflect.ValueOf(&Global.PathLists.MethodHandlerDirectories).Elem()); err != nil {
		return err
	}

	// 检查字符串切片
	if err := validateSlicePaths(reflect.ValueOf(&Global.PathLists.ProtoDirectories), "ProtoDirectories"); err != nil {
		return err
	}

	if err := validateSlicePaths(reflect.ValueOf(&Global.PathLists.RobotProtoDirectories), "RobotProtoDirectories"); err != nil {
		return err
	}

	if err := validateSlicePaths(reflect.ValueOf(&Global.Parser.IncludePaths), "Parser.IncludePaths"); err != nil {
		return err
	}

	return nil
}

// validateStructPaths 验证结构体中的路径
func validateStructPaths(val reflect.Value) error {
	valType := val.Type()

	for i := 0; i < valType.NumField(); i++ {
		field := valType.Field(i)
		value := val.Field(i).String()

		if strings.Contains(value, "{{") || strings.Contains(value, "}}") {
			return fmt.Errorf("路径字段 '%s' 包含未替换的变量: %s", field.Name, value)
		}
	}

	return nil
}

// validateSlicePaths 验证字符串切片中的路径
func validateSlicePaths(val reflect.Value, name string) error {
	if val.Kind() != reflect.Ptr || val.Elem().Kind() != reflect.Slice {
		return nil
	}

	slice := val.Elem()
	for i := 0; i < slice.Len(); i++ {
		value := slice.Index(i).String()
		if strings.Contains(value, "{{") || strings.Contains(value, "}}") {
			return fmt.Errorf("%s[%d] 包含未替换的变量: %s", name, i, value)
		}
	}

	return nil
}

// createRequiredDirs 创建必要的目录
func createRequiredDirs() error {
	dirs := []string{
		Global.Paths.GeneratedOutputDir,
		Global.Paths.GrpcOutputDir,
		Global.Paths.ProtoBufCLuaDir,
		Global.Paths.TempFileGenDir,
		Global.Paths.ProtoBufDescDir,
		Global.Paths.RoomNodeEventHandlerDirectory,
		Global.Paths.CentreNodeEventHandlerDirectory,
	}

	// 添加DirectoryNames中的目录（新增）
	if Global.DirectoryNames.RobotProtoName != "" {
		dirs = append(dirs, Global.DirectoryNames.RobotProtoName)
	}
	if Global.DirectoryNames.RobotGoZeroProtoName != "" {
		dirs = append(dirs, Global.DirectoryNames.RobotGoZeroProtoName)
	}

	// 添加robot相关目录
	if Global.Generators.EnableRobotProto {
		dirs = append(dirs,
			Global.Paths.RobotGenerated,
			Global.Paths.RobotProto,
			Global.Paths.RobotGeneratedProto,
			Global.Paths.RobotGoZeroProto,
			Global.Paths.RobotGeneratedOutputDir,
			Global.Paths.RobotGoGamePbDir,
		)
	}

	for _, dir := range dirs {
		if dir == "" {
			continue
		}
		if err := os.MkdirAll(dir, 0755); err != nil {
			return fmt.Errorf("创建目录 %s 失败: %w", dir, err)
		}
	}

	return nil
}

// GetProtoFullPaths 获取完整的Proto文件目录路径
func (c *Config) GetProtoFullPaths() []string {
	var fullPaths []string
	for _, dir := range c.PathLists.ProtoDirectories {
		fullPath := dir
		if !filepath.IsAbs(dir) {
			fullPath = filepath.Join(c.Paths.ProtoDir, dir)
		}
		fullPath = formatPathWithSlash(fullPath, fullPath)
		fullPaths = append(fullPaths, fullPath)
	}
	return fullPaths
}

// GetRobotProtoFullPaths 获取完整的Robot Proto文件目录路径
func (c *Config) GetRobotProtoFullPaths() []string {
	var fullPaths []string
	if !c.Generators.EnableRobotProto {
		return fullPaths
	}

	for _, dir := range c.PathLists.RobotProtoDirectories {
		fullPath := dir
		if !filepath.IsAbs(dir) {
			fullPath = filepath.Join(c.Paths.RobotProto, dir)
		}
		fullPath = formatPathWithSlash(fullPath, fullPath)
		fullPaths = append(fullPaths, fullPath)
	}
	return fullPaths
}

// GetAllProtoFullPaths 获取所有Proto文件目录路径（包括全局和Robot）
func (c *Config) GetAllProtoFullPaths() []string {
	fullPaths := c.GetProtoFullPaths()
	robotPaths := c.GetRobotProtoFullPaths()
	return append(fullPaths, robotPaths...)
}

// GetIncludePaths 获取完整的Include路径
func (c *Config) GetIncludePaths() []string {
	var includePaths []string
	seen := make(map[string]bool) // 去重

	// 添加配置中的include paths
	for _, path := range c.Parser.IncludePaths {
		if strings.Contains(path, "{{") {
			// 如果还有变量，尝试替换
			path = replaceVariables(path, map[string]string{
				"{{output_root}}":              c.Paths.OutputRoot,
				"{{proto_dir}}":                c.Paths.ProtoDir,
				"{{robot_proto}}":              c.Paths.RobotProto,
				"{{robot}}":                    c.Paths.Robot,
				"{{proto_dir_name}}":           c.DirectoryNames.ProtoDirName,         // 修改：从DirectoryNames获取
				"{{robot_proto_name}}":         c.DirectoryNames.RobotProtoName,       // 修改：从DirectoryNames获取
				"{{robot_go_zero_proto_name}}": c.DirectoryNames.RobotGoZeroProtoName, // 修改：从DirectoryNames获取
			})
		}

		absPath, err := filepath.Abs(path)
		if err == nil {
			path = absPath
		}

		if !seen[path] {
			includePaths = append(includePaths, path)
			seen[path] = true
		}
	}

	// 添加默认的include路径
	defaultPaths := []string{
		c.Paths.ProtoParentIncludePath,
		c.Paths.ProtoDir,
		c.Paths.RobotProto,
		c.Paths.RobotProtoImportPath,
		c.DirectoryNames.RobotProtoName, // 新增：添加DirectoryNames中的路径
	}

	for _, path := range defaultPaths {
		if path != "" && !seen[path] {
			includePaths = append(includePaths, path)
			seen[path] = true
		}
	}

	return includePaths
}

// GetRobotOutputDirs 获取Robot相关的输出目录映射
func (c *Config) GetRobotOutputDirs() map[string]string {
	return map[string]string{
		"generated":        c.Paths.RobotGenerated,
		"proto":            c.Paths.RobotProto,
		"generated_proto":  c.Paths.RobotGeneratedProto,
		"go_zero_proto":    c.Paths.RobotGoZeroProto,
		"go_gen":           c.Paths.RobotGoGenDir,
		"game_pb":          c.Paths.RobotGoGamePbDir,
		"robot_proto_name": c.DirectoryNames.RobotProtoName, // 新增：添加DirectoryNames中的目录
	}
}

// GetTemplatePath 获取模板的完整路径
func (c *Config) GetTemplatePath(mappingName string) (string, error) {
	mapping, ok := c.Mappings[mappingName]
	if !ok {
		return "", fmt.Errorf("模板映射 '%s' 不存在", mappingName)
	}
	return mapping.Template, nil
}

// GetOutputPath 获取生成文件的输出路径
func (c *Config) GetOutputPath(mappingName, protoDir, protoFile string) (string, error) {
	mapping, ok := c.Mappings[mappingName]
	if !ok {
		return "", fmt.Errorf("模板映射 '%s' 不存在", mappingName)
	}

	path := replaceVariables(mapping.Path, map[string]string{
		"{{proto_dir}}":      protoDir,
		"{{proto_file}}":     protoFile,
		"{{proto_dir_name}}": c.DirectoryNames.ProtoDirName, // 修改：从DirectoryNames获取
	})

	return path, nil
}

// IsGeneratorEnabled 检查生成器是否启用
func (c *Config) IsGeneratorEnabled(generator string) bool {
	switch generator {
	case "cpp":
		return c.Generators.EnableCpp
	case "go":
		return c.Generators.EnableGo
	case "handler":
		return c.Generators.EnableHandler
	case "rpc_response":
		return c.Generators.EnableRpcResponse
	case "robot_proto":
		return c.Generators.EnableRobotProto
	case "robot_go_zero":
		return c.Generators.EnableRobotGoZero
	default:
		return false
	}
}

// ShouldIgnoreFile 检查文件是否应该被忽略
func (c *Config) ShouldIgnoreFile(filename string) bool {
	for _, pattern := range c.Parser.IgnoreFiles {
		if matched, _ := filepath.Match(pattern, filename); matched {
			return true
		}
	}
	return false
}

// ========== 辅助方法 ==========

// GetDirName 获取目录名配置（修改：从DirectoryNames获取）
func (c *Config) GetDirName(name string) string {
	switch name {
	case "proto":
		return c.DirectoryNames.ProtoDirName
	case "go_zero_proto":
		return c.DirectoryNames.GoZeroProtoDirName
	case "robot_proto":
		return c.DirectoryNames.RobotProtoName
	case "robot_go_zero_proto":
		return c.DirectoryNames.RobotGoZeroProtoName
	case "generated_rpc":
		return c.DirectoryNames.GeneratedRpcName
	case "service_info":
		return c.DirectoryNames.ServiceInfoName
	case "model":
		return c.DirectoryNames.ModelDirName
	case "service_include":
		return c.DirectoryNames.ServiceIncludeDir
	default:
		return ""
	}
}

// ========== 原有配置的兼容方法 ==========

// GetProtoDirIndex 获取Proto目录索引
func (c *Config) GetProtoDirIndex(name string) int {
	switch name {
	case "common":
		return c.PathLists.ProtoDirectoryIndexes.CommonProtoDirIndex
	case "logic_component":
		return c.PathLists.ProtoDirectoryIndexes.LogicComponentProtoDirIndex
	case "logic_event":
		return c.PathLists.ProtoDirectoryIndexes.LogicEventProtoDirIndex
	case "logic_shared":
		return c.PathLists.ProtoDirectoryIndexes.LogicSharedProtoDirIndex
	case "logic":
		return c.PathLists.ProtoDirectoryIndexes.LogicProtoDirIndex
	case "player_locator":
		return c.PathLists.ProtoDirectoryIndexes.PlayerLocatorDirIndex
	case "constants":
		return c.PathLists.ProtoDirectoryIndexes.ConstantsDirIndex
	case "etcd":
		return c.PathLists.ProtoDirectoryIndexes.EtcdProtoDirIndex
	case "login":
		return c.PathLists.ProtoDirectoryIndexes.LoginProtoDirIndex
	case "db":
		return c.PathLists.ProtoDirectoryIndexes.DbProtoDirIndex
	case "center":
		return c.PathLists.ProtoDirectoryIndexes.CenterProtoDirIndex
	case "room":
		return c.PathLists.ProtoDirectoryIndexes.RoomProtoDirIndex
	case "gate":
		return c.PathLists.ProtoDirectoryIndexes.GateProtoDirIndex
	case "chat":
		return c.PathLists.ProtoDirectoryIndexes.ChatProtoDirIndex
	case "team":
		return c.PathLists.ProtoDirectoryIndexes.TeamProtoDirIndex
	case "mail":
		return c.PathLists.ProtoDirectoryIndexes.MailProtoDirIndex
	case "robot":
		return c.PathLists.ProtoDirectoryIndexes.RobotProtoDirIndex
	default:
		return -1
	}
}

// GetNodeType 获取节点类型值
func (c *Config) GetNodeType(name string) uint32 {
	switch name {
	case "tcp":
		return c.PathLists.NodeTypes.TcpNode
	case "grpc":
		return c.PathLists.NodeTypes.GrpcNode
	case "http":
		return c.PathLists.NodeTypes.HttpNode
	default:
		return uint32(0)
	}
}
