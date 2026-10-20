package internal

import (
	_ "embed"
	"fmt"
	"path/filepath"
	"regexp"
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

// strayGrpcUnnamedSectionName 是新骨架里本来没有无名段位置的函数(构造函数、<M>()、RegisterMethods 等)中出现的无名段在报错清单里的名字。
const strayGrpcUnnamedSectionName = "无名段"

// outsideGrpcFunctionLabel 是函数外、又不是文件第一个段(FirstCode)的无名段在报错清单里代替方法名的标签。
const outsideGrpcFunctionLabel = "(函数外)"

// legacyGrpcWrapperPattern 匹配旧模板 grpc::Status 包装函数的签名(第 2 组是方法名):"<Svc>Impl::<M>(" 之后允许任意空白与换行,
// 紧跟 grpc::ServerContext(可带前导 ::)。不依赖当前 proto 的方法列表,已从 proto 删掉的方法与折行签名同样命中。
var legacyGrpcWrapperPattern = regexp.MustCompile(`(\w+)Impl::(\w+)\s*\(\s*(?:::)?grpc::ServerContext\b`)

// grpcHandleDefinitionPattern 匹配 Handle* 定义的签名(第 1 组服务名、第 2 组方法名),"(" 前允许空白。
var grpcHandleDefinitionPattern = regexp.MustCompile(`(\w+)Impl::Handle(\w+)\s*\(`)

// legacyGrpcNamedSectionPattern 匹配旧模板的命名段 grpc.<M>.in_loop / grpc.<M>.post_dispatch(任意方法名,第 1 组)。
var legacyGrpcNamedSectionPattern = regexp.MustCompile(`^grpc\.(\w+)\.(?:in_loop|post_dispatch)$`)

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
	sections, unnamed, err := readNamedGrpcCodeSections(dst)
	if err != nil {
		return "", err
	}
	if err := checkGrpcHeaderUnnamedSections(dst, unnamed); err != nil {
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

// checkGrpcHeaderUnnamedSections:头文件骨架只渲染 grpc.header.includes / public / private 三个命名段,
// 无名段在新生成里没有位置。与 cpp 同口径 fail-closed —— 任一无名段非空就返回 error,逐行列出「文件:开始标记所在行」;
// 全部为空才由生成静默丢弃。
func checkGrpcHeaderUnnamedSections(headerPath string, unnamed []grpcUnnamedSection) error {
	var found []string
	for _, section := range unnamed {
		if !isEmptyGrpcCodeSection(section.Code) {
			found = append(found, fmt.Sprintf("%s:%d", headerPath, section.Line))
		}
	}
	if len(found) == 0 {
		return nil
	}
	return fmt.Errorf("头文件有 %d 个无名守护段非空，但骨架只保留 grpc.header.includes / grpc.header.public / grpc.header.private 三个命名段，拒绝生成(宁可拒绝也不吞代码)："+
		"请把代码移进对应命名段或删除后再生成(行号为开始标记所在行)：\n  %s",
		len(found), strings.Join(found, "\n  "))
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

	return utils.GlobalEngine.Render("grpcServiceHandlerHead", grpcServiceHandlerHeadTemplate, data, funcs)
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
	// 预检(validateGrpcHandlerCppSections)同时交回 codeParser 的取段结果:渲染用的 Handle* 体与 FirstCode
	// 就是被检查过 Problems 的那一次解析,不再另走宽松入口重读。
	validated, err := validateGrpcHandlerCppSections(dst, methods)
	if err != nil {
		panic(err)
	}
	namedSections := validated.named
	yourCodesMap, firstCode := validated.parsed.CodeMap, validated.parsed.FirstCode

	emptyStr := ""
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

	return utils.RenderTemplateOrExit("grpcServiceHandlerCpp", grpcServiceHandlerCppTemplate, data)
}

// GenerateGrpcHandlerNameWrapper generates the method lookup key for code parsing.
// 守护段外,这个字面量在生成正文里只能出现在 Handle* 的定义行:RegisterMethods 里调用 Handle* 一律不带类名前缀。
// 守护段内(FirstCode、Handle* 体、grpc.<M>.guard)出现它不影响取段:codeParser 不在段内匹配查找键。
func GenerateGrpcHandlerNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + "Impl::Handle" + info.Method() + "("
}

// grpcGuardSectionName 是 <M>() 里唯一的命名守护段。
func grpcGuardSectionName(info *MethodInfo) string {
	return "grpc." + info.Method() + ".guard"
}

// orphanGrpcHandleSectionName 是新生成带不走的 Handle* 体(proto 已删除该方法,或签名里找不到查找键)在报错清单里的名字。
func orphanGrpcHandleSectionName(method string) string {
	return "Handle" + method + " 体"
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

// grpcHandlerCppSections 是 validateGrpcHandlerCppSections 通过之后交回的 cpp 守护段:
// named 是按名字读到的命名段(grpc.<M>.guard 与已退役的空旧段),parsed 是 codeParser 按 Handle* 查找键取到的 Handle* 体与 FirstCode。
type grpcHandlerCppSections struct {
	named  map[string]string
	parsed CodeSections
}

// validateGrpcHandlerCppSections 是 .h 预检与 .cpp 生成共用的 cpp 守护段检查:
// 先拒绝结构损坏(嵌套 / 未闭合 / 重名),再报全部新骨架没有位置的非空段(checkLegacyGrpcSections,带迁移指引),
// 再拒绝未识别的命名段,然后按 codeParser 的 Problems 拒绝(refuseUnexpectedGrpcCodeSectionProblems),
// 最后拒绝被 codeParser 当成 FirstCode / Handle* 体取走的命名段(refuseNamedSectionsTakenAsUnnamed)——
// 前三步是按段形状的扫描(整行标记、函数归属),与 codeParser 的取段规则并不逐条一致;生成时真正取段的是 codeParser,
// 它读到却带不走、判为结构损坏、或取错了的段只能按它的结果报出,否则会被静默丢掉或错位。
func validateGrpcHandlerCppSections(cppPath string, methods RPCMethods) (grpcHandlerCppSections, error) {
	sections, _, err := readNamedGrpcCodeSections(cppPath)
	if err != nil {
		return grpcHandlerCppSections{}, err
	}
	if err := checkLegacyGrpcSections(cppPath, methods); err != nil {
		return grpcHandlerCppSections{}, err
	}
	guardNames := make([]string, 0, len(methods))
	for _, method := range methods {
		guardNames = append(guardNames, grpcGuardSectionName(method))
	}
	// 走到这里旧命名段必然全空(checkLegacyGrpcSections):不论方法是否还在 proto 里,一律按空段接受并丢弃。
	var legacyNames []string
	for name := range sections {
		if legacyGrpcNamedSectionPattern.MatchString(name) {
			legacyNames = append(legacyNames, name)
		}
	}
	if err := validateNamedGrpcCodeSections(sections, guardNames, legacyNames); err != nil {
		return grpcHandlerCppSections{}, err
	}
	parsed, err := ReadCodeSectionsByKeys(cppPath, methodLookupKeys(&methods, GenerateGrpcHandlerNameWrapper, ""))
	if err != nil {
		return grpcHandlerCppSections{}, err
	}
	if err := refuseUnexpectedGrpcCodeSectionProblems(cppPath, parsed, guardNames); err != nil {
		return grpcHandlerCppSections{}, err
	}
	if err := refuseNamedSectionsTakenAsUnnamed(cppPath, parsed, methods); err != nil {
		return grpcHandlerCppSections{}, err
	}
	return grpcHandlerCppSections{named: sections, parsed: parsed}, nil
}

// refuseNamedSectionsTakenAsUnnamed 拒绝 codeParser 把命名段当成 FirstCode / Handle* 体取走的文件。
// codeParser 按「行内含开始标记」起 FirstCode 与方法段,不区分命名段:签名命中的 Handle* 自己没有段、结束括号又不是
// 函数体结束行(isFunctionBodyEnd,如 "} // 注释")时,解析停在它的函数体里,其后 <M>() 里的 grpc.<M>.guard 被当成这个 Handle* 的体;
// 缺 FirstCode、文件第一个段又是 guard 时,它被当成 FirstCode。此前该方法没有段可覆盖,Problems 不记,扫描器也只按名字看命名段 ——
// 重生成会把 guard 复制进 Handle* 体 / 文件开头(readNamedGrpcCodeSections 仍按名字把它放回 <M>()),代码错位、产物里 guard 重名,
// 下一次生成才因重名拒绝。这里提前拒绝,不写出错位的产物。
func refuseNamedSectionsTakenAsUnnamed(cppPath string, parsed CodeSections, methods RPCMethods) error {
	var found []string
	if name := leadingNamedSectionName(parsed.FirstCode); name != "" {
		found = append(found, cppPath+":FirstCode:"+name)
	}
	for _, method := range methods {
		if name := leadingNamedSectionName(parsed.CodeMap[GenerateGrpcHandlerNameWrapper(method, "")]); name != "" {
			found = append(found, cppPath+":Handle"+method.Method()+" 体:"+name)
		}
	}
	if len(found) == 0 {
		return nil
	}
	return fmt.Errorf("%d 个命名守护段被当成 FirstCode / Handle* 体取走(其前签名命中的 Handle* 没有自己的守护段、结束括号又不是顶格的 \"}\" / \"};\"，"+
		"或文件缺 FirstCode)，重生成会把它复制到错误的位置，拒绝生成(宁可拒绝也不吞代码)：请给该 Handle* 补回守护段、把结束括号改成顶格的 \"}\"，"+
		"或补回文件开头的 FirstCode，再生成：\n  %s", len(found), strings.Join(found, "\n  "))
}

// leadingNamedSectionName:code(codeParser 取到的段,含首尾标记)以命名开始标记起头时返回段名,否则为空。
func leadingNamedSectionName(code string) string {
	first, _, _ := strings.Cut(code, "\n")
	trimmed := strings.TrimSpace(first)
	if !isCodeSectionBeginMarker(trimmed) {
		return ""
	}
	return codeSectionName(trimmed)
}

// refuseUnexpectedGrpcCodeSectionProblems 按 codeParser 的 Problems 拒绝生成,只放过当前方法的 grpc.<M>.guard:
// 它们落在 <M>() 里,codeParser 按 Handle* 键取段时整段跳过是预期,由 readNamedGrpcCodeSections 按名字读走。
// 其余一律拒绝,典型的三种手改形状前三步都放行、却会被 codeParser 静默丢掉:
//   - 文件的第一个段落在某个没有段的现行 Handle* 之后、函数外:扫描器按「文件第一个段」当 FirstCode 放行,
//     codeParser 已见过方法、不再认 FirstCode,整段跳过(CodeSectionUnplaced);
//   - 同一 Handle* 定义两次(#if / #else):扫描器认为两处都是各自函数的第一个段,codeParser 后段覆盖前段(CodeSectionOverwritten);
//   - 段内某行夹带结束标记文本但不独占一行:扫描器只认整行结束标记,codeParser 按行内含结束标记提前闭合,
//     段的后半截落到段外,真正的结束标记成了孤立标记(CodeSectionBroken)。
func refuseUnexpectedGrpcCodeSectionProblems(cppPath string, parsed CodeSections, guardNames []string) error {
	expected := make(map[string]bool, len(guardNames))
	for _, name := range guardNames {
		expected[name] = true
	}
	var unexpected []CodeSectionProblem
	for _, problem := range parsed.Problems {
		if problem.Kind == CodeSectionUnplaced && expected[problem.SectionName] {
			continue
		}
		unexpected = append(unexpected, problem)
	}
	parsed.Problems = unexpected
	return RefuseLostCodeSections(cppPath, parsed)
}

// checkLegacyGrpcSections 检查 cppPath 里新骨架没有位置、重生成会被丢掉的手写段:任一段非空就返回 error,
// 逐行列出「文件:方法:段名」;全部为空才由生成静默丢弃 —— 宁可拒绝生成,也不吞代码(commit 6c4021ae5 事故)。
// 检测不依赖当前 proto 的方法列表,已从 proto 删掉的方法、折行的签名同样覆盖:
//   - 旧模板(官方 grpc::Server + runInLoop/promise)的 grpc::Status 包装函数按签名形状识别(legacyGrpcWrapperPattern),
//     其中的无名段记为「无名投递前段」;
//   - 旧命名段 grpc.<M>.in_loop / grpc.<M>.post_dispatch 不论位置、不论方法是否还在 proto 里,按名字识别;
//   - 新生成只带走当前方法 Handle* 里的第一个段(codeParser 按 GenerateGrpcHandlerNameWrapper 取段,签名起始行必须逐字含该键):
//     proto 已删除的方法、签名起始行里找不到查找键的 Handle*、以及 Handle* 里第一个段之后的无名段,记为「Handle<M> 体」;
//   - 其余函数(构造函数、<M>()、RegisterMethods)里本来没有无名段的位置,出现即记为「无名段」;
//   - 函数外的无名段只有文件里的第一个段会被 codeParser 当作 FirstCode 带走;其后的(手工插在函数之间的段,
//     或扫描器没认出所在函数、归到函数外的段)记为「(函数外):无名段」。
//
// 报错开头的数目按段计;同一「文件:方法:段名」下有多个段时只列一行,行尾注明段数。
// 其余命名段由 validateNamedGrpcCodeSections 按名字判定,不在此列。文件不存在视为没有旧段。
func checkLegacyGrpcSections(cppPath string, methods RPCMethods) error {
	signatures, sections, err := scanGrpcCppSections(cppPath)
	if err != nil {
		return err
	}
	handleKeys := make(map[string]bool, len(methods))
	for _, method := range methods {
		handleKeys[GenerateGrpcHandlerNameWrapper(method, "")] = true
	}
	// entries 按首次出现的顺序列「文件:方法:段名」,counts 记每条下的段数,lostSections 是段的总数。
	var entries []string
	counts := make(map[string]int)
	lostSections := 0
	// 函数下标 → 该函数里已出现的段数(命名段、空段都算):codeParser 只取 Handle* 查找键之后的第一个段。
	ordinals := make(map[int]int)
	for index, section := range sections {
		ordinal := ordinals[section.Function]
		ordinals[section.Function]++
		if isEmptyGrpcCodeSection(section.Code) {
			continue
		}
		var method, name string
		switch {
		case section.Name != "":
			match := legacyGrpcNamedSectionPattern.FindStringSubmatch(section.Name)
			if match == nil {
				continue
			}
			method, name = match[1], section.Name
		case section.Function >= 0:
			var lost bool
			method, name, lost = lostUnnamedGrpcSection(signatures[section.Function], ordinal, handleKeys)
			if !lost {
				continue
			}
		case index == 0:
			continue // FirstCode
		default:
			method, name = outsideGrpcFunctionLabel, strayGrpcUnnamedSectionName
		}
		entry := cppPath + ":" + method + ":" + name
		if counts[entry] == 0 {
			entries = append(entries, entry)
		}
		counts[entry]++
		lostSections++
	}
	if lostSections == 0 {
		return nil
	}
	found := make([]string, 0, len(entries))
	for _, entry := range entries {
		if count := counts[entry]; count > 1 {
			entry += fmt.Sprintf("(%d 段)", count)
		}
		found = append(found, entry)
	}
	return fmt.Errorf("%d 个守护段非空但新骨架没有它们的位置，拒绝生成(宁可拒绝也不吞代码)：旧模板的投递前 / in_loop / post_dispatch 段请按 docs/design/grpc-muduo-transport.md §15.8.4 手工迁到 grpc.<M>.guard；"+
		"proto 已删除方法的 Handle* 体与其余位置的无名段请先手工移走或删除，再生成：\n  %s",
		lostSections, strings.Join(found, "\n  "))
}

// lostUnnamedGrpcSection 判断签名为 signature 的函数里第 ordinal 个段(从 0 计)——一个非空无名段——在新生成里有没有去处;
// 没有去处时返回报错清单里的方法名与段名(规则见 checkLegacyGrpcSections)。
func lostUnnamedGrpcSection(signature string, ordinal int, handleKeys map[string]bool) (method, name string, lost bool) {
	if match := legacyGrpcWrapperPattern.FindStringSubmatch(signature); match != nil {
		return match[2], legacyGrpcPreDispatchSectionName, true
	}
	startLine, _, _ := strings.Cut(signature, "\n")
	if match := grpcHandleDefinitionPattern.FindStringSubmatch(signature); match != nil {
		key := match[1] + "Impl::Handle" + match[2] + "("
		// codeParser 只在签名行(即这里的起始行)上认查找键:键只出现在折行后的续行里时它带不走这个段。
		if ordinal == 0 && handleKeys[key] && strings.Contains(startLine, key) {
			return "", "", false
		}
		return match[2], orphanGrpcHandleSectionName(match[2]), true
	}
	// 签名的起始行必含 "<Svc>Impl::<Name>"(isGrpcCppFunctionStart),取不到函数名只是防御,退回签名首行。
	function := startLine
	if match := grpcImplFunctionPattern.FindStringSubmatch(signature); match != nil {
		function = match[2]
	}
	return function, strayGrpcUnnamedSectionName, true
}

// isEmptyGrpcCodeSection:去掉标记行(整行恰为开始 / 命名开始 / 结束标记)后不剩任何非空白字符才算空。
// 注释也算非空;标记行上夹带的代码不算标记行,一律按非空处理。
func isEmptyGrpcCodeSection(code string) bool {
	end := _config.Global.Naming.YourCodeEnd
	for _, line := range strings.Split(code, "\n") {
		trimmed := strings.TrimSpace(line)
		if trimmed == "" || trimmed == end || isCodeSectionBeginMarker(trimmed) {
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
