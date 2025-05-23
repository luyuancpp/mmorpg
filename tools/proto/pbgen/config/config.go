package config

// 常量：项目目录和路径
const (
	ProjectDir                = "../../../"
	ProtoDir                  = ProjectDir + "proto/"
	ProtoParentIncludePathDir = ProjectDir
	ProtoEx                   = ".proto"
	ProtoPbcEx                = ".pb.cc"
	ProtoPbhEx                = ".pb.h"
	ProtoGoEx                 = ".pb.go"
	GrpcPbcEx                 = ".grpc.pb.cc"
	GrpcPbhEx                 = ".grpc.pb.h"
	ProtoDescExtension        = ".desc"
	ProtoDirName              = "proto/"
	ThirdPartyDirectory       = "third_party/"
	GrpcDirectory             = ProjectDir + ThirdPartyDirectory + "grpc/"
	ProtoBufferDirectory      = GrpcDirectory + "third_party/protobuf/src/"
)

// 常量：文件扩展名
const (
	HeaderExtension               = ".h"
	CppExtension                  = ".cpp"
	LuaCppExtension               = "_lua.cpp"
	HandlerHeaderExtension        = "_handler.h"
	HandlerCppExtension           = "_handler.cpp"
	RepliedHandlerHeaderExtension = "_replied_handler.h"
	CppRepliedHandlerEx           = "_replied_handler.cpp"
	CppSol2Extension              = "_sol2.cpp"
	MessageIdName                 = "MessageId"
	MethodIndexName               = "Index"
	CppSystemExtension            = "_system.cpp"
	GrpcHeaderExtension           = "_grpc.h"
	GrpcCppExtension              = "_grpc.cpp"
	GrpcExtension                 = "_grpc"
)

// 常量：目录路径
const (
	NodePkgDirectory               = "node_pkg/"
	NodePkgLogicDirectory          = NodePkgDirectory + "node_logic/"
	NodePkgLogicSceneNodeDirectory = NodePkgLogicDirectory + "scene_node/"
	GameNodeDirectory              = "node/scene/"
	CentreNodeDirectory            = "node/centre/"
	GateNodeDirectory              = "node/gate/"
	RobotDirectory                 = "robot/"
	PbcProtoDirectory              = "pkg/pbc/src/"
	ServiceIdFilePath              = ProtoDir + "serviceid.txt"
	HandlerSourceDirectory         = "src/handler/service/"
	RepliedHandlerSourceDirectory  = "src/handler/service_replied/"
	EventHandlerSourceDirectory    = "src/handler/event/"
	PbcLuaDirectory                = PbcOutputDirectory + "lua/"
)

// 常量：方法处理目录
const (
	RobotMethodHandlerDirectory                 = ProjectDir + RobotDirectory + "logic/handler/"
	GameNodeMethodHandlerDirectory              = ProjectDir + GameNodeDirectory + HandlerSourceDirectory
	GameNodePlayerMethodHandlerDirectory        = ProjectDir + GameNodeDirectory + HandlerSourceDirectory + PlayerName + "/"
	GameNodeMethodRepliedHandlerDirectory       = ProjectDir + GameNodeDirectory + RepliedHandlerSourceDirectory
	GameNodePlayerMethodRepliedHandlerDirectory = ProjectDir + GameNodeDirectory + RepliedHandlerSourceDirectory + PlayerName + "/"
	CentreNodeMethodHandlerDirectory            = ProjectDir + CentreNodeDirectory + HandlerSourceDirectory
	CentreNodePlayerMethodHandlerDirectory      = ProjectDir + CentreNodeDirectory + HandlerSourceDirectory + PlayerName + "/"
	CentreMethodRepliedHandleDir                = ProjectDir + CentreNodeDirectory + RepliedHandlerSourceDirectory
	CentrePlayerMethodRepliedHandlerDirectory   = ProjectDir + CentreNodeDirectory + RepliedHandlerSourceDirectory + PlayerName + "/"
	GateMethodHandlerDirectory                  = ProjectDir + GateNodeDirectory + HandlerSourceDirectory
	GateMethodRepliedHandlerDirectory           = ProjectDir + GateNodeDirectory + RepliedHandlerSourceDirectory
	GateNodePlayerMethodHandlerDirectory        = ProjectDir + GateNodeDirectory + HandlerSourceDirectory + PlayerName + "/"
	GateNodePlayerMethodRepliedHandlerDirectory = ProjectDir + GateNodeDirectory + RepliedHandlerSourceDirectory + PlayerName + "/"
)

