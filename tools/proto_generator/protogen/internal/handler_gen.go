package internal

import (
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

	return utils.GlobalEngine.MustExecute("serviceHandlerMethod", tmplStr, data)
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

// GenerateMethodHandlerNameWrapper generates the method handler name.
func GenerateMethodHandlerNameWrapper(info *MethodInfo, _ string) string {
	return info.Service() + _config.Global.Naming.HandlerFile + "::" + info.Method() + "("
}

// GenerateMethodHandlerNameWithClassPrefixWrapper generates the handler name with class prefix.
func GenerateMethodHandlerNameWithClassPrefixWrapper(info *MethodInfo, classPrefix string) string {
	return classPrefix + "::" + info.Method() + "("
}
