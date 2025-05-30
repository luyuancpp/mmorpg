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
	UseCppClassName     bool
	IsRepliedHandler    bool
}

func writeHandlerHeadFile(methodList RPCMethods, cfg HandlerConfig) {
	defer util.Wg.Done()

	if !cfg.IsValidFunc(&methodList) {
		return
	}

	first := methodList[0]
	fileName := first.FileNameNoEx() + cfg.HeaderExt
	fullPath := cfg.Dir + fileName

	data, err := cfg.GenerateDataFunc(methodList)
	if err != nil {
		log.Fatal(err)
	}

	util.WriteMd5Data2File(fullPath, data)
}

func writeHandlerCppFile(methodList RPCMethods, cfg HandlerConfig) {
	defer util.Wg.Done()

	if !cfg.IsValidFunc(&methodList) {
		return
	}

	first := methodList[0]
	fileName := strings.ToLower(first.FileNameNoEx()) + cfg.CppExt
	fullPath := cfg.CppDir + fileName

	var data string
	if cfg.UseCppClassName {
		data = cfg.GenerateCppDataFunc(fullPath, methodList,
			getCppClassName(*first, cfg),
			getCppIncludeName(*first, cfg))
	} else {
		data = getMethodHandlerCppStr(fullPath, &methodList)
	}

	util.WriteMd5Data2File(fullPath, data)
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
