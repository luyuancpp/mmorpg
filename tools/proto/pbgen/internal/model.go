package internal

import (
	"golang.org/x/text/cases"
	"golang.org/x/text/language"
	"google.golang.org/protobuf/types/descriptorpb"
	"path/filepath"
	"pbgen/config"
	"strings"
	"sync"
)

type EmptyStruct struct{}

// MethodInfo 定义RPC方法信息
type MethodInfo struct {
	Id                     uint64
	Index                  uint64
	FdSet                  *descriptorpb.FileDescriptorSet
	MethodDescriptorProto  *descriptorpb.MethodDescriptorProto
	ServiceDescriptorProto *descriptorpb.ServiceDescriptorProto
}

// RPCMethods 是RPCMethod的切片
type RPCMethods []*MethodInfo

// RPCServiceInfo 定义RPC服务信息
type RPCServiceInfo struct {
	MethodInfo             RPCMethods
	FdSet                  *descriptorpb.FileDescriptorSet
	ServiceDescriptorProto *descriptorpb.ServiceDescriptorProto
	FileServiceIndex       uint32
}

var FileServiceMap sync.Map

// RpcServiceMap 存储RPC服务映射
var RpcServiceMap sync.Map

// RpcIdMethodMap 存储RPC方法ID映射
var RpcIdMethodMap = map[uint64]*MethodInfo{}

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
func (info *MethodInfo) KeyName() string {
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

func (info *RPCServiceInfo) FileBaseNameCamel() string {
	// 1. 去掉扩展名
	base := strings.Replace(info.FileName(), config.ProtoEx, "", 1)
	// 2. 把下划线替换成空格，方便 Title() 把每个单词首字母大写
	base = strings.ReplaceAll(base, "_", " ")
	// 3. 首字母大写处理
	base = cases.Title(language.English).String(base)
	// 4. 去掉空格，连接成 CamelCase
	base = strings.ReplaceAll(base, " ", "")
	return base
}

func (info *RPCServiceInfo) ProtoPathWithFileBaseName() string {
	return (info.Path()) + info.FileBaseName()
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
	return info.FileBaseName() + config.GrpcExtension
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

func (info *RPCServiceInfo) ServiceInfoHeadInclude() string {
	return info.FileBaseName() + config.ServiceInfoExtension + config.HeaderExtension
}

// FileNameNoEx 返回文件基本名
func (info *MethodInfo) FileNameNoEx() string {
	return strings.Replace(info.FileBaseName(), config.ProtoEx, "", 1)
}

func (info *MethodInfo) ServiceInfoHeadInclude() string {
	return info.FileNameNoEx() + config.ServiceInfoExtension + config.HeaderExtension
}

func (info *MethodInfo) PbcHeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoEx, config.ProtoPbhEx, 1)
}

func (info *MethodInfo) MethodName() string {
	return info.MethodDescriptorProto.GetName()
}

func (info *MethodInfo) FileBaseName() string {
	return filepath.Base(*info.FdSet.GetFile()[0].Name)
}

func (info *MethodInfo) Package() string {
	if nil == info.FdSet.GetFile()[0].Package {
		return ""
	}
	return *info.FdSet.GetFile()[0].Package
}

func (info *MethodInfo) GrpcHeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoEx, config.GrpcPbhEx, 1)
}

func (info *MethodInfo) GrpcIncludeHeadName() string {
	return config.IncludeBegin + strings.Replace(info.Path(), config.ProtoDir, config.ProtoDirName, 1) + info.GrpcHeadName() + "\"\n"
}

// IncludeName 返回包含头文件名
func (info *MethodInfo) IncludeName() string {
	if info.CcGenericServices() {
		return config.IncludeBegin + strings.Replace(info.Path(), config.ProtoDir, config.ProtoDirName, 1) + info.PbcHeadName() + "\"\n"
	}
	return info.GrpcIncludeHeadName()
}

func (info *MethodInfo) Path() string {
	return strings.Replace(filepath.Dir(*info.FdSet.GetFile()[0].Name), "\\", "/", -1) + "/"
}

func (info *MethodInfo) ServiceInfoIncludeName() string {
	return config.IncludeBegin + info.FileNameNoEx() + config.ServiceInfoExtension + config.HeaderExtension + "\"\n"
}

