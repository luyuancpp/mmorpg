package internal

import (
	"bufio"
	"bytes"
	"fmt"
	"log"
	"os"
	"pbgen/config"
	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"sort"
	"strings"
	"sync"
	"text/template"
)

// Type definitions for callback functions
type checkRepliedCb func(methods *RPCMethods) bool

// Function to write the header file for service ID
func writeServiceIdHeadFile(serviceInfo []*RPCServiceInfo) {

	if len(serviceInfo) <= 0 {
		return
	}

	fileName := serviceInfo[0].ServiceInfoHeadInclude()
	utils2.WriteFileIfChanged(_config.Global.Paths.ServiceInfoDir+fileName, []byte(GenServiceIdHeader(serviceInfo)))
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
		panic(err) // 可以根据需求调整错误处理
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
		panic(err)
	}

	return buf.String()
}

// 获取 C++ 头文件字符串，使用 text/template
func GetServiceHandlerHeadStr(methods RPCMethods) (string, error) {
	// 定义 C++ 类头文件的模板
	const tmplStr = `#pragma once
{{.Include}}

class {{.Service}}Handler : public ::{{.Service}}
{
public:
{{range .Methods}}
{{getServiceHandlerMethodStr .}}
{{end}}
};`

	// 创建一个模板对象
	tmpl, err := template.New("header").Funcs(template.FuncMap{
		"getServiceHandlerMethodStr": getServiceHandlerMethodStr,
	}).Parse(tmplStr)
	if err != nil {
		return "", err
	}

	// 定义模板的数据
	data := struct {
		Include string
		Service string
		Methods RPCMethods
	}{
		Include: methods[0].IncludeName(), // 假设所有方法使用相同的 Include
		Service: methods[0].Service(),     // 假设所有方法属于同一个服务
		Methods: methods,
	}

	// 使用模板填充数据
	var result strings.Builder
	err = tmpl.Execute(&result, data)
	if err != nil {
		return "", err
	}

	// 返回生成的 C++ 头文件
	return result.String(), nil
}

// Helper function to generate method strings for service handlers
func getServiceHandlerMethodStr(method *MethodInfo) (string, error) {

	const methodTemplate = `
	void {{.Method}}({{.GoogleMethodController}} const {{.CppRequest}}* request, {{.CppResponse}}* response, ::google::protobuf::Closure* done) override;
`

	type MethodData struct {
		Method                 string
		GoogleMethodController string
		CppRequest             string
		CppResponse            string
	}
	// 填充模板所需的数据
	data := MethodData{
		Method:                 method.Method(),
		GoogleMethodController: _config.Global.Naming.GoogleMethodController,
		CppRequest:             method.CppRequest(),
		CppResponse:            method.CppResponse(),
	}

	// 创建模板
	tmpl, err := template.New("methodTemplate").Parse(methodTemplate)
	if err != nil {
		return "", err
	}

	// 使用 bytes.Buffer 来捕获模板输出
	var output bytes.Buffer
	err = tmpl.Execute(&output, data)
	if err != nil {
		return "", err
	}

	return output.String(), nil
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
		PlayerServiceIncludeName: config.PlayerServiceIncludeName,
		MacroReturnIncludeName:   config.MacroReturnIncludeName,
		Service:                  methods[0].Service(),
		MethodHandlerFunctions:   getPlayerMethodHandlerFunctions(methods),
	}

	// 创建模板
	tmpl, err := template.New("playerMethodHeadTemplate").Parse(playerMethodHeadTemplate)
	if err != nil {
		return "", err
	}

	// 使用 bytes.Buffer 来捕获模板输出
	var output bytes.Buffer
	err = tmpl.Execute(&output, data)
	if err != nil {
		return "", err
	}

	return output.String(), nil
}

