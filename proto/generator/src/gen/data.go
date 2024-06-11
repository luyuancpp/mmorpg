package gen

import (
	"gengo/config"
	"strings"
	"sync"
)

type EmptyStruct struct{}

type RpcMethodInfo struct {
	Service  string
	Method   string
	Request  string
	Response string
	Id       uint64
	Index    uint64
	FileName string
	Path     string
}

type RpcMethodInfos []*RpcMethodInfo

type RpcServiceInfo struct {
	FileName   string
	Path       string
	MethodInfo RpcMethodInfos
}

var rpcLineReplacer = strings.NewReplacer("(", "", ")", "", ";", "", "\n", "")

var RpcServiceMap sync.Map
var GrpcServiceFileMap sync.Map
var RpcIdMethodMap = map[uint64]*RpcMethodInfo{}
var ServiceIdMap = map[string]uint64{}
var ServiceMethodMap = map[string]RpcMethodInfos{}
var MaxMessageId = uint64(0)
var MessageIdFileMaxId = uint64(0)
var FileMaxMessageId = uint64(0)

func (info *RpcMethodInfo) KeyName() (idName string) {
	return info.Service + info.Method
}

func (info *RpcServiceInfo) IncludeName() (includeName string) {
	return "#include \"" + strings.Replace(info.Path, config.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

func (info *RpcServiceInfo) PbcHeadName() (pbcHeadName string) {
	return strings.Replace(info.FileName, config.ProtoEx, config.ProtoPbhEx, 1)
}

func (info *RpcServiceInfo) HeadName() (headName string) {
	return strings.Replace(info.FileName, config.ProtoEx, config.HeadEx, 1)
}

func (info *RpcServiceInfo) FileBaseName() (fileBaseName string) {
	return strings.Replace(info.FileName, config.ProtoEx, "", 1)
}

func (info *RpcServiceInfo) IsPlayerService() (isPlayerService bool) {
	return strings.Contains(info.Path, config.ProtoDirNames[config.ClientPlayerDirIndex]) ||
		strings.Contains(info.Path, config.ProtoDirNames[config.ServerPlayerDirIndex])
}

func (info *RpcMethodInfo) FileBaseName() (fileBaseName string) {
	return strings.Replace(info.FileName, config.ProtoEx, "", 1)
}

func (info *RpcMethodInfo) PbcHeadName() (pbcHeadName string) {
	return strings.Replace(info.FileName, config.ProtoEx, config.ProtoPbhEx, 1)
}

func (info *RpcMethodInfo) IncludeName() (includeName string) {
	return config.IncludeBegin + strings.Replace(info.Path, config.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

func (info *RpcMethodInfo) CppHandlerIncludeName() (includeName string) {
	return config.IncludeBegin + info.FileBaseName() + config.HeadHandlerEx + config.IncludeEndLine
}

func (info *RpcMethodInfo) CppRepliedHandlerIncludeName() (includeName string) {
	return config.IncludeBegin + info.FileBaseName() + config.HeadRepliedHandlerEx + config.IncludeEndLine
}

func (info *RpcMethodInfo) CppHandlerClassName() (includeName string) {
	return info.Service + config.HandlerName
}

func (info *RpcMethodInfo) CppRepliedHandlerClassName() (includeName string) {
	return info.Service + config.RepliedHandlerName
}

func (info *RpcMethodInfo) IsPlayerService() (isPlayerService bool) {
	return strings.Contains(info.Path, config.ProtoDirNames[config.ClientPlayerDirIndex]) ||
		strings.Contains(info.Path, config.ProtoDirNames[config.ServerPlayerDirIndex])
}

func (s RpcMethodInfos) Len() int {
	return len(s)
}

func (s RpcMethodInfos) Less(i, j int) bool {
	if s[i].Service < s[j].Service {
		return true
	}
	return s[i].Index < s[j].Index
}

func (s RpcMethodInfos) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}

func MessageIdLen() uint64 {
	return FileMaxMessageId + 1
}
