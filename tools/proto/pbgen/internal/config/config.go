package config

// ----------- 项目根目录及相关路径 -----------
const (
	OutputRoot                = "../../../"
	ProtoDir                  = OutputRoot + "proto/"
	ProtoParentIncludePathDir = OutputRoot
	ThirdPartyDirectory       = "third_party/"
	GrpcDirectory             = OutputRoot + ThirdPartyDirectory + "grpc/"
	ProtoBufferDirectory      = GrpcDirectory + "third_party/protobuf/src/"
)

// ----------- Proto 文件扩展名 -----------
const (
	ProtoEx            = ".proto"
	ProtoPbcEx         = ".pb.cc"
	ProtoPbhEx         = ".pb.h"
	ProtoGoEx          = ".pb.go"
	GrpcPbcEx          = ".grpc.pb.cc"
	GrpcPbhEx          = ".grpc.pb.h"
	ProtoDescExtension = ".desc"
	ProtoDirName       = "proto/"
)

// ----------- 通用文件扩展名 -----------
const (
	HeaderExtension               = ".h"
	CppExtension                  = ".cpp"
	LuaCppExtension               = "_lua.cpp"
	HandlerHeaderExtension        = "_handler.h"
	HandlerCppExtension           = "_handler.cpp"
	RepliedHandlerHeaderExtension = "_replied_handler.h"
	CppRepliedHandlerEx           = "_replied_handler.cpp"
	CppSol2Extension              = "_sol2.cpp"
	CppSystemExtension            = "_system.cpp"
	GrpcClientExtension           = "_grpc_client"
	GrpcClientHeaderExtension     = GrpcClientExtension + HeaderExtension
	GrpcClientCppExtension        = GrpcClientExtension + CppExtension
)

// ----------- 消息与方法相关名字 -----------
const (
	MessageIdName   = "MessageId"
	MethodIndexName = "Index"
)

// ----------- 项目目录路径 -----------
const (
	NodeGoDirectory = OutputRoot + "go/"

	NodePkgDirectory               = "node_pkg/"
	NodePkgLogicDirectory          = NodePkgDirectory + "node_logic/"
	NodePkgLogicSceneNodeDirectory = NodePkgLogicDirectory + "scene_node/"
	GameNodeDirectory              = "node/scene/"
	CentreNodeDirectory            = "node/centre/"
	GateNodeDirectory              = "node/gate/"
	RobotDirectory                 = "robot/"
	PbcProtoDirectory              = "pkg/pbc/src/"
	CommonSrcDirectory             = "pkg/common/src/"

	ServiceIdFilePath           = ProtoDir + "serviceid.txt"
	RpcHandlerSourceDirectory   = "src/handler/rpc/"
	RpcResponseSourceDirectory  = "src/rpc_response/"
	EventHandlerSourceDirectory = "src/handler/event/"
	PbcLuaDirectory             = PbcOutputDirectory + "lua/"
)

// ----------- 方法处理目录 -----------
const (
	RobotMethodHandlerDirectory                 = OutputRoot + RobotDirectory + "logic/handler/"
	GameNodeMethodHandlerDirectory              = OutputRoot + GameNodeDirectory + RpcHandlerSourceDirectory
	GameNodePlayerMethodHandlerDirectory        = OutputRoot + GameNodeDirectory + RpcHandlerSourceDirectory + TypePlayer + "/"
	GameNodeMethodRepliedHandlerDirectory       = OutputRoot + GameNodeDirectory + RpcResponseSourceDirectory
	GameNodePlayerMethodRepliedHandlerDirectory = OutputRoot + GameNodeDirectory + RpcResponseSourceDirectory + TypePlayer + "/"
	CentreNodeMethodHandlerDirectory            = OutputRoot + CentreNodeDirectory + RpcHandlerSourceDirectory
	CentreNodePlayerMethodHandlerDirectory      = OutputRoot + CentreNodeDirectory + RpcHandlerSourceDirectory + TypePlayer + "/"
	CentreMethodRepliedHandleDir                = OutputRoot + CentreNodeDirectory + RpcResponseSourceDirectory
	CentrePlayerMethodRepliedHandlerDirectory   = OutputRoot + CentreNodeDirectory + RpcResponseSourceDirectory + TypePlayer + "/"
	GateMethodHandlerDirectory                  = OutputRoot + GateNodeDirectory + RpcHandlerSourceDirectory
	GateMethodRepliedHandlerDirectory           = OutputRoot + GateNodeDirectory + RpcResponseSourceDirectory
	GateNodePlayerMethodHandlerDirectory        = OutputRoot + GateNodeDirectory + RpcHandlerSourceDirectory + TypePlayer + "/"
	GateNodePlayerMethodRepliedHandlerDirectory = OutputRoot + GateNodeDirectory + RpcResponseSourceDirectory + TypePlayer + "/"
)

