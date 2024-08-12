package gen

import (
	"pbgen/config"
	"strings"
	"sync"
)

type EmptyStruct struct{}

// RPCMethod 定义RPC方法信息
type RPCMethod struct {
	Service           string
	Method            string
	Request           string
	Response          string
	Id                uint64
	Index             uint64
	FileName          string
	Path              string
	CcGenericServices bool
}

// RPCMethods 是RPCMethod的切片
type RPCMethods []*RPCMethod

// RPCServiceInfo 定义RPC服务信息
type RPCServiceInfo struct {
	FileName   string
	Path       string
	MethodInfo RPCMethods
}

// rpcLineReplacer 用于字符串替换
var rpcLineReplacer = strings.NewReplacer("(", "", ")", "", ";", "", "\n", "")

// RpcServiceMap 存储RPC服务映射
var RpcServiceMap sync.Map

// GrpcServiceFileMap 存储gRPC服务文件映射
var GrpcServiceFileMap sync.Map

// RpcIdMethodMap 存储RPC方法ID映射
var RpcIdMethodMap = map[uint64]*RPCMethod{}

// ServiceIdMap 存储服务ID映射
var ServiceIdMap = map[string]uint64{}

// ServiceMethodMap 存储服务方法映射
var ServiceMethodMap = map[string]RPCMethods{}

// MaxMessageId 存储最大消息ID
var MaxMessageId = uint64(0)

// MessageIdFileMaxId 存储消息ID文件最大ID
var MessageIdFileMaxId = uint64(0)

// FileMaxMessageId 存储文件最大消息ID
var FileMaxMessageId = uint64(0)

// KeyName 返回RPC方法的键名
func (info *RPCMethod) KeyName() string {
	return info.Service + info.Method
}

// IncludeName 返回包含头文件名
func (info *RPCServiceInfo) IncludeName() string {
	return "#include \"" + strings.Replace(info.Path, config.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

// PbcHeadName 返回Proto文件头文件名
func (info *RPCServiceInfo) PbcHeadName() string {
	return strings.Replace(info.FileName, config.ProtoEx, config.ProtoPbhEx, 1)
}

// HeadName 返回头文件名
func (info *RPCServiceInfo) HeadName() string {
	return strings.Replace(info.FileName, config.ProtoEx, config.HeadEx, 1)
}

// FileBaseName 返回文件基本名
func (info *RPCServiceInfo) FileBaseName() string {
	return strings.Replace(info.FileName, config.ProtoEx, "", 1)
}

// IsPlayerService 检查是否为玩家服务
func (info *RPCServiceInfo) IsPlayerService() bool {
	return strings.Contains(info.Path, config.ProtoDirNames[config.ClientPlayerDirIndex]) ||
		strings.Contains(info.Path, config.ProtoDirNames[config.ServerPlayerDirIndex])
}

// FileBaseName 返回文件基本名
func (info *RPCMethod) FileBaseName() string {
	return strings.Replace(info.FileName, config.ProtoEx, "", 1)
}

// PbcHeadName 返回Proto文件头文件名
func (info *RPCMethod) PbcHeadName() string {
	return strings.Replace(info.FileName, config.ProtoEx, config.ProtoPbhEx, 1)
}

// IncludeName 返回包含头文件名
func (info *RPCMethod) IncludeName() string {
	return config.IncludeBegin + strings.Replace(info.Path, config.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

func (info *RPCMethod) ServiceInfoIncludeName() string {
	return config.IncludeBegin + info.FileBaseName() + config.ServiceInfoExtName + config.HeadEx + "\"\n"
}

// CppHandlerIncludeName 返回Cpp处理器包含文件名
func (info *RPCMethod) CppHandlerIncludeName() string {
	return config.IncludeBegin + info.FileBaseName() + config.HeadHandlerEx + config.IncludeEndLine
}

// CppRepliedHandlerIncludeName 返回Cpp已响应处理器包含文件名
func (info *RPCMethod) CppRepliedHandlerIncludeName() string {
	return config.IncludeBegin + info.FileBaseName() + config.HeadRepliedHandlerEx + config.IncludeEndLine
}

// CppHandlerClassName 返回Cpp处理器类名
func (info *RPCMethod) CppHandlerClassName() string {
	return info.Service + config.HandlerName
}

// CppRepliedHandlerClassName 返回Cpp已响应处理器类名
func (info *RPCMethod) CppRepliedHandlerClassName() string {
	return info.Service + config.RepliedHandlerName
}

// IsPlayerService 检查是否为玩家服务
func (info *RPCMethod) IsPlayerService() bool {
	return strings.Contains(info.Path, config.ProtoDirNames[config.ClientPlayerDirIndex]) ||
		strings.Contains(info.Path, config.ProtoDirNames[config.ServerPlayerDirIndex])
}

// Len 返回RPCMethods的长度
func (s RPCMethods) Len() int {
	return len(s)
}

// Less 比较两个RPCMethods的索引
func (s RPCMethods) Less(i, j int) bool {
	if s[i].Service < s[j].Service {
		return true
	}
	return s[i].Index < s[j].Index
}

// Swap 交换RPCMethods的元素
func (s RPCMethods) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}

// MessageIdLen 返回消息ID的长度
func MessageIdLen() uint64 {
	return FileMaxMessageId + 1
}
