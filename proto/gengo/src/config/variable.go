package config

const ProjectDir = "../../../"
const ProtoDir = "../../"
const Md5Ex = ".md5"
const Md5Dir = ProtoDir + "gengo/bin/md5/"
const ProtoEx = ".proto"
const ProtoPbcEx = ".pb.cc"
const ProtoPbhEx = ".pb.h"
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
const ProtoPbhIncludeEndLine = ProtoPbhEx + IncludeEndLine
const Tab = "	"
const Tab2 = Tab + Tab
const Tab3 = Tab + Tab + Tab
const Tab4 = Tab + Tab + Tab + Tab
const GameDirName = "game_server/"
const ControllerDirName = "controller_server/"
const GateName = "gate_server/"
const LoginDirName = "login_server/"
const DataBaseDirName = "database_server/"
const ClientDirName = "client/"
const PlayerServiceIncludeName = "#include \"player_service.h\""
const PlayerServiceRepliedIncludeName = "#include \"player_service_replied.h\""
const HandlerName = "Handler"
const RepliedHandlerName = "RepliedHandler"
const YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
const YourCodeEnd = "///<<< END WRITING YOUR CODE"
const YourCodePair = YourCodeBegin + "\n" + YourCodeEnd + "\n"

var SourceNames = [...]string{
	"common/",
	"controller_server/",
	"game_server/",
	"gate_server/",
	"login_server/",
	"lobby_server/",
	"database_server/",
	"deploy_server/",
	"client"}

var ProjectSourceDirs []string
var ProjectSourceMd5Dirs []string

const PbcDirName = "common/src/pb/pbc/"

const PbcOutDir = ProjectDir + PbcDirName
const ServiceCppFileName = PbcOutDir + "service.cpp"
const ServiceHeadFileName = PbcOutDir + "service.h"
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

const ControllerMethodHandleDir = ProjectDir + ControllerDirName + HandlerDirName
const ControllerMethodRepliedHandleDir = ProjectDir + ControllerDirName + RepliedHandlerDirName
const ControllerEventHandleDir = ProjectDir + ControllerDirName + EventHandlerDirName
const ControllerPrefixName = "controller"

const ClientMethodHandleDir = ProjectDir + ClientDirName + HandlerDirName

const GateMethodHandleDir = ProjectDir + GateName + HandlerDirName
const GateMethodRepliedHandleDir = ProjectDir + GateName + RepliedHandlerDirName
const GatePrefixName = "gate"

const LoginMethodHandleDir = ProjectDir + LoginDirName + HandlerDirName
const LoginMethodRepliedHandleDir = ProjectDir + LoginDirName + RepliedHandlerDirName
const LoginPrefixName = "login"

const DataBaseMethodHandleDir = ProjectDir + DataBaseDirName + HandlerDirName
const DataBaseMethodRepliedHandleDir = ProjectDir + DataBaseDirName + RepliedHandlerDirName
const DataBasePrefixName = "database"

const ClientLuaProjectRelative = "script/lua/service/"
const ClientLuaDir = ProjectDir + "bin/" + ClientLuaProjectRelative
const ClientLuaServiceFile = ProjectDir + "client/src/handler/service_lua.cpp"
const ClientServiceInstanceFile = ProjectDir + "client/src/handler/player_service.cpp"
const C2SMethodContainsName = "C2S"
const MysqlName = "mysql"
const PlayerServiceName = "player_service.cpp"
const PlayerRepliedServiceName = "player_service_replied.cpp"
const GoogleEmptyProtoName = "google::protobuf::Empty"
const RegisterRepliedHandlerCppEx = "register" + CppRepliedHandlerEx
const RegisterHandlerCppEx = "register" + CppHandlerEx
