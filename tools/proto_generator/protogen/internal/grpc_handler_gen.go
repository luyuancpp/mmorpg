package internal

import (
	_ "embed"
	"strings"
	"text/template"

	_config "protogen/internal/config"
	"protogen/internal/utils"
)

// 模板与工具同批编译，独立模板文件是骨架的事实源，运行时不依赖工作目录定位模板。
//
//go:embed template/grpc_service_handler.h.tmpl
var grpcServiceHandlerHeadTemplate string

//go:embed template/grpc_service_handler.cpp.tmpl
var grpcServiceHandlerCppTemplate string

// GrpcHandlerMethod holds data for a gRPC handler method.
type GrpcHandlerMethod struct {
	HandlerName    string
	CppRequest     string
	CppResponse    string
	Code           string
	HasCode        bool
	IsVoidResponse bool
	// PreDispatchCode 是 gRPC 包装函数里、投递到事件循环之前的守护段(含首尾标记)。
	// 用于必须在 gRPC 线程上完成的准入检查(如 Agones 建场景许可),可返回非 OK。
	// 包装函数原本没有守护段,守护段外的手写代码会被重生成吞掉(commit 6c4021ae5 事故)。
	PreDispatchCode  string
	InLoopCode       string
	PostDispatchCode string
}

// GrpcHandlerHeadData holds data for generating a gRPC service handler header.
type GrpcHandlerHeadData struct {
	GrpcInclude  string
	Service      string
	Package      string
	Methods      []*MethodInfo
	IncludesCode string
	PublicCode   string
	PrivateCode  string
}

// GrpcHandlerCppData holds data for generating a gRPC service handler CPP file.
type GrpcHandlerCppData struct {
	IncludeName  string
	Service      string
	Package      string
	FirstCode    string
	YourCodePair string
	Methods      []GrpcHandlerMethod
}

// GetGrpcServiceHandlerHeadStr generates the gRPC service handler header file.
// The generated class extends the grpc-generated ::Service base and dispatches
// each RPC to a static Handle* method. Handle* methods contain BEGIN WRITING YOUR CODE blocks.
func GetGrpcServiceHandlerHeadStr(methods RPCMethods) (string, error) {
	return renderGrpcServiceHandlerHead(methods, nil)
}

// 头文件的 include、公开声明与私有状态也属于自定义代码，必须在重生成时保存。
func GetGrpcServiceHandlerHeadFromFileStr(dst string, methods RPCMethods) (string, error) {
	sections, err := readNamedGrpcCodeSections(dst)
	if err != nil {
		return "", err
	}
	return renderGrpcServiceHandlerHead(methods, sections)
}

func renderGrpcServiceHandlerHead(methods RPCMethods, sections map[string]string) (string, error) {
	if err := validateNamedGrpcCodeSections(sections, []string{
		"grpc.header.includes", "grpc.header.public", "grpc.header.private",
	}); err != nil {
		return "", err
	}

	if len(methods) == 0 {
		return "", nil
	}

	first := methods[0]

	data := GrpcHandlerHeadData{
		GrpcInclude:  first.GrpcIncludeHeadName(),
		Service:      first.Service(),
		Package:      cppPackagePrefix(first),
		Methods:      methods,
		IncludesCode: namedGrpcCodeSection(sections, "grpc.header.includes", ""),
		PublicCode:   namedGrpcCodeSection(sections, "grpc.header.public", "    "),
		PrivateCode:  namedGrpcCodeSection(sections, "grpc.header.private", "    "),
	}

	funcs := template.FuncMap{
		"handleResponseParam": func(m *MethodInfo) string {
			if isVoidResponse(m) {
				return ""
			}
			return ", " + m.CppResponse() + "* response"
		},
	}

	return utils.GlobalEngine.MustExecute("grpcServiceHandlerHead", grpcServiceHandlerHeadTemplate, data, funcs)
}

