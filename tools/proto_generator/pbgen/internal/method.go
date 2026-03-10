package internal

import (
	"bufio"
	"bytes"
	"fmt"
	"os"
	"sort"
	"strings"
	"sync"
	"text/template"

	"go.uber.org/zap" // 引入zap结构化日志字段
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger" // 引入全局logger包
)

// Type definitions for callback functions
type checkRepliedCb func(methods *RPCMethods) bool

// Function to write the header file for service ID
func writeServiceIdHeadFile(serviceInfo []*RPCServiceInfo) {
	if len(serviceInfo) <= 0 {
		return
	}

	fileName := serviceInfo[0].ServiceInfoHeadInclude()
	outputPath := _config.Global.Paths.ServiceInfoDir + fileName
	utils2.WriteFileIfChanged(outputPath, []byte(GenServiceIdHeader(serviceInfo)))

	logger.Global.Info("服务ID头文件生成完成",
		zap.String("file_path", outputPath),
		zap.Int("service_count", len(serviceInfo)),
	)
}

// GenServiceIdHeader 使用模板生成服务 ID 头文件内容
func GenServiceIdHeader(serviceInfo []*RPCServiceInfo) string {
	if len(serviceInfo) == 0 {
		return ""
	}

	// 创建模板内容
	tmpl := `#pragma once
#include <cstdint>

{{.IncludeName}}
{{- range .ServiceInfo }}
{{range .MethodInfo}}
constexpr uint32_t {{.KeyName}}{{$.MessageIdName}} = {{.Id}};
constexpr uint32_t {{.KeyName}}Index = {{.Index}};
#define {{.KeyName}}Method  ::{{.Service}}_Stub::descriptor()->method({{.Index}})
{{end}}
{{end}}
`
	// 创建模板并解析
	t, err := template.New("serviceIdHeader").Parse(tmpl)
	if err != nil {
		logger.Global.Fatal("解析服务ID头文件模板失败", zap.Error(err))
		panic(err)
	}

	// 使用模板填充数据
	var buf bytes.Buffer
	data := struct {
		IncludeName   string
		ServiceInfo   []*RPCServiceInfo
		MessageIdName string
	}{
		IncludeName:   serviceInfo[0].IncludeName(),
		ServiceInfo:   serviceInfo,
		MessageIdName: _config.Global.Naming.MessageId,
	}

	// 执行模板并写入 buffer
	err = t.Execute(&buf, data)
	if err != nil {
		logger.Global.Fatal("执行服务ID头文件模板失败", zap.Error(err))
		panic(err)
	}

	return buf.String()
}

// 获取 C++ 头文件字符串，使用 text/template
func GetServiceHandlerHeadStr(methods RPCMethods) (string, error) {
	if len(methods) == 0 {
		return "", nil
	}

	const tmplStr = `#pragma once
{{.Include}}

class {{.Service}}Handler : public ::{{.Service}}
{
public:
{{.MethodDeclarations}}
};`

	tmpl, err := template.New("header").Parse(tmplStr)
	if err != nil {
		logger.Global.Error("解析服务处理类头文件模板失败", zap.Error(err))
		return "", err
	}

	data := struct {
		Include            string
		Service            string
		MethodDeclarations string
	}{
		Include:            methods[0].IncludeName(),
		Service:            methods[0].Service(),
		MethodDeclarations: buildServiceHandlerDeclarations(methods),
	}

	var result strings.Builder
	err = tmpl.Execute(&result, data)
	if err != nil {
		logger.Global.Error("执行服务处理类头文件模板失败", zap.Error(err))
		return "", err
	}

	return result.String(), nil
}

func buildServiceHandlerDeclarations(methods RPCMethods) string {
	declarations := make([]string, 0, len(methods))
	for _, method := range methods {
		declarations = append(declarations, fmt.Sprintf(
			"    void %s(%sconst %s* request,\n        %s* response,\n        ::google::protobuf::Closure* done) override;",
			method.Method(),
			_config.Global.Naming.GoogleMethodController,
			method.CppRequest(),
			method.CppResponse(),
		))
	}

	return strings.Join(declarations, "\n\n")
}

