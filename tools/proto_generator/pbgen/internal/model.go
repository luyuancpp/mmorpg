package internal

import (
	"github.com/iancoleman/strcase"
	"golang.org/x/text/cases"
	"golang.org/x/text/language"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/types/descriptorpb"
	"path"
	"path/filepath"
	"pbgen/config"
	_config "pbgen/internal/config"
	"strings"
	"sync"
)

type EmptyStruct struct{}

// MethodInfo 定义RPC方法信息
type MethodInfo struct {
	Id                     uint64
	Index                  uint64
	Fd                     *descriptorpb.FileDescriptorProto
	MethodDescriptorProto  *descriptorpb.MethodDescriptorProto
	ServiceDescriptorProto *descriptorpb.ServiceDescriptorProto
}

// RPCMethods 是RPCMethod的切片
type RPCMethods []*MethodInfo

// RPCServiceInfo 定义RPC服务信息
type RPCServiceInfo struct {
	MethodInfo             RPCMethods
	Fd                     *descriptorpb.FileDescriptorProto
	ServiceDescriptorProto *descriptorpb.ServiceDescriptorProto
	ServiceIndex           uint32
}

var FdSet = &descriptorpb.FileDescriptorSet{}

// RPCServiceInfoList 是用于排序的类型
type RPCServiceInfoList []*RPCServiceInfo

func (s RPCServiceInfoList) Len() int {
	return len(s)
}

func (s RPCServiceInfoList) Less(i, j int) bool {
	// 升序排序：ServiceIndex 小的在前
	return s[i].ServiceIndex < s[j].ServiceIndex
}

func (s RPCServiceInfoList) Swap(i, j int) {
	s[i], s[j] = s[j], s[i]
}

var GlobalRPCServiceList RPCServiceInfoList

var FileServiceMap sync.Map

// RpcIdMethodMap 存储RPC方法ID映射
var RpcIdMethodMap = map[uint64]*MethodInfo{}

// ServiceIdMap 存储服务ID映射
var ServiceIdMap = map[string]uint64{}

// MaxMessageId 存储最大消息ID
var MaxMessageId = uint64(0)

// MessageIdFileMaxId 存储消息ID文件最大ID
var MessageIdFileMaxId = uint64(0)

// FileMaxMessageId 存储文件最大消息ID
var FileMaxMessageId = uint64(0)

var (
	ActiveMsgDescCache map[protoreflect.FullName]protoreflect.MessageDescriptor
	FileDescCache      map[string]protoreflect.FileDescriptor

	DescriptorsLoaded bool
	LoadMutex         sync.Mutex
)

// 初始化缓存
func init() {
	ActiveMsgDescCache = make(map[protoreflect.FullName]protoreflect.MessageDescriptor)
	FileDescCache = make(map[string]protoreflect.FileDescriptor)
}

// KeyName 返回RPC方法的键名
func (info *MethodInfo) KeyName() string {
	return info.Service() + info.Method()
}

// IncludeName 返回包含头文件名
func (info *RPCServiceInfo) IncludeName() string {
	return "#include \"" + strings.Replace(info.Path(), _config.Global.Paths.ProtoDir, "", 1) + info.PbcHeadName() + "\"\n"
}

func (info *RPCServiceInfo) FileName() string {
	return *info.Fd.Name
}

func (info *RPCServiceInfo) Path() string {
	return strings.Replace(filepath.Dir(*info.Fd.Name), "\\", "/", -1) + "/"
}

func (info *RPCServiceInfo) GetServiceName() string {
	return info.ServiceDescriptorProto.GetName()
}

func (info *RPCServiceInfo) BasePathForCpp() string {
	return strcase.ToCamel(path.Base(info.Path()))
}

// PbcHeadName 返回Proto文件头文件名
func (info *RPCServiceInfo) PbcHeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoExt, _config.Global.FileExtensions.PbH, 1)
}

// HeadName 返回头文件名
func (info *RPCServiceInfo) HeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoExt, _config.Global.FileExtensions.Header, 1)
}

func (info *RPCServiceInfo) ServiceInfoIncludeName() string {
	return config.IncludeBegin + config.GeneratedRpcName + config.ServiceInfoName + info.FileBaseNameNoEx() + config.ServiceInfoExtension + _config.Global.FileExtensions.Header + "\"\n"
}

