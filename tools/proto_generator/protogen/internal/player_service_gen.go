package internal

import (
	_config "protogen/internal/config"
	"protogen/internal/utils"
)

// PlayerMethod holds data for a player service method.
type PlayerMethod struct {
	Method           string
	CppRequest       string
	CppResponse      string
	HasErrorResponse bool
}

// PlayerMethodFunctionsData holds data for generating player method functions.
type PlayerMethodFunctionsData struct {
	PlayerMethodController string
	Methods                []PlayerMethod
}

// GetPlayerServiceHeadStr generates the player service header file.
func GetPlayerServiceHeadStr(methods RPCMethods) (string, error) {
	const tmplStr = `#pragma once

#include <type_traits>
#include <utility>

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

	if len(methods) == 0 {
		return "", nil
	}

	data := struct {
		IncludeName              string
		PlayerServiceIncludeName string
		MacroReturnIncludeName   string
		Service                  string
		MethodHandlerFunctions   string
	}{
		IncludeName:              methods[0].IncludeName(),
		PlayerServiceIncludeName: _config.Global.Naming.PlayerServiceIncludeName,
		MacroReturnIncludeName:   _config.Global.Naming.MacroReturnIncludeName,
		Service:                  methods[0].Service(),
		MethodHandlerFunctions:   getPlayerMethodHandlerFunctions(methods),
	}

	return utils.GlobalEngine.Render("playerServiceHead", tmplStr, data)
}

// GetPlayerServiceHeadFromFileStr:先对同名 .cpp 做与 GetPlayerServiceHandlerCppStr 相同的取段预检(类名前缀与 handler.go
// 传给 .cpp 生成的 CppHandlerClassName 一致),出错时 .h 也不写,理由见 GetServiceHandlerHeadFromFileStr。
func GetPlayerServiceHeadFromFileStr(dst string, methods RPCMethods) (string, error) {
	if len(methods) > 0 {
		if err := precheckHandlerCppForHeader(dst, methods, GenerateMethodHandlerNameWithClassPrefixWrapper,
			methods[0].CppHandlerClassName()); err != nil {
			return "", err
		}
	}
	return GetPlayerServiceHeadStr(methods)
}

// getPlayerMethodHandlerFunctions generates player method handler function declarations.
func getPlayerMethodHandlerFunctions(methods RPCMethods) string {
	const tmplStr = `
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
			{
{{- if $method.HasErrorResponse }}
            static_assert(std::is_same_v<decltype(std::declval<{{ $method.CppResponse }}&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
{{- end }}
            {{ $method.Method }}(player,
                static_cast<const {{ $method.CppRequest }}*>(request),
                static_cast<{{ $method.CppResponse }}*>(response));
			}
            break;
{{- end }}
        default:
            break;
        }
    }
`

	var methodList []PlayerMethod
	for _, method := range methods {
		cppResponse := method.CppResponse()
		// 标准 Empty 与配置的本地 Empty 都没有错误字段；业务包里的同名类型仍须满足契约。
		hasErrorResponse := cppResponse != "::google::protobuf::Empty" &&
			cppResponse != "::"+_config.Global.Naming.EmptyResponse
		methodList = append(methodList, PlayerMethod{
			Method:           method.Method(),
			CppRequest:       method.CppRequest(),
			CppResponse:      cppResponse,
			HasErrorResponse: hasErrorResponse,
		})
	}

	data := PlayerMethodFunctionsData{
		PlayerMethodController: _config.Global.Naming.PlayerMethodController,
		Methods:                methodList,
	}

	return utils.RenderTemplateOrExit("playerMethodFunctions", tmplStr, data)
}

// PlayerHandlerMethod represents a player handler method.
type PlayerHandlerMethod struct {
	HandlerName string
	CppRequest  string
	CppResponse string
	Code        string
	HasCode     bool
}

// GetPlayerServiceHandlerCppStr generates the player service handler CPP file.
func GetPlayerServiceHandlerCppStr(dst string, methods RPCMethods, className string, includeName string) string {
	const tmplStr = `
{{ .IncludeName }}
{{- if .FirstCode }}
{{ .FirstCode }}
{{ end }}

{{- range $index, $method := .Methods }}
{{- if gt $index 0 }}

{{- end }}

void {{ $method.HandlerName }}{{ $.PlayerMethodController }}const {{ $method.CppRequest }}* request,
	{{ $method.CppResponse }}* response)
{
{{- if $method.HasCode }}
{{ $method.Code -}}
{{ else }}
{{ $.YourCodePair }}
{{ end }}
}
{{ end }}
`

	if len(methods) == 0 {
		return includeName
	}

	// 与 GetServiceHandlerCppStr 同一预检:结构损坏或带不走的非空段一律拒绝生成;字符串接口的调用方只在返回后才写文件。
	yourCodesMap, firstCode, err := readCheckedCodeSections(dst, &methods, GenerateMethodHandlerNameWithClassPrefixWrapper, className)
	if err != nil {
		panic(err)
	}

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

	data := struct {
		IncludeName            string
		FirstCode              string
		PlayerMethodController string
		YourCodePair           string
		Methods                []PlayerHandlerMethod
	}{
		IncludeName:            includeName,
		FirstCode:              firstCode,
		PlayerMethodController: _config.Global.Naming.PlayerMethodController,
		YourCodePair:           _config.Global.Naming.YourCodePair,
		Methods:                methodList,
	}

	return utils.RenderTemplateOrExit("playerHandlerCpp", tmplStr, data)
}