// Function to get the header string for player method handlers
func GetPlayerServiceHeadStr(methods RPCMethods) (string, error) {
	const playerMethodHeadTemplate = `#pragma once

{{.IncludeName}}
{{.PlayerServiceIncludeName}}
{{.MacroReturnIncludeName}}

class {{.Service}}Handler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

{{.MethodHandlerFunctions}}
};
`

	type PlayerMethodData struct {
		IncludeName              string
		PlayerServiceIncludeName string
		MacroReturnIncludeName   string
		Service                  string
		MethodHandlerFunctions   string
	}

	// 填充模板所需的数据
	data := PlayerMethodData{
		IncludeName:              methods[0].IncludeName(),
		PlayerServiceIncludeName: _config.Global.Naming.PlayerServiceIncludeName,
		MacroReturnIncludeName:   _config.Global.Naming.MacroReturnIncludeName,
		Service:                  methods[0].Service(),
		MethodHandlerFunctions:   getPlayerMethodHandlerFunctions(methods),
	}

	// 创建模板
	tmpl, err := template.New("playerMethodHeadTemplate").Parse(playerMethodHeadTemplate)
	if err != nil {
		logger.Global.Error("解析玩家服务头文件模板失败", zap.Error(err))
		return "", err
	}

	// 使用 bytes.Buffer 来捕获模板输出
	var output bytes.Buffer
	err = tmpl.Execute(&output, data)
	if err != nil {
		logger.Global.Error("执行玩家服务头文件模板失败", zap.Error(err))
		return "", err
	}

	return output.String(), nil
}

const playerMethodFunctionsTemplate = `{{- range .Methods }}
    static void {{ .Method }}({{ $.PlayerMethodController }}
        const {{ .CppRequest }}* request,
        {{ .CppResponse }}* response);

{{- end }}

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
{{- range $index, $method := .Methods }}
        case {{ $index }}:
			{
            {{ $method.Method }}(player,
                static_cast<const {{ $method.CppRequest }}*>(request),
                static_cast<{{ $method.CppResponse }}*>(response));
{{- if not $method.IsEmptyResponse }}
            TRANSFER_ERROR_MESSAGE(static_cast<{{ $method.CppResponse }}*>(response));
{{- end }}
			}
            break;
{{- end }}
        default:
            break;
        }
    }
`

type PlayerMethod struct {
	Method          string
	CppRequest      string
	CppResponse     string
	IsEmptyResponse bool
}

type PlayerMethodFunctionsData struct {
	PlayerMethodController string
	Methods                []PlayerMethod
}

func getPlayerMethodHandlerFunctions(methods RPCMethods) string {
	var methodList []PlayerMethod

	for _, method := range methods {
		methodList = append(methodList, PlayerMethod{
			Method:          method.Method(),
			CppRequest:      method.CppRequest(),
			CppResponse:     method.CppResponse(),
			IsEmptyResponse: strings.Contains(method.CppResponse(), _config.Global.Naming.EmptyResponse),
		})
	}

	data := PlayerMethodFunctionsData{
		PlayerMethodController: _config.Global.Naming.PlayerMethodController,
		Methods:                methodList,
	}

	tmpl, err := template.New("playerMethodFunctions").Parse(playerMethodFunctionsTemplate)
	if err != nil {
		logger.Global.Fatal("解析玩家方法函数模板失败", zap.Error(err))
		panic(err)
	}

	var output bytes.Buffer
	err = tmpl.Execute(&output, data)
	if err != nil {
		logger.Global.Fatal("执行玩家方法函数模板失败", zap.Error(err))
		panic(err)
	}

	return output.String()
}

const playerMethodRepliedHeadTemplate = `
#pragma once
{{.IncludeName}}
{{.PlayerServiceRepliedIncludeName}}

class {{.Service}}Reply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;

{{.MethodHandlerFunctions}}
};
`

type PlayerMethodRepliedData struct {
	IncludeName                     string
	PlayerServiceRepliedIncludeName string
	Service                         string
	MethodHandlerFunctions          string
}

