package gen

import (
	"google.golang.org/protobuf/types/descriptorpb"
	"path/filepath"
	"pbgen/config"
	"strings"
	"sync"
)

type EmptyStruct struct{}

// RPCMethod 定义RPC方法信息
type RPCMethod struct {
	Id                     uint64
	Index                  uint64
	FdSet                  *descriptorpb.FileDescriptorSet
	FileServiceIndex       uint32
	MethodDescriptorProto  *descriptorpb.MethodDescriptorProto
	ServiceDescriptorProto *descriptorpb.ServiceDescriptorProto
}

// RPCMethods 是RPCMethod的切片
type RPCMethods []*RPCMethod

// RPCServiceInfo 定义RPC服务信息
type RPCServiceInfo struct {
	MethodInfo             RPCMethods
	FdSet                  *descriptorpb.FileDescriptorSet
	ServiceDescriptorProto *descriptorpb.ServiceDescriptorProto
	FileServiceIndex       uint32
}

var FileServiceMap sync.Map

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
	return info.Service() + info.Method()
}

// IncludeName 返回包含头文件名
func (info *RPCServiceInfo) IncludeName() string {
	return "#include \"" + strings.Replace(info.Path(), config.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

func (info *RPCServiceInfo) FileName() string {
	return filepath.Base(*info.FdSet.GetFile()[0].Name)
}

func (info *RPCServiceInfo) Path() string {
	return strings.Replace(filepath.Dir(*info.FdSet.GetFile()[0].Name), "\\", "/", -1) + "/"
}

// PbcHeadName 返回Proto文件头文件名
func (info *RPCServiceInfo) PbcHeadName() string {
	return strings.Replace(info.FileName(), config.ProtoEx, config.ProtoPbhEx, 1)
}

// HeadName 返回头文件名
func (info *RPCServiceInfo) HeadName() string {
	return strings.Replace(info.FileName(), config.ProtoEx, config.HeaderExtension, 1)
}

// FileBaseName 返回文件基本名
func (info *RPCServiceInfo) FileBaseName() string {
	return strings.Replace(info.FileName(), config.ProtoEx, "", 1)
}

func (info *RPCServiceInfo) GrpcIncludeHeadName() string {
	return config.IncludeBegin + strings.Replace(info.Path(), config.ProtoDir, config.ProtoDirName, 1) + info.GrpcHeadName() + config.GrpcPbhEx + "\"\n"
}

func (info *RPCServiceInfo) GrpcHeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoEx, config.GrpcPbhEx, 1)
}

func (info *RPCServiceInfo) GetServiceFullNameWithNoColon() string {
	if len(info.Package()) <= 0 {
		return info.Service()
	}
	return info.Package() + info.Service()
}

func (info *RPCServiceInfo) GeneratorGrpcFileName() string {
	return info.FileBaseName()
}

func (info *RPCServiceInfo) Service() string {
	return info.ServiceDescriptorProto.GetName()
}

func (info *RPCServiceInfo) Package() string {
	if nil == info.FdSet.GetFile()[0].Package {
		return ""
	}
	return *info.FdSet.GetFile()[0].Package
}

// FileNameNoEx 返回文件基本名
func (info *RPCMethod) FileNameNoEx() string {
	return strings.Replace(info.FileBaseName(), config.ProtoEx, "", 1)
}

// PbcHeadName 返回Proto文件头文件名
func (info *RPCMethod) PbcHeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoEx, config.ProtoPbhEx, 1)
}

func (info *RPCMethod) FileBaseName() string {
	return filepath.Base(*info.FdSet.GetFile()[0].Name)
}

func (info *RPCMethod) Package() string {
	if nil == info.FdSet.GetFile()[0].Package {
		return ""
	}
	return *info.FdSet.GetFile()[0].Package
}

// IncludeName 返回包含头文件名
func (info *RPCMethod) IncludeName() string {
	return config.IncludeBegin + strings.Replace(info.Path(), config.ProtoDir, config.ProtoDirName, 1) + info.PbcHeadName() + "\"\n"
}

func (info *RPCMethod) Path() string {
	return strings.Replace(filepath.Dir(*info.FdSet.GetFile()[0].Name), "\\", "/", -1) + "/"
}

func (info *RPCMethod) ServiceInfoIncludeName() string {
	return config.IncludeBegin + info.FileNameNoEx() + config.ServiceInfoExtension + config.HeaderExtension + "\"\n"
}

// CppHandlerIncludeName 返回Cpp处理器包含文件名
func (info *RPCMethod) CppHandlerIncludeName() string {
	return config.IncludeBegin + info.FileNameNoEx() + config.HandlerHeaderExtension + config.IncludeEndLine
}

// CppRepliedHandlerIncludeName 返回Cpp已响应处理器包含文件名
func (info *RPCMethod) CppRepliedHandlerIncludeName() string {
	return config.IncludeBegin + info.FileNameNoEx() + config.RepliedHandlerHeaderExtension + config.IncludeEndLine
}

// CppHandlerClassName 返回Cpp处理器类名
func (info *RPCMethod) CppHandlerClassName() string {
	return info.Service() + config.HandlerFileName
}

// CppRepliedHandlerClassName 返回Cpp已响应处理器类名
func (info *RPCMethod) CppRepliedHandlerClassName() string {
	return info.Service() + config.RepliedHandlerFileName
}

// IsPlayerService 检查是否为玩家服务
func (info *RPCMethod) IsPlayerService() bool {
	return strings.Contains(info.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) ||
		strings.Contains(info.Path(), config.ProtoDirectoryNames[config.ServerPlayerDirIndex])
}

func (info *RPCMethod) CcGenericServices() bool {
	files := info.FdSet.GetFile()
	if len(files) == 0 || files[0].Options == nil || files[0].Options.CcGenericServices == nil {
		return false
	}
	return *files[0].Options.CcGenericServices
}

func (info *RPCMethod) GetServiceFullNameWithColon() string {
	if len(info.Package()) <= 0 {
		return info.Service()
	}
	return info.Package() + "::" + info.Service()
}

func (info *RPCMethod) CppRequest() string {
	// 获取 InputType
	inputType := info.MethodDescriptorProto.GetInputType()

	// 将 InputType 中的点（.）替换为双冒号（::）
	updatedInputType := strings.Replace(inputType, ".", "::", -1)

	return updatedInputType
}

func (info *RPCMethod) Method() string {
	return info.MethodDescriptorProto.GetName()
}

func (info *RPCMethod) CppResponse() string {
	// 获取 OutputType
	outputType := info.MethodDescriptorProto.GetOutputType()

	// 将 OutputType 中的点（.）替换为双冒号（::）
	updatedOutputType := strings.Replace(outputType, ".", "::", -1)

	return updatedOutputType
}

func (info *RPCMethod) GetServiceFullNameWithNoColon() string {
	if len(info.Package()) <= 0 {
		return info.Service()
	}
	return info.Package() + info.Service()
}

func (info *RPCMethod) GetPackageNameWithColon() string {
	if len(info.Package()) <= 0 {
		return ""
	}
	return info.Package() + "::"
}

func (info *RPCMethod) Service() string {
	return info.ServiceDescriptorProto.GetName()
}

// Len 返回RPCMethods的长度
func (s RPCMethods) Len() int {
	return len(s)
}

// Less 比较两个RPCMethods的索引
func (s RPCMethods) Less(i, j int) bool {
	if s[i].Service() < s[j].Service() {
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
