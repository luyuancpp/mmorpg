package cpp

import (
	"log"
	"pbgen/internal"
	"pbgen/utils"
	"strings"
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
}

func writeHandlerHeadFile(methodList internal.RPCMethods, cfg HandlerConfig) {
	defer utils.Wg.Done()

	if !cfg.IsValidFunc(&methodList) {
		return
	}

	first := methodList[0]
	if !first.CcGenericServices() {
		return
	}
	fileName := first.FileBaseNameNoEx() + cfg.HeaderExt
	fullPath := cfg.Dir + fileName

	data, err := cfg.GenerateDataFunc(methodList)
	if err != nil {
		log.Fatal(err)
	}

	utils.WriteFileIfChanged(fullPath, []byte(data))
}

func writeHandlerCppFile(methodList internal.RPCMethods, cfg HandlerConfig) {
	defer utils.Wg.Done()

	if !cfg.IsValidFunc(&methodList) {
		return
	}

	first := methodList[0]
	if !first.CcGenericServices() {
		return
	}
	fileName := strings.ToLower(first.FileBaseNameNoEx()) + cfg.CppExt
	fullPath := cfg.CppDir + fileName

	data := cfg.GenerateCppDataFunc(fullPath, methodList,
		getCppClassName(*first, cfg),
		getCppIncludeName(*first, cfg))

	utils.WriteFileIfChanged(fullPath, []byte(data))
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