// ----------- 输出目录 -----------
const (
	ToolDir               = "tools/"
	TempFileGenerationDir = OutputRoot + ToolDir + "generated/temp/files/"
	PbDescDirectory       = TempFileGenerationDir + "proto_desc/"
	AllInOneProtoDescFile = PbDescDirectory + "all_in_one.pb.desc"

	PbcProtoOutputDirectory  = OutputRoot + PbcProtoDirectory + ProtoDirName
	PbcOutputDirectory       = OutputRoot + PbcProtoDirectory
	GrpcOutputDirectory      = OutputRoot + PbcProtoDirectory
	GrpcTempDirectory        = TempFileGenerationDir + "grpc/"
	PbcTempDirectory         = TempFileGenerationDir
	GrpcProtoOutputDirectory = OutputRoot + PbcProtoDirectory + ProtoDirName

	RobotGoOutputDirectory = OutputRoot + RobotDirectory
	GoPbGameDirectory      = "pb/game/"
	RobotGoGamePbDirectory = RobotGoOutputDirectory + GoPbGameDirectory
	MessageIdGoFile        = "message_id.go"
	RobotMessageIdFilePath = RobotGoGamePbDirectory + MessageIdGoFile

	LoginDirectory       = OutputRoot + "go/login/"
	LoginGoGameDirectory = LoginDirectory + GoPbGameDirectory
	LoginMessageIdGoFile = LoginGoGameDirectory + MessageIdGoFile
	DbGoDirectory        = OutputRoot + "go/db/"

	ServiceInfoName       = "service_info/"
	ServiceInfoDirectory  = PbcOutputDirectory + ServiceInfoName
	ServiceInfoExtension  = "_service_info"
	ServiceCppFilePath    = ServiceInfoDirectory + "service_info.cpp"
	ServiceHeaderFilePath = ServiceInfoDirectory + "service_info.h"

	LuaServiceFilePath       = PbcLuaDirectory + "lua_service.cpp"
	ClientLuaDirectory       = OutputRoot + "bin/script/lua/service/"
	ClientLuaServiceFilePath = OutputRoot + "client/src/handler/service_lua.cpp"

	GrpcGeneratorDirectory = "grpc/generator/"

	RobotMessageBodyHandlerDirectory = RobotMethodHandlerDirectory + "message_body_handler.go"
	PlayerStorageSystemDirectory     = OutputRoot + NodePkgLogicSceneNodeDirectory + "src/player/system/"
	CppGenGrpcDirectory              = OutputRoot + PbcProtoDirectory + GrpcGeneratorDirectory
	PlayerStorageTempDirectory       = TempFileGenerationDir + "/node/game/game_logic/"
	GrpcInitFileCppPath              = OutputRoot + PbcProtoDirectory + GrpcGeneratorDirectory + "grpc_init.cpp"
	GrpcInitFileHeadPath             = OutputRoot + PbcProtoDirectory + GrpcGeneratorDirectory + "grpc_init.h"

	GeneratorDirectory     = "generator/"
	UtilGeneratorDirectory = GeneratorDirectory + "util/"
	GenUtilFileCppPath     = OutputRoot + CommonSrcDirectory + UtilGeneratorDirectory + "gen_util.cpp"
	GenUtilFileHeadPath    = OutputRoot + CommonSrcDirectory + UtilGeneratorDirectory + "gen_util.h"
)

// ----------- 文件名 -----------
const (
	HandlerFileName                    = "Handler"
	RepliedHandlerFileName             = "RepliedHandler"
	EventHandlerFileNameBase           = "event_handler"
	EventHandlerHeaderFileName         = EventHandlerFileNameBase + HeaderExtension
	EventHandlerCppFileName            = EventHandlerFileNameBase + CppExtension
	RegisterRepliedHandlerCppExtension = "register" + CppRepliedHandlerEx
	RegisterHandlerCppExtension        = "register" + HandlerCppExtension
	DbProtoFileName                    = "db_base.proto"
	PlayerDataLoaderName               = "player_data_loader_system.h"
)

// ----------- 代码相关字符串常量 -----------
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
	PlayerServiceName        = "player_service.cpp"
	PlayerRepliedServiceName = "player_service_replied.cpp"
	CompleteQueueName        = "CompleteQueue"
)

// ----------- 项目源目录相关 -----------
var (
	ProtoDirectoryNames = [...]string{
		"common/",
		"logic/component/",
		"logic/event/",
		"logic/shared/",
		"logic/",
		"player_locator/",
		"logic/constants/",
		"etcd/",
		"login/",
		"db/",
		"centre/",
		"scene/",
		"gate/",
		"chat/",
		"team/",
		"mail/",
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
	SceneProtoDirIndex          = 11
	GateProtoDirIndex           = 12
	ChatProtoDirIndex           = 13
	TeamProtoDirIndex           = 14
	MailProtoDirIndex           = 15
)

// ----------- 其他业务相关常量 -----------
const (
	DatabasePrefixName              = "db"
	ClientPrefixName                = "Client" //
	TypePlayer                      = "player"
	DisplayPlayer                   = "Player"
	ServiceIncludeName              = "rpc/"
	PlayerServiceIncludeName        = "#include \"rpc/player_service.h\"\n"
	PlayerServiceRepliedIncludeName = "#include \"rpc/player_service_replied.h\"\n"
	MacroReturnIncludeName          = "#include \"macros/return_define.h\""
	ClientLuaProjectRelative        = "script/lua/rpc/"
	ClassNameSuffix                 = "Handler"
	NodeEnumName                    = "eNodeType"
	NodeServiceSuffix               = "nodeservice"
)

// ----------- 事件处理目录 -----------
const (
	GameNodeEventHandlerDirectory   = OutputRoot + GameNodeDirectory + EventHandlerSourceDirectory
	CentreNodeEventHandlerDirectory = OutputRoot + CentreNodeDirectory + EventHandlerSourceDirectory
)

// ----------- 玩家数据库名称 -----------
const (
	PlayerDatabaseName  = "player_database"
	PlayerDatabaseName1 = "player_database_1"
)

var GrpcServices = map[string]bool{
	"login":          true,
	"team":           true,
	"mail":           true,
	"chat":           true,
	"etcd":           true,
	"player_locator": true,
	"db":             true,
}

const (
	TcpNode  = 0
	GrpcNode = 1
	HttpNode = 2
)