const playerMethodFunctionsTemplate = `
{{- range .Methods }}
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
                static_cast<const {{ $method.CppRequest }}*>(request),
                static_cast<{{ $method.CppResponse }}*>(response));
{{- if not $method.IsEmptyResponse }}
            TRANSFER_ERROR_MESSAGE(static_cast<{{ $method.CppResponse }}*>(response));
{{- end }}
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
		panic(err) // 可以改为返回 error
	}

	var output bytes.Buffer
	err = tmpl.Execute(&output, data)
	if err != nil {
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
		PlayerServiceRepliedIncludeName: config.PlayerServiceRepliedIncludeName,
		Service:                         methods[0].Service(),
		MethodHandlerFunctions:          getPlayerMethodRepliedHandlerFunctions(methods),
	}

	// 创建模板
	tmpl, err := template.New("playerMethodRepliedHeadTemplate").Parse(playerMethodRepliedHeadTemplate)
	if err != nil {
		return "", err
	}

	// 使用 bytes.Buffer 来捕获模板输出
	var output bytes.Buffer
	err = tmpl.Execute(&output, data)
	if err != nil {
		return "", err
	}

	return output.String(), nil
}

const playerMethodRepliedFunctionsTemplate = `
{{- range .Methods }}
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
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, data); err != nil {
		panic(err)
	}

	return output.String()
}

func GetServiceRepliedHandlerHeadStr(methods RPCMethods) (string, error) {
	const methodRepliedHandlerHeadTemplate = `#pragma once
#include "muduo/net/TcpConnection.h"
{{.FirstMethodInfo.IncludeName }}
using namespace muduo;
using namespace muduo::net;

{{- range .Methods }}
void On{{ .KeyName }}{{ $.RepliedHandlerFileName }}(const TcpConnectionPtr& conn, const std::shared_ptr<{{ .CppResponse }}>& replied, Timestamp timestamp);

{{- end }}
`
	type MethodRepliedHandlerData struct {
		FirstMethodInfo        *MethodInfo
		Methods                RPCMethods
		RepliedHandlerFileName string
	}

	type MethodInfo struct {
		KeyName     string
		CppResponse string
		IncludeName string
	}

	// Ensure there are methods in the list
	if len(methods) == 0 {
		return "", nil
	}

	// Ensure there are methods in the list
	if len(methods) == 0 {
		return "", nil
	}

	// Prepare data for the template
	var methodsInfo []MethodInfo
	for _, method := range methods {
		methodsInfo = append(methodsInfo, MethodInfo{
			KeyName:     method.KeyName(),
			CppResponse: method.CppResponse(),
			IncludeName: method.IncludeName(),
		})
	}

	data := MethodRepliedHandlerData{
		FirstMethodInfo:        (methods)[0],
		Methods:                methods,
		RepliedHandlerFileName: _config.Global.Naming.RepliedHandlerFile,
	}

	// Parse and execute the template
	tmpl, err := template.New("methodRepliedHandlerHead").Parse(methodRepliedHandlerHeadTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, data); err != nil {
		panic(err)
	}

	return output.String(), nil
}