// 常量：输出目录
const (
	ToolDir                          = "tools/"
	TempFileGenerationDir            = ProjectDir + ToolDir + "generated/temp/files/"
	PbDescDirectory                  = TempFileGenerationDir + "proto_desc/"
	PbcProtoOutputDirectory          = ProjectDir + PbcProtoDirectory + ProtoDirName
	PbcOutputDirectory               = ProjectDir + PbcProtoDirectory
	GrpcOutputDirectory              = ProjectDir + PbcProtoDirectory
	GrpcTempDirectory                = TempFileGenerationDir + "grpc/"
	PbcTempDirectory                 = TempFileGenerationDir
	GrpcProtoOutputDirectory         = ProjectDir + PbcProtoDirectory + ProtoDirName
	RobotGoOutputDirectory           = ProjectDir + RobotDirectory
	GoPbGameDirectory                = "pb/game/"
	RobotGoGamePbDirectory           = RobotGoOutputDirectory + GoPbGameDirectory
	MessageIdGoFile                  = "message_id.go"
	RobotMessageIdFilePath           = RobotGoGamePbDirectory + MessageIdGoFile
	LoginDirectory                   = ProjectDir + "node/login/"
	DeployDirectory                  = ProjectDir + "node/deploy/"
	LoginGoGameDirectory             = LoginDirectory + GoPbGameDirectory
	LoginMessageIdGoFile             = LoginGoGameDirectory + MessageIdGoFile
	DbGoDirectory                    = ProjectDir + "node/db/"
	ServiceInfoName                  = "service_info/"
	ServiceInfoDirectory             = PbcOutputDirectory + ServiceInfoName
	ServiceInfoExtension             = "_service_info"
	ServiceCppFilePath               = ServiceInfoDirectory + "service_info.cpp"
	ServiceHeaderFilePath            = ServiceInfoDirectory + "service_info.h"
	LuaServiceFilePath               = PbcLuaDirectory + "lua_service.cpp"
	ClientLuaDirectory               = ProjectDir + "bin/script/lua/service/"
	ClientLuaServiceFilePath         = ProjectDir + "client/src/handler/service_lua.cpp"
	ClientServiceInstanceFilePath    = ProjectDir + "client/src/handler/player_service.cpp"
	RobotMessageBodyHandlerDirectory = RobotMethodHandlerDirectory + "message_body_handler.go"
	PlayerStorageSystemDirectory     = ProjectDir + NodePkgLogicSceneNodeDirectory + "src/player/system/"
	CppGenGrpcDirectory              = ProjectDir + PbcProtoDirectory + "grpc/generator/"
	PlayerStorageTempDirectory       = TempFileGenerationDir + "/node/game/game_logic/"
)

// 常量：文件名
const (
	HandlerFileName                    = "Handler"
	RepliedHandlerFileName             = "RepliedHandler"
	EventHandlerFileNameBase           = "event_handler"
	EventHandlerHeaderFileName         = EventHandlerFileNameBase + HeaderExtension
	EventHandlerCppFileName            = EventHandlerFileNameBase + CppExtension
	RegisterRepliedHandlerCppExtension = "register" + CppRepliedHandlerEx
	RegisterHandlerCppExtension        = "register" + HandlerCppExtension
	DbProtoFileName                    = "db_base.proto"
	GameMysqlDBProtoFileName           = "mysql_database_table.proto"
	LoginServiceProtoFileName          = "login_service.proto"
)

// 常量：代码相关
const (
	GoogleMethodController   = "::google::protobuf::RpcController* controller,"
	PlayerMethodController   = "entt::entity player,"
	IncludeEndLine           = "\"\n"
	IncludeBegin             = "#include \""
	Tab                      = "	"
	Tab2                     = Tab + Tab
	Tab3                     = Tab + Tab + Tab
	Tab4                     = Tab + Tab + Tab + Tab
	YourCodeBegin            = "///<<< BEGIN WRITING YOUR CODE"
	YourCodeEnd              = "///<<< END WRITING YOUR CODE"
	YourCodePair             = YourCodeBegin + "\n" + YourCodeEnd + "\n"
	CcGenericServices        = "cc_generic_services"
	EmptyResponseName        = "Empty"
	MysqlName                = "mysql"
	PlayerServiceName        = "player_service.cpp"
	PlayerRepliedServiceName = "player_service_replied.cpp"
)

// 常量：项目源目录
var (
	ProtoDirectoryNames = [...]string{
		"common/",
		"logic/component/",
		"logic/event/",
		"logic/server/",
		"logic/client_player/",
		"logic/server_player/",
		"logic/constants/",
		"etcd/",
	}

	ProtoDirs []string

	CommonProtoDirIndex    = 0
	ComponentProtoDirIndex = 1
	EventProtoDirIndex     = 2
	LogicProtoDirIndex     = 3
	ClientPlayerDirIndex   = 4
	ServerPlayerDirIndex   = 5
	ConstantsDirIndex      = 6
	EtcdProtoDirIndex      = 7
)

// 常量：其他常量
const (
	LoginPrefixName                 = "login"
	DeployPrefixName                = "deploy"
	DatabasePrefixName              = "db"
	GameNodePrefixName              = "game"
	GameNodePlayerPrefixName        = "game_player"
	CentrePrefixName                = "centre"
	CentrePlayerPrefixName          = "centre_player"
	PlayerName                      = "player"
	ServiceIncludeName              = "service/"
	PlayerServiceIncludeName        = "#include \"service/player_service.h\"\n"
	PlayerServiceRepliedIncludeName = "#include \"service/player_service_replied.h\"\n"
	MacroReturnIncludeName          = "#include \"macros/return_define.h\""
	ClientLuaProjectRelative        = "script/lua/service/"
	GatePrefixName                  = "gate"
	ClassNameSuffix                 = "Handler"
)

const (
	GameNodeEventHandlerDirectory   = ProjectDir + GameNodeDirectory + EventHandlerSourceDirectory
	CentreNodeEventHandlerDirectory = ProjectDir + CentreNodeDirectory + EventHandlerSourceDirectory
)

const (
	PlayerDatabaseName  = "player_database"
	PlayerDatabaseName1 = "player_database_1"
)