// CppHandlerIncludeName 返回Cpp处理器包含文件名
func (info *MethodInfo) CppHandlerIncludeName() string {
	return config.IncludeBegin + info.FileNameNoEx() + config.HandlerHeaderExtension + config.IncludeEndLine
}

// CppRepliedHandlerIncludeName 返回Cpp已响应处理器包含文件名
func (info *MethodInfo) CppRepliedHandlerIncludeName() string {
	return config.IncludeBegin + info.FileNameNoEx() + config.RepliedHandlerHeaderExtension + config.IncludeEndLine
}

// CppHandlerClassName 返回Cpp处理器类名
func (info *MethodInfo) CppHandlerClassName() string {
	return info.Service() + config.HandlerFileName
}

// CppRepliedHandlerClassName 返回Cpp已响应处理器类名
func (info *MethodInfo) CppRepliedHandlerClassName() string {
	return info.Service() + config.RepliedHandlerFileName
}

func (info *MethodInfo) CcGenericServices() bool {
	files := info.FdSet.GetFile()
	if len(files) == 0 || files[0].Options == nil || files[0].Options.CcGenericServices == nil {
		return false
	}
	return *files[0].Options.CcGenericServices
}

func (info *MethodInfo) GetServiceFullNameWithColon() string {
	if len(info.Package()) <= 0 {
		return info.Service()
	}
	return info.Package() + "::" + info.Service()
}

func (info *MethodInfo) CppRequest() string {
	// 获取 InputType
	inputType := info.MethodDescriptorProto.GetInputType()

	// 将 InputType 中的点（.）替换为双冒号（::）
	updatedInputType := strings.Replace(inputType, ".", "::", -1)

	return updatedInputType
}

func (info *MethodInfo) Method() string {
	return info.MethodDescriptorProto.GetName()
}

func (info *MethodInfo) CppResponse() string {
	// 获取 OutputType
	outputType := info.MethodDescriptorProto.GetOutputType()

	// 将 OutputType 中的点（.）替换为双冒号（::）
	updatedOutputType := strings.Replace(outputType, ".", "::", -1)

	return updatedOutputType
}

func (info *MethodInfo) GoRequest() string {
	// 获取 inputType
	inputType := info.MethodDescriptorProto.GetInputType()

	// .package.TypeName => 提取 TypeName
	lastDot := strings.LastIndex(inputType, ".")
	if lastDot >= 0 && lastDot < len(inputType)-1 {
		return inputType[lastDot+1:]
	}
	return inputType
}

func (info *MethodInfo) GoResponse() string {
	// 获取 OutputType
	outputType := info.MethodDescriptorProto.GetOutputType()

	// .package.TypeName => 提取 TypeName
	lastDot := strings.LastIndex(outputType, ".")
	if lastDot >= 0 && lastDot < len(outputType)-1 {
		return outputType[lastDot+1:]
	}
	return outputType
}

func GetTypeName(fullTypeName string) string {
	// 查找最后一个'.'的位置
	lastDotIndex := strings.LastIndex(fullTypeName, ".")
	if lastDotIndex != -1 {
		// 返回'.'后面的部分，即类型名
		return fullTypeName[lastDotIndex+1:]
	}
	// 如果没有'.'，直接返回原始字符串
	return fullTypeName
}

func (info *MethodInfo) RequestName() string {
	return GetTypeName(info.MethodDescriptorProto.GetInputType())
}

func (info *MethodInfo) GetServiceFullNameWithNoColon() string {
	if len(info.Package()) <= 0 {
		return info.Service()
	}
	return info.Package() + info.Service()
}

func (info *MethodInfo) ClientStreaming() bool {
	if nil == info.MethodDescriptorProto.ClientStreaming {
		return false
	}
	return *info.MethodDescriptorProto.ClientStreaming
}

func (info *MethodInfo) ServerStreaming() bool {
	if nil == info.MethodDescriptorProto.ServerStreaming {
		return false
	}
	return *info.MethodDescriptorProto.ServerStreaming
}

func (info *MethodInfo) GetPackageNameWithColon() string {
	if len(info.Package()) <= 0 {
		return ""
	}
	return info.Package() + "::"
}

func (info *MethodInfo) Service() string {
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
