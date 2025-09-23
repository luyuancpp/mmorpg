package internal

import (
	"log"
	"pbgen/util"
	"strings"
)

type HandlerConfig struct {
	IsValidFunc         func(*RPCMethods) bool
	GenerateDataFunc    func(RPCMethods) (string, error)
	GenerateCppDataFunc func(string, RPCMethods, string, string) string
	Dir                 string
	CppDir              string
	HeaderExt           string
	CppExt              string
	IsRepliedHandler    bool
}

func writeHandlerHeadFile(methodList RPCMethods, cfg HandlerConfig) {
	defer util.Wg.Done()

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

	WriteFileIfChanged(fullPath, []byte(data))
}

func writeHandlerCppFile(methodList RPCMethods, cfg HandlerConfig) {
	defer util.Wg.Done()

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

	WriteFileIfChanged(fullPath, []byte(data))
}

func getCppClassName(info MethodInfo, cfg HandlerConfig) string {
	if cfg.IsRepliedHandler {
		return info.CppRepliedHandlerClassName()
	}
	return info.CppHandlerClassName()
}

func getCppIncludeName(info MethodInfo, cfg HandlerConfig) string {
	if cfg.IsRepliedHandler {
		return info.CppRepliedHandlerIncludeName()
	}
	return info.CppHandlerIncludeName()
}
