package internal

import (
	_config "protogen/internal/config"
	"protogen/internal/utils"
)

// 本文件只剩玩家回包链(rpc_replies/player/* 的 <Svc>Reply : PlayerServiceReplied)。节点级 muduo 回包
// (On<KeyName>Reply + gRpcResponseDispatcher 注册)已退场,回包改在调用点的每次调用 lambda 里完成
// (docs/design/muduo-rpc-channel.md §9.2)。

// RepliedMethod holds data for a replied RPC method.
type RepliedMethod struct {
	Method      string
	CppRequest  string
	CppResponse string
}

// GetPlayerMethodRepliedHeadStr generates the player replied method header file.
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

// getPlayerMethodRepliedHandlerFunctions generates player replied handler function declarations.
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
