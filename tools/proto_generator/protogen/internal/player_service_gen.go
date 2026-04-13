package internal

import (
	"strings"

	_config "protogen/internal/config"
	"protogen/internal/utils"
)

// PlayerMethod holds data for a player service method.
type PlayerMethod struct {
	Method          string
	CppRequest      string
	CppResponse     string
	IsEmptyResponse bool
}

// PlayerMethodFunctionsData holds data for generating player method functions.
type PlayerMethodFunctionsData struct {
	PlayerMethodController string
	Methods                []PlayerMethod
}

// GetPlayerServiceHeadStr generates the player service header file.
func GetPlayerServiceHeadStr(methods RPCMethods) (string, error) {
	const tmplStr = `#pragma once

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

	return utils.GlobalEngine.MustExecute("playerServiceHead", tmplStr, data)
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

	return utils.ExecuteTemplate("playerMethodFunctions", tmplStr, data)
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

	return utils.ExecuteTemplate("playerHandlerCpp", tmplStr, data)
}
