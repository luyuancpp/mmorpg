package config

// 常量：项目目录和路径
const (
	ProjectDir    = "../../../../"
	ProtoDir      = "../../../../proto/"
	Md5Ex         = ".md5"
	Md5Dir        = "../bin/md5/"
	ProtoEx       = ".proto"
	ProtoPbcEx    = ".pb.cc"
	ProtoPbhEx    = ".pb.h"
	ProtoGoEx     = ".pb.go"
	GrpcPbcEx     = ".grpc.pb.cc"
	GrpcEx        = "grpc"
	ClientGoMd5Ex = "client_go"
	LoginGoMd5Ex  = "login_go"
	DBGoMd5Ex     = "db_go"
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
	MessageIdName                 = "MsgId"
	MethodIndexName               = "Index"
)

// 常量：目录路径
const (
	GameNodeDirectory             = "node/game/"
	CentreNodeDirectory           = "node/centre/"
	GateNodeDirectory             = "node/gate/"
	ClientDirectory               = "client/"
	PbcProtoDirectory             = "pkg/pbc/src/proto/"
	ServiceIdFilePath             = ProtoDir + "serviceid.txt"
	HandlerSourceDirectory        = "src/handler/service/"
	RepliedHandlerSourceDirectory = "src/handler/service_replied/"
	EventHandlerSourceDirectory   = "src/handler/event/"
	PbcLuaDirectory               = PbcOutputDirectory + "lua/"
)

// 常量：输出目录
const (
	PbcOutputDirectory        = ProjectDir + PbcProtoDirectory
	GrpcOutputDirectory       = ProjectDir + PbcProtoDirectory
	ClientGoOutDir            = ProjectDir + ClientDirectory
	GoPbGameDir               = "pb/game/"
	ClientGoGamePbDir         = ClientGoOutDir + GoPbGameDir
	MessageIdGo               = "message_id.go"
	ClientMessageIdFile       = ClientGoGamePbDir + MessageIdGo
	LoginDir                  = ProjectDir + "node/login/"
	LoginGoGameDir            = LoginDir + GoPbGameDir
	LoginMessageIdGo          = LoginGoGameDir + MessageIdGo
	DbGoDir                   = ProjectDir + "node/db/"
	DBGoGameDir               = DbGoDir + GoPbGameDir
	ServiceInfoDirName        = PbcOutputDirectory + "service_info/"
	ServiceInfoExtName        = "_service_info"
	ServiceCppFileName        = ServiceInfoDirName + "service_info.cpp"
	ServiceHeadFileName       = ServiceInfoDirName + "service_info.h"
	LuaServiceFileName        = PbcLuaDirectory + "lua_service.cpp"
	ClientLuaDir              = ProjectDir + "bin/script/lua/service/"
	ClientLuaServiceFile      = ProjectDir + "client/src/handler/service_lua.cpp"
	ClientServiceInstanceFile = ProjectDir + "client/src/handler/player_service.cpp"
)

// 常量：文件名
const (
	HandlerName                 = "Handler"
	RepliedHandlerName          = "RepliedHandler"
	EventHandlerFileName        = "event_handler"
	EventHandlerFileNameHead    = EventHandlerFileName + HeaderExtension
	EventHandlerFileNameCpp     = EventHandlerFileName + CppExtension
	RegisterRepliedHandlerCppEx = "register" + CppRepliedHandlerEx
	RegisterHandlerCppEx        = "register" + HandlerCppExtension
	DbProtoName                 = "db_base.proto"
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
	C2SMethodContainsName    = "C2S"
	MysqlName                = "mysql"
	PlayerServiceName        = "player_service.cpp"
	PlayerRepliedServiceName = "player_service_replied.cpp"
)

// 常量：项目源目录
var (
	SourceDirs = [...]string{
		"pkg/common/",
		"pkg/pbc/",
		"node/centre/",
		"node/game/",
		"node/gate/",
		"client/",
	}

	ProtoDirNames = [...]string{
		"common/",
		"logic/component/",
		"logic/event/",
		"logic/server/",
		"logic/client_player/",
		"logic/server_player/",
		"logic/constants/",
	}

	ProjectSourceDirs    []string
	ProjectSourceMd5Dirs []string
	ProtoDirs            []string
	ProtoMd5Dirs         []string

	CommonProtoDirIndex    = 0
	ComponentProtoDirIndex = 1
	EventProtoDirIndex     = 2
	LogicProtoDirIndex     = 3
	ClientPlayerDirIndex   = 4
	ServerPlayerDirIndex   = 5
)

// 常量：方法处理目录
const (
	ClientMethodHandleDir = ProjectDir + ClientDirectory + HandlerSourceDirectory

	GsMethodHandleDir                  = ProjectDir + GameNodeDirectory + HandlerSourceDirectory
	GsPlayerMethodHandleDir            = ProjectDir + GameNodeDirectory + HandlerSourceDirectory + PlayerName + "/"
	GsMethodRepliedHandleDir           = ProjectDir + GameNodeDirectory + RepliedHandlerSourceDirectory
	GsPlayerMethodRepliedHandleDir     = ProjectDir + GameNodeDirectory + RepliedHandlerSourceDirectory + PlayerName + "/"
	CentreMethodHandleDir              = ProjectDir + CentreNodeDirectory + HandlerSourceDirectory
	CentrePlayerMethodHandleDir        = ProjectDir + CentreNodeDirectory + HandlerSourceDirectory + PlayerName + "/"
	CentreMethodRepliedHandleDir       = ProjectDir + CentreNodeDirectory + RepliedHandlerSourceDirectory
	CentrePlayerMethodRepliedHandleDir = ProjectDir + CentreNodeDirectory + RepliedHandlerSourceDirectory + PlayerName + "/"
	GateMethodHandleDir                = ProjectDir + GateNodeDirectory + HandlerSourceDirectory
	GateMethodRepliedHandleDir         = ProjectDir + GateNodeDirectory + RepliedHandlerSourceDirectory
)

// 常量：其他常量
const (
	LoginPrefixName                 = "login"
	DeployPrefixName                = "deploy"
	DatabasePrefixName              = "db"
	GsPrefixName                    = "game"
	GsPlayerPrefixName              = "game_player"
	CentrePrefixName                = "centre"
	CentrePlayerPrefixName          = "centre_player"
	PlayerName                      = "player"
	PlayerServiceIncludeName        = "#include \"player_service.h\"\n"
	PlayerServiceRepliedIncludeName = "#include \"player_service_replied.h\"\n"
	MacroReturnIncludeName          = "#include \"macros/return_define.h\""
	ClientLuaProjectRelative        = "script/lua/service/"
)

const (
	GsEventHandleDir     = ProjectDir + GameNodeDirectory + EventHandlerSourceDirectory
	CentreEventHandleDir = ProjectDir + CentreNodeDirectory + EventHandlerSourceDirectory
)
