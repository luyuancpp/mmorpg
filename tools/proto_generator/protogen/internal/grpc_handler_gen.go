package internal

import (
	_ "embed"
	"fmt"
	"path/filepath"
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

// legacyGrpcPreDispatchSectionName 是旧模板包装函数里无名投递前段在报错清单里的名字。
const legacyGrpcPreDispatchSectionName = "无名投递前段"

// GrpcHandlerMethod holds data for a gRPC handler method.
type GrpcHandlerMethod struct {
	HandlerName    string
	CppRequest     string
	CppResponse    string
	Code           string
	HasCode        bool
	IsVoidResponse bool
	// GuardCode 是 <M>() 里、call->Proceed() 之前的命名守护段 grpc.<M>.guard(含首尾标记),在 loop 线程执行:
	// 同步拒绝,或发起异步操作后 return、在回调里 Proceed / Reject(docs/design/grpc-muduo-transport.md §15.8)。
	// 它取代旧模板的投递前 / in_loop / post_dispatch 三段;旧段非空时拒绝生成(checkLegacyGrpcSections)。
	GuardCode string
}

// GrpcHandlerHeadData holds data for generating a gRPC service handler header.
type GrpcHandlerHeadData struct {
	PbInclude    string
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
// The generated class implements muduo_grpc::Service: RegisterMethods routes each :path to <M>(),
// which runs the grpc.<M>.guard section and then the static Handle* method on the node EventLoop.
func GetGrpcServiceHandlerHeadStr(methods RPCMethods) (string, error) {
	return renderGrpcServiceHandlerHead(methods, nil)
}

// 头文件的 include、公开声明与私有状态也属于自定义代码，必须在重生成时保存。
// .h 与 .cpp 由通用管线分别生成、互不等待(handlers.go / handler.go):旧段检查必须在渲染头文件之前对同名 .cpp 做完,
// 出错时 .h 也不写,不会留下「新 .h 配旧 .cpp」(grpc-muduo-transport.md §15.8.3)。
func GetGrpcServiceHandlerHeadFromFileStr(dst string, methods RPCMethods) (string, error) {
	sections, err := readNamedGrpcCodeSections(dst)
	if err != nil {
		return "", err
	}
	if len(methods) > 0 {
		cppPath, err := grpcHandlerCppPathForHeader(dst)
		if err != nil {
			return "", err
		}
		if _, err := validateGrpcHandlerCppSections(cppPath, methods); err != nil {
			return "", err
		}
	}
	return renderGrpcServiceHandlerHead(methods, sections)
}

func renderGrpcServiceHandlerHead(methods RPCMethods, sections map[string]string) (string, error) {
	if err := validateNamedGrpcCodeSections(sections, []string{
		"grpc.header.includes", "grpc.header.public", "grpc.header.private",
	}, nil); err != nil {
		return "", err
	}

	if len(methods) == 0 {
		return "", nil
	}
	if err := rejectStreamingGrpcMethods(methods); err != nil {
		return "", err
	}

	first := methods[0]

	data := GrpcHandlerHeadData{
		PbInclude:    grpcHandlerPbInclude(first),
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
		"grpcServerMethodPath": grpcServerMethodPath,
	}

	return utils.GlobalEngine.MustExecute("grpcServiceHandlerHead", grpcServiceHandlerHeadTemplate, data, funcs)
}

// GetGrpcServiceHandlerCppStr generates the gRPC service handler CPP file content.
func GetGrpcServiceHandlerCppStr(dst string, methods RPCMethods, className string, includeName string) string {

	if len(methods) == 0 {
		return ""
	}

	// 拒绝生成而不是覆盖损坏或未迁移的守护段；既有字符串接口的调用方只在返回后才写文件。
	if err := rejectStreamingGrpcMethods(methods); err != nil {
		panic(err)
	}
	namedSections, err := validateGrpcHandlerCppSections(dst, methods)
	if err != nil {
		panic(err)
	}

	emptyStr := ""
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateGrpcHandlerNameWrapper, emptyStr)

	first := methods[0]

	var methodList []GrpcHandlerMethod
	for _, methodInfo := range methods {
		handlerName := methodInfo.Method()
		lookupKey := GenerateGrpcHandlerNameWrapper(methodInfo, emptyStr)
		code, exists := yourCodesMap[lookupKey]
		methodList = append(methodList, GrpcHandlerMethod{
			HandlerName:    handlerName,
			CppRequest:     methodInfo.CppRequest(),
			CppResponse:    methodInfo.CppResponse(),
			Code:           strings.TrimRight(code, "\n"),
			HasCode:        exists,
			IsVoidResponse: isVoidResponse(methodInfo),
			GuardCode:      namedGrpcCodeSection(namedSections, grpcGuardSectionName(methodInfo), "    "),
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
// 这个字面量在生成正文里只能出现在 Handle* 的定义行:RegisterMethods 里调用 Handle* 一律不带类名前缀。
func GenerateGrpcHandlerNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + "Impl::Handle" + info.Method() + "("
}

// legacyGrpcWrapperKey 是旧模板 grpc::Status 包装函数的查找键,只用于 checkLegacyGrpcSections 检测旧的无名投递前段。
// 带上 "(grpc::ServerContext" 是为了不与 Handle* 键、以及同名前缀方法(如 Create / CreateScene)互相命中。
func legacyGrpcWrapperKey(info *MethodInfo, _ string) string {
	return info.Service() + "Impl::" + info.Method() + "(grpc::ServerContext"
}

// grpcGuardSectionName 是 <M>() 里唯一的命名守护段。
func grpcGuardSectionName(info *MethodInfo) string {
	return "grpc." + info.Method() + ".guard"
}

// grpcLegacyNamedSectionNames 是旧模板的两个命名段;只在为空时接受,然后丢弃。
func grpcLegacyNamedSectionNames(info *MethodInfo) []string {
	return []string{"grpc." + info.Method() + ".in_loop", "grpc." + info.Method() + ".post_dispatch"}
}

// grpcServerMethodPath 返回 gRPC :path(线上契约):有 package 时为 "/pkg.Svc/Method",否则 "/Svc/Method"。
// 与 grpc_async_client.h.tmpl 的 k<Svc><Method>GrpcPath 同一规则;服务端按它精确路由(§15.5.4)。
func grpcServerMethodPath(info *MethodInfo) string {
	if pkg := info.Package(); pkg != "" {
		return "/" + pkg + "." + info.Service() + "/" + info.Method()
	}
	return "/" + info.Service() + "/" + info.Method()
}

// grpcHandlerPbInclude 返回服务 proto 的 .pb.h include(与 MethodInfo.IncludeName 的 cc_generic 分支同式)。
// 服务端不再 include .grpc.pb.h:骨架只用消息类型,不再继承官方生成的 ::Service(§15.1)。
func grpcHandlerPbInclude(info *MethodInfo) string {
	return _config.Global.Naming.IncludeBegin +
		strings.Replace(info.Path(), _config.Global.Paths.ProtoDir, _config.Global.DirectoryNames.ProtoDirName, 1) +
		info.PbcHeadName() + "\"\n"
}

// grpcHandlerCppPathForHeader 由头文件路径推出同名 .cpp:与 handler.go 写 .cpp 的路径同式(文件名转小写、换扩展名)。
// 推不出来就报错 —— 预检做不了就不生成,不能默默跳过。
func grpcHandlerCppPathForHeader(headerPath string) (string, error) {
	headerExt := _config.Global.FileExtensions.GrpcHandlerH
	cppExt := _config.Global.FileExtensions.GrpcHandlerCpp
	if headerExt == "" || cppExt == "" || !strings.HasSuffix(headerPath, headerExt) {
		return "", fmt.Errorf("%s：无法推出同名 gRPC handler cpp(grpc_handler_h=%q grpc_handler_cpp=%q)，拒绝在未预检旧守护段时生成头文件",
			headerPath, headerExt, cppExt)
	}
	dir, base := filepath.Split(strings.TrimSuffix(headerPath, headerExt))
	return dir + strings.ToLower(base) + cppExt, nil
}

// rejectStreamingGrpcMethods:服务端(muduo_grpc)只支持 unary;服务里有流式方法就拒绝生成,不产出半套骨架(fail-closed)。
func rejectStreamingGrpcMethods(methods RPCMethods) error {
	var streaming []string
	for _, method := range methods {
		if method.ClientStreaming() || method.ServerStreaming() {
			streaming = append(streaming, method.Service()+"."+method.Method())
		}
	}
	if len(streaming) == 0 {
		return nil
	}
	return fmt.Errorf("gRPC 服务端骨架只支持 unary 方法，拒绝生成流式方法：%s", strings.Join(streaming, ", "))
}

// validateGrpcHandlerCppSections 是 .h 预检与 .cpp 生成共用的 cpp 守护段检查,返回 cpp 的命名守护段:
// 先报全部非空旧段(checkLegacyGrpcSections),再拒绝损坏或未识别的命名段。
func validateGrpcHandlerCppSections(cppPath string, methods RPCMethods) (map[string]string, error) {
	if err := checkLegacyGrpcSections(cppPath, methods); err != nil {
		return nil, err
	}
	sections, err := readNamedGrpcCodeSections(cppPath)
	if err != nil {
		return nil, err
	}
	var guardNames, legacyNames []string
	for _, method := range methods {
		guardNames = append(guardNames, grpcGuardSectionName(method))
		legacyNames = append(legacyNames, grpcLegacyNamedSectionNames(method)...)
	}
	if err := validateNamedGrpcCodeSections(sections, guardNames, legacyNames); err != nil {
		return nil, err
	}
	return sections, nil
}

// checkLegacyGrpcSections 检查 cppPath 里旧模板(官方 grpc::Server + runInLoop/promise)留下的三类手写段:
// 包装函数里的无名投递前段、grpc.<M>.in_loop、grpc.<M>.post_dispatch。新模板没有它们的位置,
// 任一段非空就返回 error,逐行列出「文件:方法:旧段名」—— 宁可拒绝生成,也不吞代码(commit 6c4021ae5 事故)。
// 文件不存在视为没有旧段。
func checkLegacyGrpcSections(cppPath string, methods RPCMethods) error {
	named, err := readNamedGrpcCodeSections(cppPath)
	if err != nil {
		return err
	}
	unnamed, _, err := ReadCodeSectionsFromFile(cppPath, &methods, legacyGrpcWrapperKey, "")
	if err != nil {
		return err
	}
	var found []string
	for _, method := range methods {
		// 包装函数缺无名段时解析器会读到下一个段(in_loop):以命名标记开头即串段,按「不存在」处理,
		// 那个命名段由下面按名字检查,不重复列出。
		code := unnamed[legacyGrpcWrapperKey(method, "")]
		if !startsWithNamedGrpcMarker(code) && !isEmptyGrpcCodeSection(code) {
			found = append(found, cppPath+":"+method.Method()+":"+legacyGrpcPreDispatchSectionName)
		}
		for _, name := range grpcLegacyNamedSectionNames(method) {
			if code, exists := named[name]; exists && !isEmptyGrpcCodeSection(code) {
				found = append(found, cppPath+":"+method.Method()+":"+name)
			}
		}
	}
	if len(found) == 0 {
		return nil
	}
	return fmt.Errorf("%d 个旧 gRPC 守护段非空，拒绝生成(宁可拒绝也不吞代码)；请按 docs/design/grpc-muduo-transport.md §15.8.4 手工迁到 grpc.<M>.guard 后再生成：\n  %s",
		len(found), strings.Join(found, "\n  "))
}

// startsWithNamedGrpcMarker 判断一段代码的首行是不是命名守护段的开始标记。
func startsWithNamedGrpcMarker(code string) bool {
	firstLine, _, _ := strings.Cut(code, "\n")
	return strings.HasPrefix(strings.TrimSpace(firstLine), _config.Global.Naming.YourCodeBegin+":")
}

// isEmptyGrpcCodeSection:去掉标记行(整行恰为开始 / 命名开始 / 结束标记)后不剩任何非空白字符才算空。
// 注释也算非空;标记行上夹带的代码不算标记行,一律按非空处理。
func isEmptyGrpcCodeSection(code string) bool {
	begin := _config.Global.Naming.YourCodeBegin
	end := _config.Global.Naming.YourCodeEnd
	for _, line := range strings.Split(code, "\n") {
		trimmed := strings.TrimSpace(line)
		if trimmed == "" || trimmed == begin || trimmed == end || strings.HasPrefix(trimmed, begin+":") {
			continue
		}
		return false
	}
	return true
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
	return "#include \"" + info.FileBaseNameNoEx() + _config.Global.FileExtensions.GrpcHandlerH + "\""
}