func GetPlayerMethodRepliedHeadStr(methods RPCMethods) (string, error) {
	// 填充模板所需的数据
	data := PlayerMethodRepliedData{
		IncludeName:                     methods[0].IncludeName(),
		PlayerServiceRepliedIncludeName: _config.Global.Naming.PlayerServiceRepliedIncludeName,
		Service:                         methods[0].Service(),
		MethodHandlerFunctions:          getPlayerMethodRepliedHandlerFunctions(methods),
	}

	// 创建模板
	tmpl, err := template.New("playerMethodRepliedHeadTemplate").Parse(playerMethodRepliedHeadTemplate)
	if err != nil {
		logger.Global.Error("解析玩家回复服务头文件模板失败", zap.Error(err))
		return "", err
	}

	// 使用 bytes.Buffer 来捕获模板输出
	var output bytes.Buffer
	err = tmpl.Execute(&output, data)
	if err != nil {
		logger.Global.Error("执行玩家回复服务头文件模板失败", zap.Error(err))
		return "", err
	}

	return output.String(), nil
}

const playerMethodRepliedFunctionsTemplate = `{{- range .Methods }}
    static void {{ .Method }}({{ $.PlayerMethodController }}
        const {{ .CppRequest }}* request,
        {{ .CppResponse }}* response);

{{- end }}

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
{{- range $index, $method := .Methods }}
        case {{ $index }}:
            {{ $method.Method }}(player,
                nullptr,
                static_cast<{{ $method.CppResponse }}*>(response));
            break;
{{- end }}
        default:
            break;
        }
    }
`

type PlayerRepliedMethod struct {
	Method      string
	CppRequest  string
	CppResponse string
}

type PlayerRepliedFunctionsData struct {
	PlayerMethodController string
	Methods                []PlayerRepliedMethod
}

func getPlayerMethodRepliedHandlerFunctions(methods RPCMethods) string {
	var methodList []PlayerRepliedMethod

	for _, method := range methods {
		methodList = append(methodList, PlayerRepliedMethod{
			Method:      method.Method(),
			CppRequest:  method.CppRequest(),
			CppResponse: method.CppResponse(),
		})
	}

	data := PlayerRepliedFunctionsData{
		PlayerMethodController: _config.Global.Naming.PlayerMethodController,
		Methods:                methodList,
	}

	tmpl, err := template.New("playerMethodRepliedFunctions").Parse(playerMethodRepliedFunctionsTemplate)
	if err != nil {
		logger.Global.Fatal("解析玩家回复方法函数模板失败", zap.Error(err))
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, data); err != nil {
		logger.Global.Fatal("执行玩家回复方法函数模板失败", zap.Error(err))
		panic(err)
	}

	return output.String()
}

func GetServiceRepliedHandlerHeadStr(methods RPCMethods) (string, error) {
	const methodRepliedHandlerHeadTemplate = `#pragma once
#include "muduo/net/TcpConnection.h"
{{.IncludeName }}
using namespace muduo;
using namespace muduo::net;

{{.MethodDeclarations}}
`
	type MethodRepliedHandlerData struct {
		IncludeName            string
		MethodDeclarations     string
		RepliedHandlerFileName string
	}

	// Ensure there are methods in the list
	if len(methods) == 0 {
		return "", nil
	}

	data := MethodRepliedHandlerData{
		IncludeName:            methods[0].IncludeName(),
		MethodDeclarations:     buildServiceRepliedHandlerDeclarations(methods),
		RepliedHandlerFileName: _config.Global.Naming.RepliedHandlerFile,
	}

	// Parse and execute the template
	tmpl, err := template.New("methodRepliedHandlerHead").Parse(methodRepliedHandlerHeadTemplate)
	if err != nil {
		logger.Global.Fatal("解析回复处理类头文件模板失败", zap.Error(err))
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, data); err != nil {
		logger.Global.Fatal("执行回复处理类头文件模板失败", zap.Error(err))
		panic(err)
	}

	return output.String(), nil
}

