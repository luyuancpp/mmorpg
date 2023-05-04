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
const RpcIdName = "MsgId"
const PlayerName = "player"
const ClientPlayerName = "client_player"
const GoogleMethodController = "(::google::protobuf::RpcController* controller,"
const PlayerMethodController = "(entt::entity player,"
const IncludeEndLine = "\"\n"
const IncludeBegin = "#include \""
const ProtoPbhIncludeEndLine = ProtoPbhEx + IncludeEndLine
const Tab = "	"
const Tab2 = Tab + Tab
const Tab3 = Tab + Tab + Tab
const GsName = "game_server/"
const ControllerName = "controller_server/"
const PlayerServiceIncludeName = "#include \"player_service.h\""
const HandlerName = "Handler"
const YourCodeBegin = "///<<< BEGIN WRITING YOUR CODE"
const YourCodeEnd = "///<<< END WRITING YOUR CODE"

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
const ServiceFileName = PbcOutDir + "service.cpp"

const CommonProtoDirIndex = 0
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
const ServiceIdsFileName = ProtoDir + "serviceid.txt"

const GsMethodHandleDir = ProjectDir + GsName + HandlerDirName
const ControllerMethodHandleDir = ProjectDir + ControllerName + HandlerDirName

const ClientLuaProjectRelative = "script/lua/service/"
const ClientLuaDir = ProjectDir + "bin/" + ClientLuaProjectRelative
const ClientLuaServiceFile = ProjectDir + "client/src/service/logic_proto/service_lua.cpp"
