package internal

import (
	"sort"
	"sync"

	"go.uber.org/zap"
	_config "pbgen/internal/config"
	"pbgen/internal/utils"
	"pbgen/logger"
)

// checkRepliedCb 回调函数类型
type checkRepliedCb func(methods *RPCMethods) bool

// RegisterFileData 注册文件数据
type RegisterFileData struct {
	Includes  []string
	InitLines []string
}

// GenRegisterFile 生成服务注册文件
func GenRegisterFile(wg *sync.WaitGroup, dst string, cb checkRepliedCb) {
	const tmplStr = `
#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>
{{- range .Includes }}
{{ . }}
{{ end }}

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
{{- range .InitLines }}
{{ . }}
{{ end }}
}
`

	defer wg.Done()

	var includes []string
	var initLines []string

	logger.Global.Info("开始生成服务注册文件",
		zap.String("output_path", dst),
	)

	for _, service := range GlobalRPCServiceList {
		if !cb(&service.Methods) {
			continue
		}
		first := service.Methods[0]
		includes = append(includes, first.CppHandlerIncludeName())
		initLine := " gNodeService.emplace(\"" + first.Service() + "\", std::make_unique_for_overwrite<" + first.Service() + _config.Global.Naming.HandlerFile + ">());"
		initLines = append(initLines, initLine)
	}

	data := RegisterFileData{
		Includes:  includes,
		InitLines: initLines,
	}

	content := utils.ExecuteTemplate("registerFile", tmplStr, data)
	utils.WriteFileIfChanged(dst, []byte(content))

	logger.Global.Info("服务注册文件生成完成",
		zap.String("output_path", dst),
		zap.Int("include_count", len(includes)),
	)
}

// RepliedRegisterData 回复注册文件数据
type RepliedRegisterData struct {
	InitFuncs []string
}

// WriteRepliedRegisterFile 生成回复注册文件
func WriteRepliedRegisterFile(wg *sync.WaitGroup, dst string, cb checkRepliedCb) {
	const tmplStr = `
void InitReply()
{
{{- range .InitFuncs }}
    void {{ . }}();
    {{ . }}();

{{- end }}
}
`

	defer wg.Done()

	var initFuncList []string

	logger.Global.Info("开始生成回复注册文件",
		zap.String("output_path", dst),
	)

	for _, service := range GlobalRPCServiceList {
		if !cb(&service.Methods) {
			continue
		}
		first := service.Methods[0]
		initFunc := "Init" + first.Service() + _config.Global.Naming.RepliedHandlerFile
		initFuncList = append(initFuncList, initFunc)
	}

	data := RepliedRegisterData{
		InitFuncs: initFuncList,
	}

	content := utils.ExecuteTemplate("repliedRegister", tmplStr, data)
	utils.WriteFileIfChanged(dst, []byte(content))

	logger.Global.Info("回复注册文件生成完成",
		zap.String("output_path", dst),
		zap.Int("init_func_count", len(initFuncList)),
	)
}

// GenerateServiceConstants 生成服务常量文件
func GenerateServiceConstants(wg *sync.WaitGroup) {
	logger.Global.Info("开始生成服务常量文件",
		zap.String("service_info_dir", _config.Global.Paths.ServiceInfoDir),
	)

	FileServiceMap.Range(func(k, v interface{}) bool {
		protoFile := k.(string)
		serviceList := v.([]*RPCServiceInfo)
		wg.Add(1)
		go func(protoFile string, serviceInfo []*RPCServiceInfo) {
			defer wg.Done()

			if len(serviceInfo) == 0 {
				return
			}

			sort.Slice(serviceInfo, func(i, j int) bool {
				return serviceInfo[i].ServiceIndex < serviceInfo[j].ServiceIndex
			})

			writeServiceIdHeadFile(serviceInfo)
		}(protoFile, serviceList)

		return true
	})
}