func buildServiceRepliedHandlerDeclarations(methods RPCMethods) string {
	declarations := make([]string, 0, len(methods))
	for _, method := range methods {
		declarations = append(declarations, fmt.Sprintf(
			"void On%s%s(const TcpConnectionPtr& conn, const std::shared_ptr<%s>& replied, Timestamp timestamp);",
			method.KeyName(),
			_config.Global.Naming.RepliedHandlerFile,
			method.CppResponse(),
		))
	}

	return strings.Join(declarations, "\n\n")
}

func buildFunctionDefinition(signature string, body string) string {
	var output strings.Builder
	output.WriteString(signature)
	output.WriteString("\n{\n")
	if body != "" {
		output.WriteString(body)
		output.WriteString("\n")
	}
	output.WriteString("}")
	return output.String()
}

func buildGeneratedCppFile(prefix string, firstCode string, functionDefinitions []string) string {
	var output strings.Builder
	output.WriteString(prefix)
	output.WriteString("\n\n")
	output.WriteString(firstCode)
	output.WriteString("\n")
	if len(functionDefinitions) > 0 {
		output.WriteString(strings.Join(functionDefinitions, "\n\n"))
		output.WriteString("\n")
	}

	return output.String()
}

// ReadCodeSectionsFromFile 函数接收一个函数作为参数，动态选择 A 或 B 方法
func ReadCodeSectionsFromFile(cppFileName string, methods *RPCMethods, methodFunc func(info *MethodInfo, funcParam string) string, funcParam string) (map[string]string, string, error) {
	// 创建一个 map 来存储每个 MethodInfo 的 name 和对应的 yourCode
	codeMap := make(map[string]string)

	// 打开文件
	fd, err := os.Open(cppFileName)
	if err != nil {
		logger.Global.Fatal("打开C++文件失败",
			zap.String("file_name", cppFileName),
			zap.Error(err),
		)
	}
	defer fd.Close()

	// 创建一个扫描器来按行读取文件
	scanner := bufio.NewScanner(fd)

	// 记录当前正在处理的 yourCode
	var currentCode string
	var currentMethod *MethodInfo
	var firstCode string // 用于保存第一个特殊的 yourCode
	var isFirstCode bool // 标记是否处理了第一个特殊的 yourCode
	var inFirstCode bool // 标记是否在处理第一个特殊的 yourCode
	var inMethodCode bool

	logger.Global.Debug("开始读取C++文件中的代码段",
		zap.String("file_name", cppFileName),
		zap.Int("method_count", len(*methods)),
	)

	// 遍历文件的每一行
	for scanner.Scan() {
		line := scanner.Text() + "\n"

		// 如果正在处理第一个 yourCode，并且发现 YourCodeEnd
		if inFirstCode && strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
			firstCode += line
			inFirstCode = false
			continue // 跳过其他处理，继续后续的代码处理
		} else if inFirstCode {
			firstCode += line
		}

		// 如果是第一个特殊的 yourCode块
		if !isFirstCode && strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
			firstCode = line
			inFirstCode = true
			isFirstCode = true
			continue
		}

		if nil == currentMethod {
			// 如果是方法的开始行，检查是否是我们关心的 MethodInfo 名称
			for _, method := range *methods {
				handlerName := methodFunc(method, funcParam)
				if strings.Contains(line, handlerName) {
					currentMethod = method
					logger.Global.Debug("匹配到方法代码段开始",
						zap.String("handler_name", handlerName),
						zap.String("method", method.Method()),
					)
					break
				}
			}
		}

		// 如果找到了当前方法的开始，接着读取直到找到结束
		if currentMethod != nil {
			if strings.Contains(line, _config.Global.Naming.YourCodeBegin) {
				inMethodCode = true
				currentCode += line
			} else if strings.Contains(line, _config.Global.Naming.YourCodeEnd) {
				currentCode += line
				// 使用 methodFunc currentMethod
				handlerName := methodFunc(currentMethod, funcParam)
				normalizedCode := utils2.TrimTrailingLineBreaks(currentCode)
				codeMap[handlerName] = normalizedCode
				logger.Global.Debug("方法代码段读取完成",
					zap.String("handler_name", handlerName),
					zap.Int("code_length", len(normalizedCode)),
				)
				currentMethod = nil
				currentCode = ""
				inMethodCode = false
			} else if inMethodCode {
				currentCode += line
			}
		}
	}

	// 检查扫描器错误
	if err := scanner.Err(); err != nil {
		logger.Global.Error("读取C++文件时出错",
			zap.String("file_name", cppFileName),
			zap.Error(err),
		)
		return codeMap, firstCode, err
	}

	// 如果没有找到第一个 yourCode，使用默认的 global_value.YourCodePair
	if firstCode == "" {
		firstCode = _config.Global.Naming.YourCodePair
		logger.Global.Debug("未找到第一个代码段，使用默认值",
			zap.String("default_code", firstCode),
		)
	}
	firstCode = utils2.TrimTrailingLineBreaks(firstCode)

	// 检查是否有方法没有找到对应的 yourCode，如果没有找到，则添加默认值
	defaultCode := utils2.TrimTrailingLineBreaks(_config.Global.Naming.YourCodePair)
	missingCount := 0
	for _, method := range *methods {
		handlerName := methodFunc(method, funcParam)
		if _, exists := codeMap[handlerName]; !exists {
			codeMap[handlerName] = defaultCode
			missingCount++
		}
	}

	logger.Global.Info("C++文件代码段读取完成",
		zap.String("file_name", cppFileName),
		zap.Int("code_map_size", len(codeMap)),
		zap.Int("missing_method_count", missingCount),
	)

	return codeMap, firstCode, nil
}

func GenerateMethodHandlerNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + _config.Global.Naming.HandlerFile + "::" + info.Method() + "("
}

func GenerateMethodHandlerNameWithClassPrefixWrapper(info *MethodInfo, classPrefix string) string {
	return classPrefix + "::" + info.Method() + "("
}

func GenerateMethodHandlerKeyNameWrapper(info *MethodInfo, _ string) string {
	return "On" + info.KeyName() + _config.Global.Naming.RepliedHandlerFile
}

func GetServiceHandlerCppStr(dst string, methods RPCMethods, className string, includeName string) string {
	type HandlerMethod struct {
		HandlerName string
		CppRequest  string
		CppResponse string
		Code        string
		HasCode     bool
	}

	if len(methods) == 0 {
		return ""
	}

	ex := ""

	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateMethodHandlerNameWrapper, ex)

	firstMethodInfo := methods[0]

	var methodList []HandlerMethod
	for _, methodInfo := range methods {
		handlerName := GenerateMethodHandlerNameWrapper(methodInfo, ex)
		code, exists := yourCodesMap[handlerName]
		methodList = append(methodList, HandlerMethod{
			HandlerName: handlerName,
			CppRequest:  methodInfo.CppRequest(),
			CppResponse: methodInfo.CppResponse(),
			Code:        code,
			HasCode:     exists,
		})
	}

	methodDefinitions := make([]string, 0, len(methodList))
	for _, method := range methodList {
		methodDefinitions = append(methodDefinitions, buildFunctionDefinition(
			fmt.Sprintf(
				`void %s%sconst %s* request,
    %s* response,
    ::google::protobuf::Closure* done)`,
				method.HandlerName,
				_config.Global.Naming.GoogleMethodController,
				method.CppRequest,
				method.CppResponse,
			),
			method.Code,
		))
	}

	return buildGeneratedCppFile(firstMethodInfo.CppHandlerIncludeName(), firstCode, methodDefinitions)
}