// ReadCodeSectionsFromFile 函数接收一个函数作为参数，动态选择 A 或 B 方法
func ReadCodeSectionsFromFile(cppFileName string, methods *RPCMethods, methodFunc func(info *MethodInfo, funcParam string) string, funcParam string) (map[string]string, string, error) {
	// 创建一个 map 来存储每个 MethodInfo 的 name 和对应的 yourCode
	codeMap := make(map[string]string)

	// 打开文件
	fd, err := os.Open(cppFileName)
	if err != nil {
		log.Fatalf("failed to open file %s: %v", cppFileName, err)
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

	// 遍历文件的每一行
	for scanner.Scan() {
		line := scanner.Text() + "\n"

		// 如果正在处理第一个 yourCode，并且发现 YourCodeEnd
		if inFirstCode && strings.Contains(line, config.YourCodeEnd) {
			firstCode += line
			inFirstCode = false
			continue // 跳过其他处理，继续后续的代码处理
		} else if inFirstCode {
			firstCode += line
		}

		// 如果是第一个特殊的 yourCode块
		if !isFirstCode && strings.Contains(line, config.YourCodeBegin) {
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
					break
				}
			}
		}

		// 如果找到了当前方法的开始，接着读取直到找到结束
		if currentMethod != nil {
			if strings.Contains(line, config.YourCodeBegin) {
				inMethodCode = true
				currentCode += line
			} else if strings.Contains(line, config.YourCodeEnd) {
				currentCode += line
				// 使用 methodFunc currentMethod
				handlerName := methodFunc(currentMethod, funcParam)
				codeMap[handlerName] = currentCode
				currentMethod = nil
				currentCode = ""
				inMethodCode = false
			} else if inMethodCode {
				currentCode += line
			}
		}
	}

	// 如果没有找到第一个 yourCode，使用默认的 config.YourCodePair
	if firstCode == "" {
		firstCode = _config.Global.Naming.YourCodePair
	}

	// 检查是否有方法没有找到对应的 yourCode，如果没有找到，则添加默认值
	for _, method := range *methods {
		handlerName := methodFunc(method, funcParam)
		if _, exists := codeMap[handlerName]; !exists {
			codeMap[handlerName] = _config.Global.Naming.YourCodePair
		}
	}

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

	const methodHandlerCppTemplate = `
{{ .CppHandlerInclude }}

{{- if .FirstCode }}
{{ .FirstCode }}
{{- end }}

{{- range .Methods }}
{{ if .HasCode }}
void {{ .HandlerName }}{{ $.GoogleMethodController }}const {{ .CppRequest }}* request,
	{{ .CppResponse }}* response,
	::google::protobuf::Closure* done)
{
{{ .Code -}}
}
{{ else }}
{{- $.YourCodePair -}}
{{ end }}
{{ end }}
`

	type HandlerMethod struct {
		HandlerName string
		CppRequest  string
		CppResponse string
		Code        string
		HasCode     bool
	}

	type HandlerCppData struct {
		CppHandlerInclude      string
		GoogleMethodController string
		FirstCode              string
		YourCodePair           string
		Methods                []HandlerMethod
	}

	if len(methods) == 0 {
		return ""
	}

	ex := ""

	// 获取 yourCode 段落
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateMethodHandlerNameWrapper, ex)

	firstMethodInfo := (methods)[0]

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

	// 填充模板数据
	data := HandlerCppData{
		CppHandlerInclude:      firstMethodInfo.CppHandlerIncludeName(),
		GoogleMethodController: _config.Global.Naming.GoogleMethodController,
		FirstCode:              firstCode,
		YourCodePair:           _config.Global.Naming.YourCodePair,
		Methods:                methodList,
	}

	// 执行模板
	tmpl, err := template.New("methodHandlerCpp").Parse(methodHandlerCppTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, data); err != nil {
		panic(err)
	}

	return output.String()
}

func GetServiceRepliedHandlerCppStr(dst string, methods RPCMethods, _ string, _ string) string {
	const methodRepliedHandlerCppTemplate = `
{{ .CppRepliedHandlerInclude }}
#include "rpc/{{ .ServiceInfoName }}{{ .ServiceInfoHeadInclude }}"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

{{ if .FirstCode }}
{{ .FirstCode }}
{{ end }}

void Init{{ .InitFuncName }}{{ .RepliedHandlerFileName }}()
{
{{- range .Methods }}
{{- if .HasCode }}
    gRpcResponseDispatcher.registerMessageCallback<{{ .CppResponse }}>({{.KeyName}}{{$.MessageIdName}},
        std::bind(&{{ .FuncName }}, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
{{- end }}
{{- end }}
}

{{- range .Methods }}
{{- if .HasCode }}

void {{ .FuncName }}(const TcpConnectionPtr& conn, const std::shared_ptr<{{ .CppResponse }}>& replied, Timestamp timestamp)
{
{{ .Code -}}
}
{{- end }}
{{- end }}
`
	type RepliedHandlerMethod struct {
		FuncName    string
		CppResponse string
		Code        string
		HasCode     bool
		KeyName     string
	}

	type RepliedHandlerCppData struct {
		CppRepliedHandlerInclude string
		FirstCode                string
		InitFuncName             string
		RepliedHandlerFileName   string
		Methods                  []RepliedHandlerMethod
		MessageIdName            string
		ServiceInfoHeadInclude   string
		ServiceInfoName          string
	}

	if len(methods) == 0 {
		return ""
	}

	emptyString := ""

	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateMethodHandlerKeyNameWrapper, emptyString)
	firstMethodInfo := (methods)[0]

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

	templateData := RepliedHandlerCppData{
		CppRepliedHandlerInclude: firstMethodInfo.CppRepliedHandlerIncludeName(),
		FirstCode:                firstCode,
		InitFuncName:             firstMethodInfo.Service(),
		RepliedHandlerFileName:   _config.Global.Naming.RepliedHandlerFile,
		Methods:                  methodsData,
		MessageIdName:            _config.Global.Naming.MessageId,
		ServiceInfoHeadInclude:   firstMethodInfo.ServiceInfoHeadInclude(),
		ServiceInfoName:          _config.Global.FileExtensions.ServiceInfoName,
	}

	tmpl, err := template.New("methodRepliedHandlerCpp").Parse(methodRepliedHandlerCppTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, templateData); err != nil {
		panic(err)
	}

	return output.String()
}

