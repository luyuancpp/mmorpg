package config

// ----------- 项目根目录及相关路径 -----------
const (
	ProjectDir                = "../../../"
	ProtoDir                  = ProjectDir + "proto/"
	ProtoParentIncludePathDir = ProjectDir
	ThirdPartyDirectory       = "third_party/"
	GrpcDirectory             = ProjectDir + ThirdPartyDirectory + "grpc/"
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
	GrpcHeaderExtension           = "_grpc.h"
	GrpcCppExtension              = "_grpc.cpp"
	GrpcExtension                 = "_grpc"
)

// ----------- 消息与方法相关名字 -----------
const (
	MessageIdName   = "MessageId"
	MethodIndexName = "Index"
)

// ----------- 项目目录路径 -----------
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

// ----------- 方法处理目录 -----------
const (
	RobotMethodHandlerDirectory                 = ProjectDir + RobotDirectory + "logic/handler/"
	GameNodeMethodHandlerDirectory              = ProjectDir + GameNodeDirectory + HandlerSourceDirectory
	GameNodePlayerMethodHandlerDirectory        = ProjectDir + GameNodeDirectory + HandlerSourceDirectory + TypePlayer + "/"
	GameNodeMethodRepliedHandlerDirectory       = ProjectDir + GameNodeDirectory + RepliedHandlerSourceDirectory
	GameNodePlayerMethodRepliedHandlerDirectory = ProjectDir + GameNodeDirectory + RepliedHandlerSourceDirectory + TypePlayer + "/"
	CentreNodeMethodHandlerDirectory            = ProjectDir + CentreNodeDirectory + HandlerSourceDirectory
	CentreNodePlayerMethodHandlerDirectory      = ProjectDir + CentreNodeDirectory + HandlerSourceDirectory + TypePlayer + "/"
	CentreMethodRepliedHandleDir                = ProjectDir + CentreNodeDirectory + RepliedHandlerSourceDirectory
	CentrePlayerMethodRepliedHandlerDirectory   = ProjectDir + CentreNodeDirectory + RepliedHandlerSourceDirectory + TypePlayer + "/"
	GateMethodHandlerDirectory                  = ProjectDir + GateNodeDirectory + HandlerSourceDirectory
	GateMethodRepliedHandlerDirectory           = ProjectDir + GateNodeDirectory + RepliedHandlerSourceDirectory
	GateNodePlayerMethodHandlerDirectory        = ProjectDir + GateNodeDirectory + HandlerSourceDirectory + TypePlayer + "/"
	GateNodePlayerMethodRepliedHandlerDirectory = ProjectDir + GateNodeDirectory + RepliedHandlerSourceDirectory + TypePlayer + "/"
)