// GetGrpcServiceHandlerCppStr generates the gRPC service handler CPP file content.
func GetGrpcServiceHandlerCppStr(dst string, methods RPCMethods, className string, includeName string) string {

	if len(methods) == 0 {
		return ""
	}

	emptyStr := ""
	namedSections, err := readNamedGrpcCodeSections(dst)
	if err == nil {
		var allowedNames []string
		for _, method := range methods {
			allowedNames = append(allowedNames, "grpc."+method.Method()+".in_loop", "grpc."+method.Method()+".post_dispatch")
		}
		err = validateNamedGrpcCodeSections(namedSections, allowedNames)
	}
	if err != nil {
		// 拒绝生成而不是覆盖损坏的守护段；既有字符串接口的调用方只在返回后才写文件。
		panic(err)
	}
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateGrpcHandlerNameWrapper, emptyStr)
	// 第二遍按包装函数签名取"投递前"守护段。解析器只在命中方法签名之后才收集守护段,
	// Handle* 的签名不含 "(grpc::ServerContext",所以两遍互不串段;
	// 旧文件的包装函数没有守护段时由 fillMissingMethods 补默认空段。
	preDispatchMap, _, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateGrpcWrapperNameWrapper, emptyStr)

	first := methods[0]

	var methodList []GrpcHandlerMethod
	for _, methodInfo := range methods {
		handlerName := methodInfo.Method()
		lookupKey := GenerateGrpcHandlerNameWrapper(methodInfo, emptyStr)
		code, exists := yourCodesMap[lookupKey]
		preDispatchCode, ok := preDispatchMap[GenerateGrpcWrapperNameWrapper(methodInfo, emptyStr)]
		if !ok {
			preDispatchCode = _config.Global.Naming.YourCodePair
		}
		methodList = append(methodList, GrpcHandlerMethod{
			HandlerName:      handlerName,
			CppRequest:       methodInfo.CppRequest(),
			CppResponse:      methodInfo.CppResponse(),
			Code:             strings.TrimRight(code, "\n"),
			HasCode:          exists,
			IsVoidResponse:   isVoidResponse(methodInfo),
			PreDispatchCode:  strings.TrimRight(preDispatchCode, "\n"),
			InLoopCode:       namedGrpcCodeSection(namedSections, "grpc."+handlerName+".in_loop", "        "),
			PostDispatchCode: namedGrpcCodeSection(namedSections, "grpc."+handlerName+".post_dispatch", "    "),
		})
	}

	data := GrpcHandlerCppData{
		IncludeName:  grpcHandlerIncludeName(first),
		Service:      first.Service(),
		Package:      cppPackagePrefix(first),
		FirstCode:    strings.TrimRight(firstCode, "\n"),
		YourCodePair: _config.Global.Naming.YourCodePair,
		Methods:      methodList,
	}

	return utils.ExecuteTemplate("grpcServiceHandlerCpp", grpcServiceHandlerCppTemplate, data)
}

// GenerateGrpcHandlerNameWrapper generates the method lookup key for code parsing.
func GenerateGrpcHandlerNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + "Impl::Handle" + info.Method() + "("
}

// GenerateGrpcWrapperNameWrapper 生成 gRPC 包装函数(投递前守护段)的查找键。
// 带上 "(grpc::ServerContext" 是为了不与 Handle* 键、以及同名前缀方法(如 Create / CreateScene)互相命中。
func GenerateGrpcWrapperNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + "Impl::" + info.Method() + "(grpc::ServerContext"
}

// cppPackagePrefix returns the C++ namespace prefix with trailing :: if non-empty.
func cppPackagePrefix(info *MethodInfo) string {
	pkg := info.CppPackage()
	if pkg == "" {
		return ""
	}
	return pkg + "::"
}

// isVoidResponse checks if the response type is Empty (void-like).
func isVoidResponse(m *MethodInfo) bool {
	resp := m.CppResponse()
	return resp == "::Empty" || resp == "Empty"
}

// grpcHandlerIncludeName returns the #include for the gRPC handler header.
func grpcHandlerIncludeName(info *MethodInfo) string {
	return "#include \"" + info.FileBaseNameNoEx() + _config.Global.FileExtensions.GrpcHandlerH + "\"\n" +
		"#include <future>"
}