func GetPlayerServiceHandlerCppStr(dst string, methods RPCMethods, className string, includeName string) string {
	const playerHandlerCppTemplate = `
{{ .IncludeName }}
{{- if .FirstCode }}
{{ .FirstCode }}
{{ end }}

{{- range .Methods }}

void {{ .HandlerName }}{{ $.PlayerMethodController }}const {{ .CppRequest }}* request,
	{{ .CppResponse }}* response)
{
{{- if .HasCode }}
{{ .Code -}}
{{ else }}
{{ $.YourCodePair }}
{{ end }}
}
{{ end }}
`
	type PlayerHandlerMethod struct {
		HandlerName string
		CppRequest  string
		CppResponse string
		Code        string
		HasCode     bool
	}

	type PlayerHandlerCppData struct {
		IncludeName            string
		FirstCode              string
		PlayerMethodController string
		YourCodePair           string
		Methods                []PlayerHandlerMethod
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

	data := PlayerHandlerCppData{
		IncludeName:            includeName,
		FirstCode:              firstCode,
		PlayerMethodController: _config.Global.Naming.PlayerMethodController,
		YourCodePair:           _config.Global.Naming.YourCodePair,
		Methods:                methodList,
	}

	tmpl, err := template.New("playerHandlerCpp").Parse(playerHandlerCppTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, data); err != nil {
		panic(err)
	}

	return output.String()
}

func GenRegisterFile(wg *sync.WaitGroup, dst string, cb checkRepliedCb) {

	const registerFileTemplate = `
#include <unordered_map>
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

	for _, service := range GlobalRPCServiceList {
		if !cb(&service.MethodInfo) {
			continue
		}
		first := service.MethodInfo[0]
		includes = append(includes, first.CppHandlerIncludeName())
		initLines = append(initLines, fmt.Sprintf(" gNodeService.emplace(\"%s\", std::make_unique_for_overwrite<%s%s>());",
			first.Service(), first.Service(), _config.Global.Naming.HandlerFile))
	}

	templateData := RegisterFileData{
		Includes:  includes,
		InitLines: initLines,
	}

	tmpl, err := template.New("registerFile").Parse(registerFileTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, templateData); err != nil {
		panic(err)
	}

	utils2.WriteFileIfChanged(dst, output.Bytes())
}

func WriteRepliedRegisterFile(wg *sync.WaitGroup, dst string, cb checkRepliedCb) {
	const repliedRegisterTemplate = `
void InitReply()
{
{{- range .InitFuncs }}
    void {{ . }}();
    {{ . }}();

{{- end }}
}
`
	type RepliedRegisterData struct {
		InitFuncs []string
	}

	defer wg.Done()

	var initFuncList []string

	for _, service := range GlobalRPCServiceList {
		if !cb(&service.MethodInfo) {
			continue
		}
		first := service.MethodInfo[0]
		initFuncList = append(initFuncList, "Init"+first.Service()+_config.Global.Naming.RepliedHandlerFile)
	}

	templateData := RepliedRegisterData{
		InitFuncs: initFuncList,
	}

	tmpl, err := template.New("repliedRegister").Parse(repliedRegisterTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, templateData); err != nil {
		panic(err)
	}

	utils2.WriteFileIfChanged(dst, output.Bytes())
}

func GenerateServiceConstants(wg *sync.WaitGroup) {
	FileServiceMap.Range(func(k, v interface{}) bool {
		protoFile := k.(string)
		serviceList := v.([]*RPCServiceInfo)
		wg.Add(1)
		go func(protoFile string, serviceInfo []*RPCServiceInfo) {
			defer wg.Done()

			if len(serviceInfo) <= 0 {
				return
			}

			sort.Slice(serviceInfo, func(i, j int) bool {
				return serviceInfo[i].ServiceIndex < serviceInfo[j].ServiceIndex
			})

			writeServiceIdHeadFile(serviceInfo)

		}(protoFile, serviceList)

		return true
	})
}
