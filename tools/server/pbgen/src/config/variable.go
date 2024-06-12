package config

const ProjectDir = "../../../../"
const ProtoDir = "../../../../proto/"
const Md5Ex = ".md5"
const Md5Dir = "..//bin/md5/"
const ProtoEx = ".proto"
const ProtoPbcEx = ".pb.cc"
const ProtoPbhEx = ".pb.h"
const GrpcPbcEx = ".grpc.pb.cc"
const GrpcEx = "grpc"
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
const GoogleMethodController = "(::google::protobuf::RpcController* controller,"
const PlayerMethodController = "(entt::entity player,"
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
const PlayerServiceIncludeName = "#include \"player_service.h\""
const PlayerServiceRepliedIncludeName = "#include \"player_service_replied.h\""
const HandlerName = "Handler"
const RepliedHandlerName = "RepliedHandler"
const YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
const YourCodeEnd = "///<<< END WRITING YOUR CODE"
const YourCodePair = YourCodeBegin + "\n" + YourCodeEnd + "\n"
const CcGenericServices = "cc_generic_services"

var SourceNames = [...]string{
	"common/",
	"center/",
	"game/",
	"gate/",
	"client"}

var ProjectSourceDirs []string
var ProjectSourceMd5Dirs []string

const PbcDirName = "pkg/pbc/src/"

const PbcOutDir = ProjectDir + PbcDirName
const GrpcOutDir = ProjectDir + PbcDirName
const ServiceDirName = PbcOutDir + "service/"
const ServiceCppFileName = ServiceDirName + "service.cpp"
const ServiceHeadFileName = ServiceDirName + "service.h"
const LuaServiceFileName = PbcLuaDirName + "lua_service.cpp"
const PbcLuaDirName = PbcOutDir + "lua/"

const CommonProtoDirIndex = 0
const ComponentProtoDirIndex = 1
const EventProtoDirIndex = 2
const LogicProtoDirIndex = 3
const ClientPlayerDirIndex = 4
const ServerPlayerDirIndex = 5

var ProtoDirNames = [...]string{
	"common_proto/",
	"component_proto/",
	"event_proto/",
	"logic_proto/",
	"client_player_proto/",
	"server_player_proto/",
	"tip_code_proto/",
	"constants_proto/",
}
var ProtoDirs []string
var ProtoMd5Dirs []string

const HandlerDirName = "src/handler/"
const RepliedHandlerDirName = "src/replied_handler/"
const ServiceIdsFileName = ProtoDir + "serviceid.txt"

const EventHandlerDirName = "src/event_handler/"
const EventHandlerFileName = "event_handler"
const EventHandlerFileNameHead = EventHandlerFileName + HeadEx
const EventHandlerFileNameCpp = EventHandlerFileName + CppEx

const GsMethodHandleDir = ProjectDir + GameDirName + HandlerDirName
const GsEventHandleDir = ProjectDir + GameDirName + EventHandlerDirName
const GsMethodRepliedHandleDir = ProjectDir + GameDirName + RepliedHandlerDirName
const GsPrefixName = "game"

const CentreMethodHandleDir = ProjectDir + CentreDirName + HandlerDirName
const CentreMethodRepliedHandleDir = ProjectDir + CentreDirName + RepliedHandlerDirName
const CentreEventHandleDir = ProjectDir + CentreDirName + EventHandlerDirName
const CentrePrefixName = "centre"

const ClientMethodHandleDir = ProjectDir + ClientDirName + HandlerDirName

const GateMethodHandleDir = ProjectDir + GateName + HandlerDirName
const GateMethodRepliedHandleDir = ProjectDir + GateName + RepliedHandlerDirName
const GatePrefixName = "gate"

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
