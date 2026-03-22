package internal

import (
	_config "protogen/internal/config"
	"protogen/internal/utils"
)

// RepliedMethod 回复方法数据
type RepliedMethod struct {
	Method      string
	CppRequest  string
	CppResponse string
}

// GetPlayerMethodRepliedHeadStr 生成玩家回复方法头文件
func GetPlayerMethodRepliedHeadStr(methods RPCMethods) (string, error) {
	const tmplStr = `
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

	if len(methods) == 0 {
		return "", nil
	}

	data := struct {
		IncludeName                     string
		PlayerServiceRepliedIncludeName string
		Service                         string
		MethodHandlerFunctions          string
	}{
		IncludeName:                     methods[0].IncludeName(),
		PlayerServiceRepliedIncludeName: _config.Global.Naming.PlayerServiceRepliedIncludeName,
		Service:                         methods[0].Service(),
		MethodHandlerFunctions:          getPlayerMethodRepliedHandlerFunctions(methods),
	}

	return utils.GlobalEngine.MustExecute("playerMethodRepliedHead", tmplStr, data)
}

// getPlayerMethodRepliedHandlerFunctions 生成玩家回复处理函数
func getPlayerMethodRepliedHandlerFunctions(methods RPCMethods) string {
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

	var methodList []RepliedMethod
	for _, method := range methods {
		methodList = append(methodList, RepliedMethod{
			Method:      method.Method(),
			CppRequest:  method.CppRequest(),
			CppResponse: method.CppResponse(),
		})
	}

	data := struct {
		PlayerMethodController string
		Methods                []RepliedMethod
	}{
		PlayerMethodController: _config.Global.Naming.PlayerMethodController,
		Methods:                methodList,
	}

	return utils.ExecuteTemplate("playerRepliedFunctions", tmplStr, data)
}

// RepliedHandlerMethod 回复处理器方法
type RepliedHandlerMethod struct {
	FuncName    string
	CppResponse string
	Code        string
	HasCode     bool
	KeyName     string
}

// GetServiceRepliedHandlerHeadStr 生成服务回复处理器头文件
func GetServiceRepliedHandlerHeadStr(methods RPCMethods) (string, error) {
	const tmplStr = `#pragma once
#include "muduo/net/TcpConnection.h"
{{.FirstMethodInfo.IncludeName }}
using namespace muduo;
using namespace muduo::net;

{{- range .Methods }}
void On{{ .KeyName }}{{ $.RepliedHandlerFileName }}(const TcpConnectionPtr& conn, const std::shared_ptr<{{ .CppResponse }}>& replied, Timestamp timestamp);

{{- end }}
`

	if len(methods) == 0 {
		return "", nil
	}

	data := struct {
		FirstMethodInfo        *MethodInfo
		Methods                RPCMethods
		RepliedHandlerFileName string
	}{
		FirstMethodInfo:        methods[0],
		Methods:                methods,
		RepliedHandlerFileName: _config.Global.Naming.RepliedHandlerFile,
	}

	return utils.GlobalEngine.MustExecute("serviceRepliedHandlerHead", tmplStr, data)
}

// GetServiceRepliedHandlerCppStr 生成服务回复处理器CPP文件
func GetServiceRepliedHandlerCppStr(dst string, methods RPCMethods, _ string, _ string) string {
	const tmplStr = `
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

	if len(methods) == 0 {
		return ""
	}

	emptyStr := ""
	yourCodesMap, firstCode, _ := ReadCodeSectionsFromFile(dst, &methods, GenerateMethodHandlerKeyNameWrapper, emptyStr)
	firstMethodInfo := methods[0]

	var methodsData []RepliedHandlerMethod
	for _, method := range methods {
		funcName := GenerateMethodHandlerKeyNameWrapper(method, emptyStr)
		code, exists := yourCodesMap[funcName]
		methodsData = append(methodsData, RepliedHandlerMethod{
			FuncName:    funcName,
			CppResponse: method.CppResponse(),
			Code:        code,
			HasCode:     exists,
			KeyName:     method.KeyName(),
		})
	}

	data := struct {
		CppRepliedHandlerInclude string
		FirstCode                string
		InitFuncName             string
		RepliedHandlerFileName   string
		Methods                  []RepliedHandlerMethod
		MessageIdName            string
		ServiceInfoHeadInclude   string
		ServiceInfoName          string
	}{
		CppRepliedHandlerInclude: firstMethodInfo.CppRepliedHandlerIncludeName(),
		FirstCode:                firstCode,
		InitFuncName:             firstMethodInfo.Service(),
		RepliedHandlerFileName:   _config.Global.Naming.RepliedHandlerFile,
		Methods:                  methodsData,
		MessageIdName:            _config.Global.Naming.MessageId,
		ServiceInfoHeadInclude:   firstMethodInfo.ServiceInfoHeadInclude(),
		ServiceInfoName:          _config.Global.DirectoryNames.ServiceInfoName,
	}

	return utils.ExecuteTemplate("serviceRepliedHandlerCpp", tmplStr, data)
}

// GenerateMethodHandlerKeyNameWrapper 生成方法处理器键名
func GenerateMethodHandlerKeyNameWrapper(info *MethodInfo, _ string) string {
	return "On" + info.KeyName() + _config.Global.Naming.RepliedHandlerFile
}
