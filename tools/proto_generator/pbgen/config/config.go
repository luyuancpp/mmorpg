package config

// ----------- 项目根目录及相关路径 -----------
// 根路径保持不变
const (
	OutputRoot = "../../../"
)

// ----------- Proto 文件扩展名 -----------

// ----------- 通用文件扩展名 -----------
const (
	HandlerCppExtension           = "_handler.cpp"
	RepliedHandlerHeaderExtension = "_response_handler.h"
	CppRepliedHandlerEx           = "_response_handler.cpp"
	ModelSqlExtension             = "mysql_database_table.sql"
)

// ----------- 项目目录路径 -----------
const (
	ModelPath                   = "model/"
	NodeCppDirectory            = OutputRoot + "cpp/"
	RoomNodeDirectory           = NodeCppDirectory + "nodes/scene/"
	CentreNodeDirectory         = NodeCppDirectory + "nodes/centre/"
	EventHandlerSourceDirectory = "handler/event/"
)

// ----------- 输出目录 -----------
// 更新后的路径
const (
	ProtoGoPackageSuffix = "_proto"
	GoPackage            = "generated/pb/game"
	GoRobotPackage       = "robot"
	MessageIdGoFile      = "message_id.go"
	GeneratedRpcName     = "rpc/"
	ServiceInfoName      = "service_metadata/"
	ServiceInfoExtension = "_service_metadata"
)

// ----------- 文件名 -----------
const (
	RegisterRepliedHandlerCppExtension = "register" + CppRepliedHandlerEx
	RegisterHandlerCppExtension        = "register" + HandlerCppExtension
	PlayerDataLoaderName               = "player_data_loader.h"
)

// ----------- 代码相关字符串常量 -----------
// 这些字符串常量保持不变
const (
	GoogleMethodController   = "::google::protobuf::RpcController* controller, "
	PlayerMethodController   = "entt::entity player,"
	IncludeEndLine           = "\"\n"
	IncludeBegin             = "#include \""
	YourCodeBegin            = "///<<< BEGIN WRITING YOUR CODE"
	YourCodeEnd              = "///<<< END WRITING YOUR CODE"
	YourCodePair             = YourCodeBegin + "\n" + YourCodeEnd + "\n"
	PlayerServiceName        = "player_service_interface.cpp"
	PlayerRepliedServiceName = "player_rpc_response_handler.cpp"
)

// ----------- 项目源目录相关 -----------
var (
	ProtoDirectoryNames = [...]string{
		"common/",
		"logic/component/",
		"logic/event/",
		"logic/",
		"logic/",
		"service/go/grpc/player_locator/",
		"logic/constants/",
		"middleware/etcd/",
		"service/go/grpc/login/",
		"service/go/grpc/db/",
		"service/cpp/rpc/centre/",
		"service/cpp/rpc/scene/",
		"service/cpp/rpc/gate/",
		"service/go/grpc/chat/",
		"service/go/grpc/team/",
		"service/go/grpc/mail/",
		"logic/database/",
	}

	ProtoDirs []string

	CommonProtoDirIndex         = 0
	LogicComponentProtoDirIndex = 1
	LogicEventProtoDirIndex     = 2
	LogicSharedProtoDirIndex    = 3
	LogicProtoDirIndex          = 4
	PlayerLocatorDirIndex       = 5
	ConstantsDirIndex           = 6
	EtcdProtoDirIndex           = 7
	LoginProtoDirIndex          = 8
	DbProtoDirIndex             = 9
	CenterProtoDirIndex         = 10
	RoomProtoDirIndex           = 11
	GateProtoDirIndex           = 12
	ChatProtoDirIndex           = 13
	TeamProtoDirIndex           = 14
	MailProtoDirIndex           = 15

	// gRPC 支持的语言列表
	GrpcLanguages = [...]string{"go", "python", "java", "csharp", "ruby", "node", "php", "objc", "swift"}
)

// ----------- 其他业务相关常量 -----------
const (
	TypePlayer                      = "player"
	ServiceIncludeName              = "rpc/"
	PlayerServiceIncludeName        = "#include \"rpc/player_service_interface.h\"\n"
	PlayerServiceRepliedIncludeName = "#include \"rpc/player_rpc_response_handler.h\"\n"
	MacroReturnIncludeName          = "#include \"macros/return_define.h\""
	NodeEnumName                    = "eNodeType"
	NodeServiceSuffix               = "nodeservice"
)

// ----------- 事件处理目录 -----------
const (
	RoomNodeEventHandlerDirectory   = RoomNodeDirectory + EventHandlerSourceDirectory
	CentreNodeEventHandlerDirectory = CentreNodeDirectory + EventHandlerSourceDirectory
)

const (
	TcpNode  = 0
	GrpcNode = 1
	HttpNode = 2
)
