package config

// ----------- 项目根目录及相关路径 -----------
// 根路径保持不变
const (
	OutputRoot = "../../../"
)

// ----------- Proto 文件扩展名 -----------
const (
	ProtoExt     = ".proto"
	ProtoDirName = "proto/"
)

// ----------- 通用文件扩展名 -----------
const (
	HandlerHeaderExtension        = "_handler.h"
	HandlerCppExtension           = "_handler.cpp"
	RepliedHandlerHeaderExtension = "_response_handler.h"
	CppRepliedHandlerEx           = "_response_handler.cpp"
	ModelSqlExtension             = "mysql_database_table.sql"
)

// ----------- 项目目录路径 -----------
const (
	ModelPath                      = "model/"
	NodeCppDirectory               = OutputRoot + "cpp/"
	NodePkgDirectory               = "cpp/libs/"
	NodeLibGameDirectory           = NodePkgDirectory + "services/"
	NodePkgLogicSceneNodeDirectory = NodeLibGameDirectory + "scene/"
	RoomNodeDirectory              = NodeCppDirectory + "nodes/scene/"
	CentreNodeDirectory            = NodeCppDirectory + "nodes/centre/"
	GateNodeDirectory              = NodeCppDirectory + "nodes/gate/"
	RobotDirectory                 = "tools/robot/"
	GeneratedDirectory             = "cpp/generated/"

	RpcHandlerSourceDirectory   = "handler/rpc/"
	RpcResponseSourceDirectory  = "rpc_replies/"
	EventHandlerSourceDirectory = "handler/event/"
)

// ----------- 方法处理目录 -----------
const (
	RobotMethodHandlerDirectory                 = OutputRoot + RobotDirectory + "logic/handler/"
	RoomNodeMethodHandlerDirectory              = RoomNodeDirectory + RpcHandlerSourceDirectory
	RoomNodePlayerMethodHandlerDirectory        = RoomNodeDirectory + RpcHandlerSourceDirectory + TypePlayer + "/"
	RoomNodeMethodRepliedHandlerDirectory       = RoomNodeDirectory + RpcResponseSourceDirectory
	RoomNodePlayerMethodRepliedHandlerDirectory = RoomNodeDirectory + RpcResponseSourceDirectory + TypePlayer + "/"
	CentreNodeMethodHandlerDirectory            = CentreNodeDirectory + RpcHandlerSourceDirectory
	CentreNodePlayerMethodHandlerDirectory      = CentreNodeDirectory + RpcHandlerSourceDirectory + TypePlayer + "/"
	CentreMethodRepliedHandleDir                = CentreNodeDirectory + RpcResponseSourceDirectory
	CentrePlayerMethodRepliedHandlerDirectory   = CentreNodeDirectory + RpcResponseSourceDirectory + TypePlayer + "/"
	GateMethodHandlerDirectory                  = GateNodeDirectory + RpcHandlerSourceDirectory
	GateMethodRepliedHandlerDirectory           = GateNodeDirectory + RpcResponseSourceDirectory
	GateNodePlayerMethodHandlerDirectory        = GateNodeDirectory + RpcHandlerSourceDirectory + TypePlayer + "/"
	GateNodePlayerMethodRepliedHandlerDirectory = GateNodeDirectory + RpcResponseSourceDirectory + TypePlayer + "/"
)

// ----------- 输出目录 -----------
// 更新后的路径
const (
	ProtoGoPackageSuffix  = "_proto"
	GoPackage             = "generated/pb/game"
	GoRobotPackage        = "robot"
	ToolDir               = "tools/"
	TempFileGenerationDir = OutputRoot + ToolDir + "generated/temp/files/"
	PbDescDirectory       = TempFileGenerationDir + "proto_desc/"

	GoNodeDirectory = OutputRoot + "go/"

	PbcProtoOutputDirectory = OutputRoot + GeneratedDirectory

	GeneratedOutputDirectory = OutputRoot + GeneratedDirectory
	GrpcTempDirectory        = TempFileGenerationDir + "grpc/"
	PbcTempDirectory         = TempFileGenerationDir
	GrpcProtoOutputDirectory = OutputRoot + GeneratedDirectory + ProtoDirName

	RobotGoOutputDirectory          = OutputRoot + RobotDirectory
	RobotGoGamePbDirectory          = RobotGoOutputDirectory + GoPackage
	RobotGoOutputGeneratedDirectory = OutputRoot + RobotDirectory
	MessageIdGoFile                 = "message_id.go"
	RobotMessageIdFilePath          = RobotGoGamePbDirectory + "/" + MessageIdGoFile

	GoGeneratorDirectory = GoNodeDirectory

	GeneratedRpcName      = "rpc/"
	GeneratedRpcDirectory = GeneratedOutputDirectory + GeneratedRpcName
	ServiceInfoName       = "service_metadata/"
	ServiceInfoDirectory  = GeneratedRpcDirectory + ServiceInfoName
	ServiceInfoExtension  = "_service_metadata"
	ServiceCppFilePath    = ServiceInfoDirectory + "service_metadata.cpp"
	ServiceHeaderFilePath = ServiceInfoDirectory + "service_metadata.h"

	GrpcGeneratorDirectory = "grpc_client/"

	RobotMessageBodyHandlerDirectory = RobotMethodHandlerDirectory + "message_body_handler.go"
	PlayerStorageSystemDirectory     = OutputRoot + NodePkgLogicSceneNodeDirectory + "player/system/"
	CppGenGrpcDirectory              = OutputRoot + GeneratedDirectory + GrpcGeneratorDirectory
	PlayerStorageTempDirectory       = TempFileGenerationDir + "/node/game/storage/"
	GrpcInitFileCppPath              = OutputRoot + GeneratedDirectory + GrpcGeneratorDirectory + "grpc_init_client.cpp"
	GrpcInitFileHeadPath             = OutputRoot + GeneratedDirectory + GrpcGeneratorDirectory + "grpc_init_client.h"

	UtilGeneratorDirectory = OutputRoot + GeneratedDirectory + "proto_helpers/"
	GenUtilFileCppPath     = UtilGeneratorDirectory + "proto_util.cpp"
	GenUtilFileHeadPath    = UtilGeneratorDirectory + "proto_util.h"
)

// ----------- 文件名 -----------
const (
	HandlerFileName                    = "Handler"
	RepliedHandlerFileName             = "Reply"
	RegisterRepliedHandlerCppExtension = "register" + CppRepliedHandlerEx
	RegisterHandlerCppExtension        = "register" + HandlerCppExtension
	DbProtoFileName                    = "db_base.proto"
	PlayerDataLoaderName               = "player_data_loader.h"
	DbTableName                        = "mysql_database_table.proto"
	DBTableMessageListJson             = "mysql_database_table_list.json"
	GameRpcProtoName                   = "game_rpc.proto"
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
	EmptyResponseName        = "Empty"
	MysqlName                = "mysql"
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