// FileBaseNameNoEx 返回文件基本名
func (info *RPCServiceInfo) FileBaseNameNoEx() string {
	return strings.Replace(info.FileBaseName(), config.ProtoExt, "", 1)
}

// FileBaseNameNoEx 返回文件基本名
func (info *RPCServiceInfo) FileBaseName() string {
	return filepath.Base(*info.Fd.Name)
}

func (info *RPCServiceInfo) FileBaseNameCamel() string {
	// 1. 去掉扩展名
	base := strings.Replace(info.FileBaseName(), config.ProtoExt, "", 1)
	// 2. 把下划线替换成空格，方便 Title() 把每个单词首字母大写
	base = strings.ReplaceAll(base, "_", " ")
	// 3. 首字母大写处理
	base = cases.Title(language.English).String(base)
	// 4. 去掉空格，连接成 CamelCase
	base = strings.ReplaceAll(base, " ", "")
	return base
}

func (info *RPCServiceInfo) CcGenericServices() bool {
	if info.Fd.Options == nil {
		return false
	}
	if info.Fd.Options.CcGenericServices == nil {
		return false
	}
	return *info.Fd.Options.CcGenericServices
}

func (info *RPCServiceInfo) ProtoPathWithFileBaseName() string {
	return (info.Path()) + info.FileBaseNameNoEx()
}

func (info *RPCServiceInfo) LogicalPath() string {
	fullPath := info.ProtoPathWithFileBaseName()
	return strings.TrimPrefix(fullPath, "proto/")
}

func (info *RPCServiceInfo) GrpcIncludeHeadName() string {
	return config.IncludeBegin + strings.Replace(info.Path(), _config.Global.Paths.ProtoDir, _config.Global.FileExtensions.ProtoDirName, 1) + info.GrpcHeadName() + _config.Global.FileExtensions.GrpcPbH + "\"\n"
}

func (info *RPCServiceInfo) GrpcHeadName() string {
	return strings.Replace(info.FileBaseNameNoEx(), config.ProtoExt, _config.Global.FileExtensions.GrpcPbH, 1)
}

func (info *RPCServiceInfo) GetServiceFullNameWithNoColon() string {
	if len(info.Package()) <= 0 {
		return info.Service()
	}
	return info.Package() + info.Service()
}

func (info *RPCServiceInfo) GeneratorGrpcFileName() string {
	return info.FileBaseNameNoEx() + _config.Global.FileExtensions.GrpcClient
}

func (info *RPCServiceInfo) Service() string {
	return info.ServiceDescriptorProto.GetName()
}

func (info *RPCServiceInfo) Package() string {
	if nil == info.Fd.Package {
		return ""
	}
	return *info.Fd.Package
}

func (info *RPCServiceInfo) ServiceInfoHeadInclude() string {
	return info.FileBaseNameNoEx() + config.ServiceInfoExtension + _config.Global.FileExtensions.Header
}

// FileBaseNameNoEx 返回文件基本名
func (info *MethodInfo) FileBaseNameNoEx() string {
	return strings.Replace(info.FileBaseName(), config.ProtoExt, "", 1)
}

func (info *MethodInfo) ServiceInfoHeadInclude() string {
	return info.FileBaseNameNoEx() + config.ServiceInfoExtension + _config.Global.FileExtensions.Header
}

func (info *MethodInfo) GeneratorGrpcFileName() string {
	return info.FileBaseNameNoEx() + _config.Global.FileExtensions.GrpcClient
}

func (info *MethodInfo) PbcHeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoExt, _config.Global.FileExtensions.PbH, 1)
}

func (info *MethodInfo) MethodName() string {
	return info.MethodDescriptorProto.GetName()
}

func (info *MethodInfo) FileBaseName() string {
	return filepath.Base(*info.Fd.Name)
}

func (info *MethodInfo) FileName() string {
	return *info.Fd.Name
}

func (info *MethodInfo) FileNameNoEx() string {
	return strings.Replace(info.FileName(), config.ProtoExt, "", 1)
}

func (info *MethodInfo) FileBaseNameCamel() string {
	// 1. 去掉扩展名
	base := strings.Replace(info.FileBaseName(), config.ProtoExt, "", 1)
	// 2. 把下划线替换成空格，方便 Title() 把每个单词首字母大写
	base = strings.ReplaceAll(base, "_", " ")
	// 3. 首字母大写处理
	base = cases.Title(language.English).String(base)
	// 4. 去掉空格，连接成 CamelCase
	base = strings.ReplaceAll(base, " ", "")
	return base
}