func GetServiceRepliedHandlerCppStr(dst string, methods RPCMethods, _ string, _ string) string {
	type RepliedHandlerMethod struct {
		FuncName    string
		CppResponse string
		Code        string
		HasCode     bool
		KeyName     string
	}

	if len(methods) == 0 {
		return ""
	}

	emptyString := ""

	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateMethodHandlerKeyNameWrapper, emptyString)
	firstMethodInfo := methods[0]

	var methodsData []RepliedHandlerMethod
	for _, method := range methods {
		funcName := GenerateMethodHandlerKeyNameWrapper(method, emptyString)
		code, exists := yourCodesMap[funcName]
		methodsData = append(methodsData, RepliedHandlerMethod{
			FuncName:    funcName,
			CppResponse: method.CppResponse(),
			Code:        code,
			HasCode:     exists,
			KeyName:     method.KeyName(),
		})
	}

	registrationLines := make([]string, 0, len(methodsData))
	handlerDefinitions := make([]string, 0, len(methodsData)+1)
	for _, method := range methodsData {
		if !method.HasCode {
			continue
		}

		registrationLines = append(registrationLines, fmt.Sprintf(
			`    gRpcResponseDispatcher.registerMessageCallback<%s>(%s%s,
        std::bind(&%s, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));`,
			method.CppResponse,
			method.KeyName,
			_config.Global.Naming.MessageId,
			method.FuncName,
		))
		handlerDefinitions = append(handlerDefinitions, buildFunctionDefinition(
			fmt.Sprintf(
				"void %s(const TcpConnectionPtr& conn, const std::shared_ptr<%s>& replied, Timestamp timestamp)",
				method.FuncName,
				method.CppResponse,
			),
			method.Code,
		))
	}

	initDefinition := buildFunctionDefinition(
		fmt.Sprintf("void Init%s%s()", firstMethodInfo.Service(), _config.Global.Naming.RepliedHandlerFile),
		strings.Join(registrationLines, "\n"),
	)

	filePrefix := strings.Join([]string{
		firstMethodInfo.CppRepliedHandlerIncludeName(),
		fmt.Sprintf("#include \"rpc/%s%s\"", _config.Global.DirectoryNames.ServiceInfoName, firstMethodInfo.ServiceInfoHeadInclude()),
		"#include \"network/codec/message_response_dispatcher.h\"",
		"",
		"extern MessageResponseDispatcher gRpcResponseDispatcher;",
	}, "\n")

	functionDefinitions := append([]string{initDefinition}, handlerDefinitions...)
	return buildGeneratedCppFile(filePrefix, firstCode, functionDefinitions)
}

func GetPlayerServiceHandlerCppStr(dst string, methods RPCMethods, className string, includeName string) string {
	type PlayerHandlerMethod struct {
		HandlerName string
		CppRequest  string
		CppResponse string
		Code        string
		HasCode     bool
	}

	if len(methods) == 0 {
		return includeName // Still return include if empty
	}

	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateMethodHandlerNameWithClassPrefixWrapper, className)

	var methodList []PlayerHandlerMethod
	for _, method := range methods {
		handlerName := GenerateMethodHandlerNameWithClassPrefixWrapper(method, className)
		code, exists := yourCodesMap[handlerName]
		methodList = append(methodList, PlayerHandlerMethod{
			HandlerName: handlerName,
			CppRequest:  method.CppRequest(),
			CppResponse: method.CppResponse(),
			Code:        code,
			HasCode:     exists,
		})
	}

	methodDefinitions := make([]string, 0, len(methodList))
	for _, method := range methodList {
		methodDefinitions = append(methodDefinitions, buildFunctionDefinition(
			fmt.Sprintf(
				`void %s%sconst %s* request,
	%s* response)`,
				method.HandlerName,
				_config.Global.Naming.PlayerMethodController,
				method.CppRequest,
				method.CppResponse,
			),
			method.Code,
		))
	}

	return buildGeneratedCppFile(includeName, firstCode, methodDefinitions)
}

