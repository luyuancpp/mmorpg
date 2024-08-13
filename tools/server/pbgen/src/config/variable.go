package config

const ProjectDir = "../../../../"
const ProtoDir = "../../../../proto/"
const Md5Ex = ".md5"
const Md5Dir = "../bin/md5/"
const ProtoEx = ".proto"
const ProtoPbcEx = ".pb.cc"
const ProtoPbhEx = ".pb.h"
const ProtoGoEx = ".pb.go"
const GrpcPbcEx = ".grpc.pb.cc"
const GrpcEx = "grpc"
const ClientGoMd5Ex = "client_go"
const LoginGoMd5Ex = "login_go"
const DBGoMd5Ex = "db_go"
const HeadEx = ".h"
const CppEx = ".cpp"
const LuaCppEx = "_lua.cpp"
const HeadHandlerEx = "_handler.h"
const CppHandlerEx = "_handler.cpp"
const HeadRepliedHandlerEx = "_replied_handler.h"
const CppRepliedHandlerEx = "_replied_handler.cpp"
const CppSol2Ex = "_sol2.cpp"
const MessageIdName = "MsgId"
const MethodIndexName = "Index"
const PlayerName = "player"
const GoogleMethodController = "::google::protobuf::RpcController* controller,"
const PlayerMethodController = "entt::entity player,"
const IncludeEndLine = "\"\n"
const IncludeBegin = "#include \""
const Tab = "	"
const Tab2 = Tab + Tab
const Tab3 = Tab + Tab + Tab
const Tab4 = Tab + Tab + Tab + Tab
const GameDirName = "node/game/"
const CentreDirName = "node/centre/"
const GateName = "node/gate/"
const ClientDirName = "client/"
const PlayerServiceIncludeName = "#include \"player_service.h\"\n"
const PlayerServiceRepliedIncludeName = "#include \"player_service_replied.h\"\n"
const MacroReturnIncludeName = "#include \"macros/return_define.h\""
const HandlerName = "Handler"
const RepliedHandlerName = "RepliedHandler"
const YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
const YourCodeEnd = "///<<< END WRITING YOUR CODE"
const YourCodePair = YourCodeBegin + "\n" + YourCodeEnd + "\n"
const CcGenericServices = "cc_generic_services"
const EmptyResponseName = "Empty"

var SourceDirs = [...]string{
	"pkg/common/",
	"pkg/pbc/",
	"node/centre/",
	"node/game/",
	"node/gate/",
	"client/"}

var ProjectSourceDirs []string
var ProjectSourceMd5Dirs []string

const PbcDirName = "pkg/pbc/src/proto/"

const PbcOutDir = ProjectDir + PbcDirName
const GrpcOutDir = ProjectDir + PbcDirName
const ClientGoOutDir = ProjectDir + ClientDirName
const GoPbGameDir = "pb/game/"
const ClientGoGamePbDir = ClientGoOutDir + GoPbGameDir
const MessageIdGo = "message_id.go"
const ClientMessageIdFile = ClientGoGamePbDir + MessageIdGo
const LoginDir = ProjectDir + "node/login/"
const LoginGoGameDir = LoginDir + GoPbGameDir
const LoginMessageIdGo = LoginGoGameDir + MessageIdGo
const DbGoDir = ProjectDir + "node/db/"
const DBGoGameDir = DbGoDir + GoPbGameDir
const ServiceInfoDirName = PbcOutDir + "service_info/"
const ServiceInfoExtName = "_service_info"
const ServiceCppFileName = ServiceInfoDirName + "service_info.cpp"
const ServiceHeadFileName = ServiceInfoDirName + "service_info.h"
const LuaServiceFileName = PbcLuaDirName + "lua_service.cpp"
const PbcLuaDirName = PbcOutDir + "lua/"

const CommonProtoDirIndex = 0
const ComponentProtoDirIndex = 1
const EventProtoDirIndex = 2
const LogicProtoDirIndex = 3
const ClientPlayerDirIndex = 4
const ServerPlayerDirIndex = 5

var ProtoDirNames = [...]string{
	"common/",
	"logic/component/",
	"logic/event/",
	"logic/server/",
	"logic/client_player/",
	"logic/server_player/",
	"logic/constants/",
}
var ProtoDirs []string
var ProtoMd5Dirs []string

const HandlerDirName = "src/handler/service/"
const RepliedHandlerDirName = "src/handler/service_replied/"
const ServiceIdsFileName = ProtoDir + "serviceid.txt"

const EventHandlerDirName = "src/handler/event/"
const EventHandlerFileName = "event_handler"
const EventHandlerFileNameHead = EventHandlerFileName + HeadEx
const EventHandlerFileNameCpp = EventHandlerFileName + CppEx

const GsMethodHandleDir = ProjectDir + GameDirName + HandlerDirName
const GsPlayerMethodHandleDir = ProjectDir + GameDirName + HandlerDirName + PlayerName + "/"
const GsEventHandleDir = ProjectDir + GameDirName + EventHandlerDirName
const GsMethodRepliedHandleDir = ProjectDir + GameDirName + RepliedHandlerDirName
const GsPlayerMethodRepliedHandleDir = ProjectDir + GameDirName + RepliedHandlerDirName + PlayerName + "/"
const GsPrefixName = "game"
const GsPlayerPrefixName = "game_player"

const CentreMethodHandleDir = ProjectDir + CentreDirName + HandlerDirName
const CentrePlayerMethodHandleDir = ProjectDir + CentreDirName + HandlerDirName + PlayerName + "/"
const CentreMethodRepliedHandleDir = ProjectDir + CentreDirName + RepliedHandlerDirName
const CentrePlayerMethodRepliedHandleDir = ProjectDir + CentreDirName + RepliedHandlerDirName + PlayerName + "/"
const CentreEventHandleDir = ProjectDir + CentreDirName + EventHandlerDirName
const CentrePrefixName = "centre"
const CentrePlayerPrefixName = "centre_player"

const ClientMethodHandleDir = ProjectDir + ClientDirName + HandlerDirName

const GateMethodHandleDir = ProjectDir + GateName + HandlerDirName
const GateMethodRepliedHandleDir = ProjectDir + GateName + RepliedHandlerDirName

const LoginPrefixName = "login"

const DeployPrefixName = "deploy"
const DatabasePrefixName = "db"

const LobbyPrefixName = "lobby"

const ClientLuaProjectRelative = "script/lua/service/"
const ClientLuaDir = ProjectDir + "bin/" + ClientLuaProjectRelative
const ClientLuaServiceFile = ProjectDir + "client/src/handler/service_lua.cpp"
const ClientServiceInstanceFile = ProjectDir + "client/src/handler/player_service.cpp"
const C2SMethodContainsName = "C2S"
const MysqlName = "mysql"
const PlayerServiceName = "player_service.cpp"
const PlayerRepliedServiceName = "player_service_replied.cpp"
const RegisterRepliedHandlerCppEx = "register" + CppRepliedHandlerEx
const RegisterHandlerCppEx = "register" + CppHandlerEx
const DbProtoName = "db_base.proto"
