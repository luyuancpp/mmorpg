package config

var ProjectDir = "../../"

var Md5Dir = string("../md5/")
var PbcOutDir = "../../common/src/pb/pbc/"
var ServerDirs = [...]string{
	"common",
	"controller_server",
	"game_server",
	"gate_server",
	"login_server",
	"lobby_server",
	"database_server",
	"deploy_server",
	"client"}
var DstDir = "src/service"
var ProtoDirNames = [...]string{
	"common_proto",
	"component_proto",
	"event_proto",
	"logic_proto",
}
var ProtoDirs = [...]string{
	"../common_proto/",
	"../component_proto/",
	"../event_proto/",
	"../logic_proto/",
}
