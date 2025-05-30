package internal

import (
	"pbgen/config"
	"pbgen/util"
)

// ---------------- Game Node ----------------

var GsHandler = HandlerConfig{
	IsValidFunc:         IsGsMethodHandler,
	GenerateDataFunc:    getServiceHandlerHeadStr,
	GenerateCppDataFunc: getServiceHandlerCppStr,
	Dir:                 config.GameNodeMethodHandlerDirectory,
	CppDir:              config.GameNodeMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	UseCppClassName:     false,
	IsRepliedHandler:    false,
}

var GsPlayerHandler = HandlerConfig{
	IsValidFunc:         IsGsPlayerHandler,
	GenerateDataFunc:    getPlayerServiceHeadStr,
	GenerateCppDataFunc: getPlayerServiceHandlerCppStr,
	Dir:                 config.GameNodePlayerMethodHandlerDirectory,
	CppDir:              config.GameNodePlayerMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	UseCppClassName:     true,
	IsRepliedHandler:    false,
}

var GsRepliedHandler = HandlerConfig{
	IsValidFunc:         isGsMethodRepliedHandler,
	GenerateDataFunc:    getServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: getServiceRepliedHandlerCppStr,
	Dir:                 config.GameNodeMethodRepliedHandlerDirectory,
	CppDir:              config.GameNodeMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	UseCppClassName:     false,
	IsRepliedHandler:    true,
}

var GsPlayerRepliedHandler = HandlerConfig{
	IsValidFunc:         isGsPlayerRepliedHandler,
	GenerateDataFunc:    getPlayerMethodRepliedHeadStr,
	GenerateCppDataFunc: getPlayerServiceHandlerCppStr,
	Dir:                 config.GameNodePlayerMethodRepliedHandlerDirectory,
	CppDir:              config.GameNodePlayerMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	UseCppClassName:     true,
	IsRepliedHandler:    true,
}

// ---------------- Centre Node ----------------

var CentreHandler = HandlerConfig{
	IsValidFunc:         isCentreMethodHandler,
	GenerateDataFunc:    getServiceHandlerHeadStr,
	GenerateCppDataFunc: getServiceHandlerCppStr,
	Dir:                 config.CentreNodeMethodHandlerDirectory,
	CppDir:              config.CentreNodeMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	UseCppClassName:     false,
	IsRepliedHandler:    false,
}

var CentrePlayerHandler = HandlerConfig{
	IsValidFunc:         isCentrePlayerHandler,
	GenerateDataFunc:    getPlayerServiceHeadStr,
	GenerateCppDataFunc: getPlayerServiceHandlerCppStr,
	Dir:                 config.CentreNodePlayerMethodHandlerDirectory,
	CppDir:              config.CentreNodePlayerMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	UseCppClassName:     true,
	IsRepliedHandler:    false,
}

var CentreRepliedHandler = HandlerConfig{
	IsValidFunc:         isCentreMethodRepliedHandler,
	GenerateDataFunc:    getServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: getServiceRepliedHandlerCppStr,
	Dir:                 config.CentreMethodRepliedHandleDir,
	CppDir:              config.CentreMethodRepliedHandleDir,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	UseCppClassName:     false,
	IsRepliedHandler:    true,
}

var CentrePlayerRepliedHandler = HandlerConfig{
	IsValidFunc:         isCentrePlayerRepliedHandler,
	GenerateDataFunc:    getPlayerMethodRepliedHeadStr,
	GenerateCppDataFunc: getPlayerServiceHandlerCppStr,
	Dir:                 config.CentrePlayerMethodRepliedHandlerDirectory,
	CppDir:              config.CentrePlayerMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	UseCppClassName:     true,
	IsRepliedHandler:    true,
}

func ProcessAllHandlers(methodList RPCMethods) {

	handlerConfigs := []HandlerConfig{
		GsHandler,
		GsPlayerHandler,
		GsRepliedHandler,
		GsPlayerRepliedHandler,
		CentreHandler,
		CentrePlayerHandler,
		CentreRepliedHandler,
		CentrePlayerRepliedHandler,
	}

	for _, cfg := range handlerConfigs {
		util.Wg.Add(1)
		writeHandlerHeadFile(methodList, cfg)
		util.Wg.Add(1)
		writeHandlerCppFile(methodList, cfg)
	}

}
