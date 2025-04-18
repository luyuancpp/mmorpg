package internal

import (
	"bufio"
	"bytes"
	"fmt"
	"os"
	"pbgen/config"
	"pbgen/util"
	"strings"
	"text/template"
)

// Type definitions for callback functions
type checkRepliedCb func(methods *RPCMethods) bool

// Function to write the header file for service ID
func writeServiceIdHeadFile(methods RPCMethods) {
	defer util.Wg.Done()

	if len(methods) <= 0 {
		return
	}

	firstMethod := methods[0]

	if !firstMethod.CcGenericServices() {
		return
	}

	fileName := methods[0].FileNameNoEx() + config.ServiceInfoExtension + config.HeaderExtension
	util.WriteMd5Data2File(config.ServiceInfoDirectory+fileName, GenServiceIdHeader(methods))
}

// GenServiceIdHeader 使用模板生成服务 ID 头文件内容
func GenServiceIdHeader(methods RPCMethods) string {
	if len(methods) == 0 {
		return ""
	}

	// 创建模板内容
	tmpl := `#pragma once
#include <cstdint>

{{.IncludeName}}

{{range .Methods}}
constexpr uint32_t {{.KeyName}}{{$.MessageIdName}} = {{.Id}};
constexpr uint32_t {{.KeyName}}Index = {{.Index}};
#define {{.KeyName}}Method  ::{{.Service}}_Stub::descriptor()->method({{.Index}})

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
		Methods       RPCMethods
		MessageIdName string
	}{
		IncludeName:   methods[0].IncludeName(),
		Methods:       methods,
		MessageIdName: config.MessageIdName,
	}

	// 执行模板并写入 buffer
	err = t.Execute(&buf, data)
	if err != nil {
		panic(err)
	}

	return buf.String()
}

// 获取 C++ 头文件字符串，使用 text/template
func getServiceHandlerHeadStr(methods RPCMethods) (string, error) {
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
func getServiceHandlerMethodStr(method *RPCMethod) (string, error) {

	const methodTemplate = `
{{.Tab}}void {{.Method}}({{.GoogleMethodController}}
{{.Tab2}}const {{.CppRequest}}* request,
{{.Tab2}}{{.CppResponse}}* response,
{{.Tab2}}::google::protobuf::Closure* done) override;
`

	type MethodData struct {
		Tab                    string
		Tab2                   string
		Method                 string
		GoogleMethodController string
		CppRequest             string
		CppResponse            string
	}
	// 填充模板所需的数据
	data := MethodData{
		Tab:                    config.Tab,
		Tab2:                   config.Tab2,
		Method:                 method.Method(),
		GoogleMethodController: config.GoogleMethodController,
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
func getPlayerMethodHeadStr(methods RPCMethods) (string, error) {

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
			IsEmptyResponse: strings.Contains(method.CppResponse(), config.EmptyResponseName),
		})
	}

	data := PlayerMethodFunctionsData{
		PlayerMethodController: config.PlayerMethodController,
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

class {{.Service}}RepliedHandler : public ::PlayerServiceReplied
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

func getPlayerMethodRepliedHeadStr(methods RPCMethods) (string, error) {
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
		PlayerMethodController: config.PlayerMethodController,
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

func getMethodRepliedHandlerHeadStr(methods *RPCMethods) string {
	const methodRepliedHandlerHeadTemplate = `
#pragma once
{{- .FirstMethodInfo.IncludeName }}
#include "muduo/net/TcpConnection.h"

using namespace muduo;
using namespace muduo::net;

{{- range .Methods }}
void On{{ .KeyName }}{{ $.RepliedHandlerFileName }}(const TcpConnectionPtr& conn, const std::shared_ptr<{{ .CppResponse }}>& replied, Timestamp timestamp);

{{- end }}
`
	type MethodRepliedHandlerData struct {
		FirstMethodInfo        *RPCMethod
		Methods                *RPCMethods
		RepliedHandlerFileName string
	}

	type MethodInfo struct {
		KeyName     string
		CppResponse string
		IncludeName string
	}

	// Ensure there are methods in the list
	if len(*methods) == 0 {
		return ""
	}

	// Ensure there are methods in the list
	if len(*methods) == 0 {
		return ""
	}

	// Prepare data for the template
	var methodsInfo []MethodInfo
	for _, method := range *methods {
		methodsInfo = append(methodsInfo, MethodInfo{
			KeyName:     method.KeyName(),
			CppResponse: method.CppResponse(),
			IncludeName: method.IncludeName(),
		})
	}

	data := MethodRepliedHandlerData{
		FirstMethodInfo:        (*methods)[0],
		Methods:                methods,
		RepliedHandlerFileName: config.RepliedHandlerFileName,
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

	return output.String()
}

// ReadCodeSectionsFromFile 函数接收一个函数作为参数，动态选择 A 或 B 方法
func ReadCodeSectionsFromFile(cppFileName string, methods *RPCMethods, methodFunc func(info *RPCMethod, funcParam string) string, funcParam string) (map[string]string, string, error) {
	// 创建一个 map 来存储每个 RPCMethod 的 name 和对应的 yourCode
	codeMap := make(map[string]string)

	// 打开文件
	fd, err := os.Open(cppFileName)
	if err != nil {
		return nil, "", fmt.Errorf("failed to open file %s: %v", cppFileName, err)
	}
	defer fd.Close()

	// 创建一个扫描器来按行读取文件
	scanner := bufio.NewScanner(fd)

	// 记录当前正在处理的 yourCode
	var currentCode string
	var currentMethod *RPCMethod
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
			// 如果是方法的开始行，检查是否是我们关心的 RPCMethod 名称
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
		firstCode = config.YourCodePair
	}

	// 检查是否有方法没有找到对应的 yourCode，如果没有找到，则添加默认值
	for _, method := range *methods {
		handlerName := methodFunc(method, funcParam)
		if _, exists := codeMap[handlerName]; !exists {
			codeMap[handlerName] = config.YourCodePair
		}
	}

	return codeMap, firstCode, nil
}

func GenerateMethodHandlerNameWrapper(info *RPCMethod, _ string) string {
	return info.Service() + config.HandlerFileName + "::" + info.Method() + "("
}

func GenerateMethodHandlerNameWithClassPrefixWrapper(info *RPCMethod, classPrefix string) string {
	return classPrefix + "::" + info.Method() + "("
}

func GenerateMethodHandlerKeyNameWrapper(info *RPCMethod, _ string) string {
	return "On" + info.KeyName() + config.RepliedHandlerFileName
}

func getMethodHandlerCppStr(dst string, methods *RPCMethods) string {

	const methodHandlerCppTemplate = `
{{ .CppHandlerInclude }}

{{- if .FirstCode }}
{{ .FirstCode }}
{{- end }}

{{- range .Methods }}
{{ if .HasCode }}
void {{ .HandlerName }}{{ $.GoogleMethodController }}const {{ .CppRequest }}* request,
{{ $.Tab }}{{ .CppResponse }}* response,
{{ $.Tab }}::google::protobuf::Closure* done)
{
{{ .Code }}
}
{{ else }}
{{ $.YourCodePair }}
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
		Tab                    string
		Methods                []HandlerMethod
	}

	if len(*methods) == 0 {
		return ""
	}

	ex := ""

	// 获取 yourCode 段落
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methods, GenerateMethodHandlerNameWrapper, ex)

	firstMethodInfo := (*methods)[0]

	var methodList []HandlerMethod
	for _, methodInfo := range *methods {
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
		GoogleMethodController: config.GoogleMethodController,
		FirstCode:              firstCode,
		YourCodePair:           config.YourCodePair,
		Tab:                    config.Tab,
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

func getMethodRepliedHandlerCppStr(dst string, methods *RPCMethods) string {
	const methodRepliedHandlerCppTemplate = `
{{ .CppRepliedHandlerInclude }}#include "network/codec/dispatcher.h"

extern ProtobufDispatcher gResponseDispatcher;

{{ if .FirstCode }}
{{ .FirstCode }}
{{ end }}

void Init{{ .InitFuncName }}{{ .RepliedHandlerFileName }}()
{
{{- range .Methods }}
{{- if .HasCode }}
    gResponseDispatcher.registerMessageCallback<{{ .CppResponse }}>(
        std::bind(&{{ .FuncName }}, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
{{- end }}
{{- end }}
}

{{- range .Methods }}
{{- if .HasCode }}

void {{ .FuncName }}(const TcpConnectionPtr& conn, const std::shared_ptr<{{ .CppResponse }}>& replied, Timestamp timestamp)
{
{{ .Code }}
}
{{- end }}
{{- end }}
`
	type RepliedHandlerMethod struct {
		FuncName    string
		CppResponse string
		Code        string
		HasCode     bool
	}

	type RepliedHandlerCppData struct {
		CppRepliedHandlerInclude string
		FirstCode                string
		InitFuncName             string
		RepliedHandlerFileName   string
		Methods                  []RepliedHandlerMethod
	}

	if len(*methods) == 0 {
		return ""
	}

	emptyString := ""

	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methods, GenerateMethodHandlerKeyNameWrapper, emptyString)
	firstMethodInfo := (*methods)[0]

	var methodsData []RepliedHandlerMethod
	for _, method := range *methods {
		funcName := GenerateMethodHandlerKeyNameWrapper(method, emptyString)
		code, exists := yourCodesMap[funcName]
		methodsData = append(methodsData, RepliedHandlerMethod{
			FuncName:    funcName,
			CppResponse: method.CppResponse(),
			Code:        code,
			HasCode:     exists,
		})
	}

	templateData := RepliedHandlerCppData{
		CppRepliedHandlerInclude: firstMethodInfo.CppRepliedHandlerIncludeName(),
		FirstCode:                firstCode,
		InitFuncName:             firstMethodInfo.KeyName(),
		RepliedHandlerFileName:   config.RepliedHandlerFileName,
		Methods:                  methodsData,
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

func getMethodPlayerHandlerCppStr(dst string, methods *RPCMethods, className string, includeName string) string {
	const playerHandlerCppTemplate = `
{{ .IncludeName }}
{{- if .FirstCode }}
{{ .FirstCode }}
{{ end }}

{{- range .Methods }}
{{- if .HasCode }}
void {{ .HandlerName }}{{ $.PlayerMethodController }}const {{ .CppRequest }}* request,
{{ $.Tab }}{{ .CppResponse }}* response)
{
{{ .Code }}
}
{{ else }}
{{ $.YourCodePair }}
{{ end }}

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
		Tab                    string
		YourCodePair           string
		Methods                []PlayerHandlerMethod
	}

	if len(*methods) == 0 {
		return includeName // Still return include if empty
	}

	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, methods, GenerateMethodHandlerNameWithClassPrefixWrapper, className)

	var methodList []PlayerHandlerMethod
	for _, method := range *methods {
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
		PlayerMethodController: config.PlayerMethodController,
		Tab:                    config.Tab,
		YourCodePair:           config.YourCodePair,
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

func GenRegisterFile(dst string, cb checkRepliedCb) {

	const registerFileTemplate = `
{{- range .Includes }}
{{ . }}
{{ end }}

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

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

	defer util.Wg.Done()

	ServiceList := GetSortServiceList()

	var includes []string
	var initLines []string

	for _, key := range ServiceList {
		methods, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !cb(&methods) {
			continue
		}

		first := methods[0]
		includes = append(includes, first.CppHandlerIncludeName())
		initLines = append(initLines, fmt.Sprintf("%sgNodeService.emplace(\"%s\", std::make_unique_for_overwrite<%s%s>());",
			config.Tab, first.Service(), first.Service(), config.HandlerFileName))
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

	util.WriteMd5Data2File(dst, output.String())
}

func writeRepliedRegisterFile(dst string, cb checkRepliedCb) {
	defer util.Wg.Done()

	var data strings.Builder

	// Start the function definition for initializing replied handlers
	data.WriteString("void InitRepliedHandler()\n{\n")

	ServiceList := GetSortServiceList()
	for _, key := range ServiceList {
		methods, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !cb(&methods) {
			continue
		}
		firstMethodInfo := methods[0]

		// Append the initialization function declaration
		initFunctionName := "Init" + firstMethodInfo.KeyName() + config.RepliedHandlerFileName
		data.WriteString(config.Tab + "void " + initFunctionName + "();\n")

		// Call the initialization function
		data.WriteString(config.Tab + initFunctionName + "();\n\n")
	}

	// End the function definition
	data.WriteString("}\n")

	// Write the generated data to the destination file using util.WriteMd5Data2File
	util.WriteMd5Data2File(dst, data.String())
}

func WriteMethodFile() {
	for _, v := range ServiceMethodMap {
		// Start concurrent operations for each service method
		util.Wg.Add(1)
		go writeServiceIdHeadFile(v)

		// gs methods
		util.Wg.Add(1)
		go writeGsMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGsMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsMethodRepliedHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGsPlayerMethodRepliedHandlerCppFile(v)

		// centre methods
		util.Wg.Add(1)
		go writeCentreMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentreMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeCentreMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentreMethodRepliedHandlerCppFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeCentrePlayerMethodRepliedHandlerCppFile(v)

		// gate methods
		util.Wg.Add(1)
		go writeGateMethodHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodHandlerCppFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerHeadFile(v)
		util.Wg.Add(1)
		go writeGateMethodRepliedHandlerCppFile(v)
	}

	// Concurrent operations for game, centre, and gate registers
	util.Wg.Add(1)
	go GenRegisterFile(config.GameNodeMethodHandlerDirectory+config.RegisterHandlerCppExtension, IsGsMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GameNodeMethodRepliedHandlerDirectory+config.RegisterRepliedHandlerCppExtension, isGsMethodRepliedHandler)

	util.Wg.Add(1)
	go GenRegisterFile(config.CentreNodeMethodHandlerDirectory+config.RegisterHandlerCppExtension, isCentreMethodHandler)
	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.CentreMethodRepliedHandleDir+config.RegisterRepliedHandlerCppExtension, isCentreMethodRepliedHandler)

	util.Wg.Add(1)
	go writeRepliedRegisterFile(config.GateMethodRepliedHandlerDirectory+config.RegisterRepliedHandlerCppExtension, isGateMethodRepliedHandler)
}
