package internal

import (
	"pbgen/config"
	"pbgen/util"
)

// ---------------- Game Node ----------------

var GsHandler = HandlerConfig{
	IsValidFunc:         IsSceneMethodHandler,
	GenerateDataFunc:    getServiceHandlerHeadStr,
	GenerateCppDataFunc: getServiceHandlerCppStr,
	Dir:                 config.GameNodeMethodHandlerDirectory,
	CppDir:              config.GameNodeMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
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
	IsRepliedHandler:    true,
}

// ---------------- Gate	 Node ----------------

var GateHandler = HandlerConfig{
	IsValidFunc:         isGateServiceHandler,
	GenerateDataFunc:    getServiceHandlerHeadStr,
	GenerateCppDataFunc: getServiceHandlerCppStr,
	Dir:                 config.GateMethodHandlerDirectory,
	CppDir:              config.GateMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var GateRepliedHandler = HandlerConfig{
	IsValidFunc:         isGateMethodRepliedHandler,
	GenerateDataFunc:    getServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: getServiceRepliedHandlerCppStr,
	Dir:                 config.GateMethodRepliedHandlerDirectory,
	CppDir:              config.GateMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
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
		GateHandler,
		GateRepliedHandler,
	}

	for _, cfg := range handlerConfigs {
		util.Wg.Add(1)
		writeHandlerHeadFile(methodList, cfg)
		util.Wg.Add(1)
		writeHandlerCppFile(methodList, cfg)
	}

}
