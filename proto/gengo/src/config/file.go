package config

const ProjectDir = "../../../"
const ProtoDir = "../../"
const Md5Ex = ".md5"
const Md5Dir = ProtoDir + "gengo/bin/md5/"
const ProtoEx = ".proto"
const ProtoPbcEx = ".pb.cc"
const ProtoPbhEx = ".pb.h"

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

const PbcOutDir = ProjectDir + "common/src/pb/pbc/"
const ServiceImplFileName = PbcOutDir + "service_impl.cpp"

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
