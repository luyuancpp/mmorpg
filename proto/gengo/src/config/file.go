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

var ProtoDirNames = [...]string{
	"common_proto/",
	"component_proto/",
	"event_proto/",
	"logic_proto/",
}
var ProtoDirs []string
var ProtoMd5Dirs []string

const ServiceDirName = "src/service"
const ServiceIdsFileName = ProtoDir + "serviceid.txt"
