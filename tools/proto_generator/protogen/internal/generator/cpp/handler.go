package cpp

import (
	"strings"
	"sync"

	"go.uber.org/zap"

	"protogen/internal"
	utils2 "protogen/internal/utils"
	"protogen/logger"
)

type HandlerConfig struct {
	IsValidFunc         func(*internal.RPCMethods) bool
	GenerateDataFunc    func(internal.RPCMethods) (string, error)
	GenerateCppDataFunc func(string, internal.RPCMethods, string, string) string
	Dir                 string
	CppDir              string
	HeaderExt           string
	CppExt              string
	IsRepliedHandler    bool
	IsGrpcHandler       bool
}

func writeHandlerHeadFile(wg *sync.WaitGroup, methodList internal.RPCMethods, cfg HandlerConfig) {
	if !cfg.IsValidFunc(&methodList) {
		return
	}

	first := methodList[0]
	// Legacy handlers require cc_generic_services; gRPC handlers do not.
	if !cfg.IsGrpcHandler && !first.CcGenericServices() {
		return
	}
	fileName := first.FileBaseNameNoEx() + cfg.HeaderExt
	fullPath := cfg.Dir + fileName

	data, err := cfg.GenerateDataFunc(methodList)
	if err != nil {
		logger.Global.Fatal("Failed to generate handler header file",
			zap.String("file_path", fullPath),
			zap.Error(err),
		)
	}

	utils2.WriteFileIfChanged(fullPath, []byte(data))
}

func writeHandlerCppFile(wg *sync.WaitGroup, methodList internal.RPCMethods, cfg HandlerConfig) {
	if !cfg.IsValidFunc(&methodList) {
		return
	}

	first := methodList[0]
	// Legacy handlers require cc_generic_services; gRPC handlers do not.
	if !cfg.IsGrpcHandler && !first.CcGenericServices() {
		return
	}
	fileName := strings.ToLower(first.FileBaseNameNoEx()) + cfg.CppExt
	fullPath := cfg.CppDir + fileName

	data := cfg.GenerateCppDataFunc(fullPath, methodList,
		getCppClassName(*first, cfg),
		getCppIncludeName(*first, cfg))

	utils2.WriteFileIfChanged(fullPath, []byte(data))
}

func getCppClassName(info internal.MethodInfo, cfg HandlerConfig) string {
	if cfg.IsRepliedHandler {
		return info.CppRepliedHandlerClassName()
	}
	return info.CppHandlerClassName()
}

func getCppIncludeName(info internal.MethodInfo, cfg HandlerConfig) string {
	if cfg.IsRepliedHandler {
		return info.CppRepliedHandlerIncludeName()
	}
	return info.CppHandlerIncludeName()
}