// ----------- 输出目录 -----------
const (
	ToolDir               = "tools/"
	TempFileGenerationDir = ProjectDir + ToolDir + "generated/temp/files/"
	PbDescDirectory       = TempFileGenerationDir + "proto_desc/"
	AllInOneProtoDescFile = PbDescDirectory + "all_in_one.pb.desc"

	PbcProtoOutputDirectory  = ProjectDir + PbcProtoDirectory + ProtoDirName
	PbcOutputDirectory       = ProjectDir + PbcProtoDirectory
	GrpcOutputDirectory      = ProjectDir + PbcProtoDirectory
	GrpcTempDirectory        = TempFileGenerationDir + "grpc/"
	PbcTempDirectory         = TempFileGenerationDir
	GrpcProtoOutputDirectory = ProjectDir + PbcProtoDirectory + ProtoDirName

	RobotGoOutputDirectory = ProjectDir + RobotDirectory
	GoPbGameDirectory      = "pb/game/"
	RobotGoGamePbDirectory = RobotGoOutputDirectory + GoPbGameDirectory
	MessageIdGoFile        = "message_id.go"
	RobotMessageIdFilePath = RobotGoGamePbDirectory + MessageIdGoFile

	LoginDirectory       = ProjectDir + "node/login/"
	DeployDirectory      = ProjectDir + "node/deploy/"
	LoginGoGameDirectory = LoginDirectory + GoPbGameDirectory
	LoginMessageIdGoFile = LoginGoGameDirectory + MessageIdGoFile
	DbGoDirectory        = ProjectDir + "node/db/"

	ServiceInfoName       = "service_info/"
	ServiceInfoDirectory  = PbcOutputDirectory + ServiceInfoName
	ServiceInfoExtension  = "_service_info"
	ServiceCppFilePath    = ServiceInfoDirectory + "service_info.cpp"
	ServiceHeaderFilePath = ServiceInfoDirectory + "service_info.h"

	LuaServiceFilePath       = PbcLuaDirectory + "lua_service.cpp"
	ClientLuaDirectory       = ProjectDir + "bin/script/lua/service/"
	ClientLuaServiceFilePath = ProjectDir + "client/src/handler/service_lua.cpp"

	GrpcGeneratorDirectory = "grpc/generator/"

	RobotMessageBodyHandlerDirectory = RobotMethodHandlerDirectory + "message_body_handler.go"
	PlayerStorageSystemDirectory     = ProjectDir + NodePkgLogicSceneNodeDirectory + "src/player/system/"
	CppGenGrpcDirectory              = ProjectDir + PbcProtoDirectory + GrpcGeneratorDirectory
	PlayerStorageTempDirectory       = TempFileGenerationDir + "/node/game/game_logic/"
	GrpcInitFileCppPath              = ProjectDir + PbcProtoDirectory + GrpcGeneratorDirectory + "grpc_init.cpp"
	GrpcInitFileHeadPath             = ProjectDir + PbcProtoDirectory + GrpcGeneratorDirectory + "grpc_init.h"
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
	GameMysqlDBProtoFileName           = "mysql_database_table.proto"
	LoginServiceProtoFileName          = "login_service.proto"
)

// ----------- 代码相关字符串常量 -----------
const (
	GoogleMethodController   = "::google::protobuf::RpcController* controller, "
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

// ----------- 项目源目录相关 -----------
var (
	ProtoDirectoryNames = [...]string{
		"common/",
		"logic/component/",
		"logic/event/",
		"logic/shared/",
		"logic/",
		"",
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
	EtcdDirIndex                = 5
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
// ----------- 其他业务相关常量 -----------
const (
	LoginPrefixName                 = "login"
	DeployPrefixName                = "deploy"
	DatabasePrefixName              = "db"
	GameNodePrefixName              = "game"
	GameNodePlayerPrefixName        = "game_player"
	CentrePrefixName                = "centre"
	CentrePlayerPrefixName          = "centre_player"
	ClientPrefixName                = "Client" //
	NodePrefixName                  = "node"   //
	TypePlayer                      = "player"
	DisplayPlayer                   = "Player"
	ServiceIncludeName              = "service/"
	PlayerServiceIncludeName        = "#include \"service/player_service.h\"\n"
	PlayerServiceRepliedIncludeName = "#include \"service/player_service_replied.h\"\n"
	MacroReturnIncludeName          = "#include \"macros/return_define.h\""
	ClientLuaProjectRelative        = "script/lua/service/"
	GatePrefixName                  = "gate"
	ClassNameSuffix                 = "Handler"
)

// ----------- 事件处理目录 -----------
const (
	GameNodeEventHandlerDirectory   = ProjectDir + GameNodeDirectory + EventHandlerSourceDirectory
	CentreNodeEventHandlerDirectory = ProjectDir + CentreNodeDirectory + EventHandlerSourceDirectory
)

// ----------- 玩家数据库名称 -----------
const (
	PlayerDatabaseName  = "player_database"
	PlayerDatabaseName1 = "player_database_1"
)

var GrpcServices = map[string]bool{
	"login": true,
	"team":  true,
	"mail":  true,
	"chat":  true,
	"etcd":  true,
}