func (info *MethodInfo) Package() string {
	if nil == info.Fd.Package {
		return ""
	}
	return *info.Fd.Package
}

func (info *MethodInfo) GrpcHeadName() string {
	return strings.Replace(info.FileBaseName(), config.ProtoExt, _config.Global.FileExtensions.GrpcPbH, 1)
}

func (info *MethodInfo) GrpcIncludeHeadName() string {
	return config.IncludeBegin + strings.Replace(info.Path(), _config.Global.Paths.ProtoDir, _config.Global.FileExtensions.ProtoDirName, 1) + info.GrpcHeadName() + "\"\n"
}

// IncludeName 返回包含头文件名
func (info *MethodInfo) IncludeName() string {
	if info.CcGenericServices() {
		return config.IncludeBegin + strings.Replace(info.Path(), _config.Global.Paths.ProtoDir, _config.Global.FileExtensions.ProtoDirName, 1) + info.PbcHeadName() + "\"\n"
	}
	return info.GrpcIncludeHeadName()
}

func (info *MethodInfo) Path() string {
	return strings.Replace(filepath.Dir(*info.Fd.Name), "\\", "/", -1) + "/"
}

func (info *MethodInfo) GoPackagePrefix() string {
	return path.Base(info.Path()) + "."
}

func (info *MethodInfo) ServiceInfoIncludeName() string {
	return config.IncludeBegin + config.GeneratedRpcName + config.ServiceInfoName + info.FileBaseNameNoEx() + config.ServiceInfoExtension + _config.Global.FileExtensions.Header + "\"\n"
}

// CppHandlerIncludeName 返回Cpp处理器包含文件名
func (info *MethodInfo) CppHandlerIncludeName() string {
	return config.IncludeBegin + info.FileBaseNameNoEx() + config.HandlerHeaderExtension + config.IncludeEndLine
}

// CppRepliedHandlerIncludeName 返回Cpp已响应处理器包含文件名
func (info *MethodInfo) CppRepliedHandlerIncludeName() string {
	return config.IncludeBegin + info.FileBaseNameNoEx() + config.RepliedHandlerHeaderExtension + config.IncludeEndLine
}

// CppHandlerClassName 返回Cpp处理器类名
func (info *MethodInfo) CppHandlerClassName() string {
	return info.Service() + _config.Global.Naming.HandlerFile
}

// CppRepliedHandlerClassName 返回Cpp已响应处理器类名
func (info *MethodInfo) CppRepliedHandlerClassName() string {
	return info.Service() + _config.Global.Naming.RepliedHandlerFile
}

func (info *MethodInfo) CcGenericServices() bool {
	if info.Fd.Options == nil {
		return false
	}
	if info.Fd.Options.CcGenericServices == nil {
		return false
	}
	return *info.Fd.Options.CcGenericServices
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
	// 获取 OutputType
	inputType := info.MethodDescriptorProto.GetInputType()

	// .package.TypeName => 提取 TypeName
	lastDot := strings.LastIndex(inputType, ".")
	if lastDot >= 0 && lastDot < len(inputType)-1 {
		inputType = inputType[lastDot+1:]
	}

	var fileDir string
	activeMsgDesc, ok := ActiveMsgDescCache[protoreflect.FullName(inputType)]
	if ok {
		// 获取消息所在文件的完整路径
		filePath := activeMsgDesc.ParentFile().Path()
		// 提取文件所在目录
		fileDir = filepath.Base(filepath.Dir(filePath)) + "."
	}

	// 返回目录和类型名（根据需要调整返回内容）
	return fileDir + inputType
}

func (info *MethodInfo) GoResponse() string {
	// 获取 OutputType
	outputType := info.MethodDescriptorProto.GetOutputType()

	// .package.TypeName => 提取 TypeName
	lastDot := strings.LastIndex(outputType, ".")
	if lastDot >= 0 && lastDot < len(outputType)-1 {
		outputType = outputType[lastDot+1:]
	}

	var fileDir string
	activeMsgDesc, ok := ActiveMsgDescCache[protoreflect.FullName(outputType)]
	if ok {
		// 获取消息所在文件的完整路径
		filePath := activeMsgDesc.ParentFile().Path()
		// 提取文件所在目录
		fileDir = filepath.Base(filepath.Dir(filePath)) + "."
	}

	// 返回目录和类型名（根据需要调整返回内容）
	return fileDir + outputType
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
