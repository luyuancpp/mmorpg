package internal

import (
	_config "protogen/internal/config"
	"protogen/internal/utils"
	"protogen/logger"

	"go.uber.org/zap"
)

// GenServiceIdHeader generates the service ID header file content.
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

// writeServiceIdHeadFile writes the service ID header file.
func writeServiceIdHeadFile(serviceInfo []*RPCServiceInfo) {
	if len(serviceInfo) == 0 {
		return
	}

	fileName := serviceInfo[0].ServiceInfoHeadInclude()
	outputPath := _config.Global.Paths.ServiceInfoDir + fileName
	utils.WriteFileIfChanged(outputPath, []byte(GenServiceIdHeader(serviceInfo)))

	logger.Global.Info("Service ID header file generated",
		zap.String("file_path", outputPath),
		zap.Int("service_count", len(serviceInfo)),
	)
}
