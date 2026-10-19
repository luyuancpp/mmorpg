package internal

import (
	"fmt"
	"path/filepath"
	"strings"
	"text/template"

	_config "protogen/internal/config"
	"protogen/internal/utils"
)

// ServiceHandlerData holds data for generating a service handler header.
type ServiceHandlerData struct {
	Include string
	Service string
	Methods RPCMethods
}

// GetServiceHandlerHeadStr generates the service handler header file.
func GetServiceHandlerHeadStr(methods RPCMethods) (string, error) {
	// 类级注释只放头文件:cpp 由守护段解析器重建,额外的注释会干扰守护段定位。
	const tmplStr = `#pragma once
{{.Include}}

// done 由框架传入且非空:不碰它,handler 返回后框架立即回包;要等下游再回复,在返回前 RpcDoneCallback::Adopt(done)(network/rpc_done_callback.h)。
class {{.Service}}Handler : public ::{{.Service}}
{
public:
{{range .Methods}}
{{getServiceHandlerMethodStr .}}
{{end}}
};`

	if len(methods) == 0 {
		return "", nil
	}

	data := ServiceHandlerData{
		Include: methods[0].IncludeName(),
		Service: methods[0].Service(),
		Methods: methods,
	}

	funcs := template.FuncMap{
		"getServiceHandlerMethodStr": getServiceHandlerMethodStr,
	}

	return utils.GlobalEngine.Render("serviceHandlerHead", tmplStr, data, funcs)
}

// getServiceHandlerMethodStr generates a single method signature.
func getServiceHandlerMethodStr(method *MethodInfo) (string, error) {
	const tmplStr = `
	void {{.Method}}({{.GoogleMethodController}} const {{.CppRequest}}* request, {{.CppResponse}}* response, ::google::protobuf::Closure* done) override;
`

	data := struct {
		Method                 string
		GoogleMethodController string
		CppRequest             string
		CppResponse            string
	}{
		Method:                 method.Method(),
		GoogleMethodController: _config.Global.Naming.GoogleMethodController,
		CppRequest:             method.CppRequest(),
		CppResponse:            method.CppResponse(),
	}

	return utils.GlobalEngine.Render("serviceHandlerMethod", tmplStr, data)
}

// HandlerMethod holds data for a handler method.
type HandlerMethod struct {
	HandlerName string
	CppRequest  string
	CppResponse string
	Code        string
	HasCode     bool
}

// HandlerCppData holds data for generating a CPP handler file.
type HandlerCppData struct {
	CppHandlerInclude      string
	GoogleMethodController string
	FirstCode              string
	YourCodePair           string
	Methods                []HandlerMethod
}

// GetServiceHandlerCppStr generates the service handler CPP file content.
func GetServiceHandlerCppStr(dst string, methods RPCMethods, className string, includeName string) string {
	const tmplStr = `
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
{{ .Code }}
}
{{ else }}
{{- $.YourCodePair -}}
{{ end }}
{{ end }}
`

	if len(methods) == 0 {
		return ""
	}

	emptyStr := ""
	// 结构损坏(未闭合 / 嵌套 / 孤立的结束标记)、带不走的非空段(签名不对应当前方法的函数里的段、方法体里第一个段之后的段、
	// FirstCode 之后函数外的段)一律拒绝生成,与 gRPC 生成器同口径;字符串接口的调用方只在返回后才写文件。
	yourCodesMap, firstCode, err := readCheckedCodeSections(dst, &methods, GenerateMethodHandlerNameWrapper, emptyStr)
	if err != nil {
		panic(err)
	}

	firstMethodInfo := methods[0]

	var methodList []HandlerMethod
	for _, methodInfo := range methods {
		handlerName := GenerateMethodHandlerNameWrapper(methodInfo, emptyStr)
		code, exists := yourCodesMap[handlerName]
		methodList = append(methodList, HandlerMethod{
			HandlerName: handlerName,
			CppRequest:  methodInfo.CppRequest(),
			CppResponse: methodInfo.CppResponse(),
			Code:        code,
			HasCode:     exists,
		})
	}

	data := HandlerCppData{
		CppHandlerInclude:      firstMethodInfo.CppHandlerIncludeName(),
		GoogleMethodController: _config.Global.Naming.GoogleMethodController,
		FirstCode:              firstCode,
		YourCodePair:           _config.Global.Naming.YourCodePair,
		Methods:                methodList,
	}

	return utils.RenderTemplateOrExit("serviceHandlerCpp", tmplStr, data)
}

// GetServiceHandlerHeadFromFileStr:头文件本身不含守护段,但 .h 与 .cpp 由通用管线分别生成、互不等待(handlers.go / handler.go)——
// 先对同名 .cpp 做与 GetServiceHandlerCppStr 相同的取段预检,出错时 .h 也不写,不留下「新 .h 配旧 .cpp」(与 gRPC 生成器同口径)。
func GetServiceHandlerHeadFromFileStr(dst string, methods RPCMethods) (string, error) {
	if len(methods) > 0 {
		if err := precheckHandlerCppForHeader(dst, methods, GenerateMethodHandlerNameWrapper, ""); err != nil {
			return "", err
		}
	}
	return GetServiceHandlerHeadStr(methods)
}

// precheckHandlerCppForHeader 对头文件 headerPath 的同名 .cpp(与 handler.go 写 .cpp 的路径同式:文件名转小写、换扩展名)
// 做取段预检。推不出 .cpp 路径就报错 —— 预检做不了就不生成,不能默默跳过。
func precheckHandlerCppForHeader(headerPath string, methods RPCMethods, methodFunc MethodNameFunc, funcParam string) error {
	headerExt := _config.Global.FileExtensions.HandlerH
	cppExt := _config.Global.FileExtensions.HandlerCpp
	if headerExt == "" || cppExt == "" || !strings.HasSuffix(headerPath, headerExt) {
		return fmt.Errorf("%s：无法推出同名 handler cpp(handler_h=%q handler_cpp=%q)，拒绝在未预检守护段时生成头文件",
			headerPath, headerExt, cppExt)
	}
	dir, base := filepath.Split(strings.TrimSuffix(headerPath, headerExt))
	_, _, err := readCheckedCodeSections(dir+strings.ToLower(base)+cppExt, &methods, methodFunc, funcParam)
	return err
}

// GenerateMethodHandlerNameWrapper generates the method handler name.
func GenerateMethodHandlerNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + _config.Global.Naming.HandlerFile + "::" + info.Method() + "("
}

// GenerateMethodHandlerNameWithClassPrefixWrapper generates the handler name with class prefix.
func GenerateMethodHandlerNameWithClassPrefixWrapper(info *MethodInfo, classPrefix string) string {
	return classPrefix + "::" + info.Method() + "("
}
