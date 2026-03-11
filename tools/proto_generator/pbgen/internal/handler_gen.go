package internal

import (
	"text/template"

	_config "pbgen/internal/config"
	"pbgen/internal/utils"
)

// ServiceHandlerData 服务处理器头文件数据
type ServiceHandlerData struct {
	Include string
	Service string
	Methods RPCMethods
}

// GetServiceHandlerHeadStr 生成服务处理器头文件
func GetServiceHandlerHeadStr(methods RPCMethods) (string, error) {
	const tmplStr = `#pragma once
{{.Include}}

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

	return utils.GlobalEngine.MustExecute("serviceHandlerHead", tmplStr, data, funcs)
}

// getServiceHandlerMethodStr 生成单个方法签名
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

	return utils.GlobalEngine.MustExecute("serviceHandlerMethod", tmplStr, data)
}

// HandlerMethod 处理器方法数据
type HandlerMethod struct {
	HandlerName string
	CppRequest  string
	CppResponse string
	Code        string
	HasCode     bool
}

// HandlerCppData CPP处理器文件数据
type HandlerCppData struct {
	CppHandlerInclude      string
	GoogleMethodController string
	FirstCode              string
	YourCodePair           string
	Methods                []HandlerMethod
}

// GetServiceHandlerCppStr 生成服务处理器CPP文件内容
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
{{ .Code -}}
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
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateMethodHandlerNameWrapper, emptyStr)

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

	return utils.ExecuteTemplate("serviceHandlerCpp", tmplStr, data)
}

// GenerateMethodHandlerNameWrapper 生成方法处理器名称
func GenerateMethodHandlerNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + _config.Global.Naming.HandlerFile + "::" + info.Method() + "("
}

// GenerateMethodHandlerNameWithClassPrefixWrapper 生成带类前缀的方法处理器名称
func GenerateMethodHandlerNameWithClassPrefixWrapper(info *MethodInfo, classPrefix string) string {
	return classPrefix + "::" + info.Method() + "("
}