func GenRegisterFile(wg *sync.WaitGroup, dst string, cb checkRepliedCb) {
	const registerFileTemplate = `#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>
{{- range .Includes }}
{{ . }}
{{ end }}

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
{{- range .InitLines }}
{{ . }}
{{ end }}
}
`
	type RegisterFileData struct {
		Includes  []string
		InitLines []string
	}

	defer wg.Done()

	var includes []string
	var initLines []string

	logger.Global.Info("开始生成服务注册文件",
		zap.String("output_path", dst),
	)

	for _, service := range GlobalRPCServiceList {
		if !cb(&service.MethodInfo) {
			logger.Global.Debug("服务不满足回调条件，跳过注册",
				zap.String("service_name", service.Service()),
			)
			continue
		}
		first := service.MethodInfo[0]
		includes = append(includes, first.CppHandlerIncludeName())
		initLine := " gNodeService.emplace(\"" + first.Service() + "\", std::make_unique_for_overwrite<" + first.Service() + _config.Global.Naming.HandlerFile + ">());"
		initLines = append(initLines, initLine)

		logger.Global.Debug("添加服务注册行",
			zap.String("service_name", first.Service()),
			zap.String("init_line", initLine),
		)
	}

	templateData := RegisterFileData{
		Includes:  includes,
		InitLines: initLines,
	}

	tmpl, err := template.New("registerFile").Parse(registerFileTemplate)
	if err != nil {
		logger.Global.Fatal("解析服务注册文件模板失败", zap.Error(err))
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, templateData); err != nil {
		logger.Global.Fatal("执行服务注册文件模板失败", zap.Error(err))
		panic(err)
	}

	utils2.WriteFileIfChanged(dst, output.Bytes())
	logger.Global.Info("服务注册文件生成完成",
		zap.String("output_path", dst),
		zap.Int("include_count", len(includes)),
		zap.Int("init_line_count", len(initLines)),
	)
}

func WriteRepliedRegisterFile(wg *sync.WaitGroup, dst string, cb checkRepliedCb) {
	defer wg.Done()

	var initFuncList []string

	logger.Global.Info("开始生成回复注册文件",
		zap.String("output_path", dst),
	)

	for _, service := range GlobalRPCServiceList {
		if !cb(&service.MethodInfo) {
			logger.Global.Debug("服务不满足回调条件，跳过回复注册",
				zap.String("service_name", service.Service()),
			)
			continue
		}
		first := service.MethodInfo[0]
		initFunc := "Init" + first.Service() + _config.Global.Naming.RepliedHandlerFile
		initFuncList = append(initFuncList, initFunc)

		logger.Global.Debug("添加回复注册函数",
			zap.String("service_name", first.Service()),
			zap.String("init_func", initFunc),
		)
	}

	var output strings.Builder
	for _, initFunc := range initFuncList {
		output.WriteString("void ")
		output.WriteString(initFunc)
		output.WriteString("();\n")
	}
	if len(initFuncList) > 0 {
		output.WriteString("\n")
	}
	output.WriteString("void InitReply()\n{\n")
	for _, initFunc := range initFuncList {
		output.WriteString("    ")
		output.WriteString(initFunc)
		output.WriteString("();\n")
	}
	output.WriteString("}\n")

	utils2.WriteFileIfChanged(dst, []byte(output.String()))
	logger.Global.Info("回复注册文件生成完成",
		zap.String("output_path", dst),
		zap.Int("init_func_count", len(initFuncList)),
	)
}

func GenerateServiceConstants(wg *sync.WaitGroup) {
	logger.Global.Info("开始生成服务常量文件",
		zap.String("service_info_dir", _config.Global.Paths.ServiceInfoDir),
	)

	FileServiceMap.Range(func(k, v interface{}) bool {
		protoFile := k.(string)
		serviceList := v.([]*RPCServiceInfo)
		wg.Add(1)
		go func(protoFile string, serviceInfo []*RPCServiceInfo) {
			defer wg.Done()

			if len(serviceInfo) <= 0 {
				logger.Global.Debug("服务信息为空，跳过常量生成",
					zap.String("proto_file", protoFile),
				)
				return
			}

			logger.Global.Debug("开始生成服务常量",
				zap.String("proto_file", protoFile),
				zap.Int("service_count", len(serviceInfo)),
			)

			sort.Slice(serviceInfo, func(i, j int) bool {
				return serviceInfo[i].ServiceIndex < serviceInfo[j].ServiceIndex
			})

			writeServiceIdHeadFile(serviceInfo)

		}(protoFile, serviceList)

		return true
	})

	logger.Global.Info("服务常量文件生成任务已提交",
		zap.String("service_info_dir", _config.Global.Paths.ServiceInfoDir),
	)
}
