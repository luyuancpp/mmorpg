package internal

import (
	"go.uber.org/zap"
	_config "pbgen/internal/config"
	"pbgen/internal/utils"
	"pbgen/logger"
)

// GenServiceIdHeader 生成服务 ID 头文件内容
func GenServiceIdHeader(serviceInfo []*RPCServiceInfo) string {
	if len(serviceInfo) == 0 {
		return ""
	}

	const tmplStr = `#pragma once
#include <cstdint>

{{.IncludeName}}
{{- range .ServiceInfo }}
{{range .Methods}}
constexpr uint32_t {{.KeyName}}{{$.MessageIdName}} = {{.Id}};
constexpr uint32_t {{.KeyName}}Index = {{.Index}};
#define {{.KeyName}}Method  ::{{.Service}}_Stub::descriptor()->method({{.Index}})
{{end}}
{{end}}
`

	data := struct {
		IncludeName   string
		ServiceInfo   []*RPCServiceInfo
		MessageIdName string
	}{
		IncludeName:   serviceInfo[0].IncludeName(),
		ServiceInfo:   serviceInfo,
		MessageIdName: _config.Global.Naming.MessageId,
	}

	return utils.ExecuteTemplate("serviceIdHeader", tmplStr, data)
}

// writeServiceIdHeadFile 写入服务ID头文件
func writeServiceIdHeadFile(serviceInfo []*RPCServiceInfo) {
	if len(serviceInfo) == 0 {
		return
	}

	fileName := serviceInfo[0].ServiceInfoHeadInclude()
	outputPath := _config.Global.Paths.ServiceInfoDir + fileName
	utils.WriteFileIfChanged(outputPath, []byte(GenServiceIdHeader(serviceInfo)))

	logger.Global.Info("服务ID头文件生成完成",
		zap.String("file_path", outputPath),
		zap.Int("service_count", len(serviceInfo)),
	)
}
