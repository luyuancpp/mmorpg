package internal

import (
	"pbgen/internal/config"
	"pbgen/utils"
)

// ---------------- Game Node ----------------

var RoomHandler = HandlerConfig{
	IsValidFunc:         IsRoomNodeMethodHandler,
	GenerateDataFunc:    getServiceHandlerHeadStr,
	GenerateCppDataFunc: getServiceHandlerCppStr,
	Dir:                 config.RoomNodeMethodHandlerDirectory,
	CppDir:              config.RoomNodeMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var RoomPlayerHandler = HandlerConfig{
	IsValidFunc:         IsRoomNodePlayerHandler,
	GenerateDataFunc:    getPlayerServiceHeadStr,
	GenerateCppDataFunc: getPlayerServiceHandlerCppStr,
	Dir:                 config.RoomNodePlayerMethodHandlerDirectory,
	CppDir:              config.RoomNodePlayerMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var RoomRepliedHandler = HandlerConfig{
	IsValidFunc:         isRoomNodeMethodRepliedHandler,
	GenerateDataFunc:    getServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: getServiceRepliedHandlerCppStr,
	Dir:                 config.RoomNodeMethodRepliedHandlerDirectory,
	CppDir:              config.RoomNodeMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

var GsPlayerRepliedHandler = HandlerConfig{
	IsValidFunc:         isRoomNodePlayerRepliedHandler,
	GenerateDataFunc:    getPlayerMethodRepliedHeadStr,
	GenerateCppDataFunc: getPlayerServiceHandlerCppStr,
	Dir:                 config.RoomNodePlayerMethodRepliedHandlerDirectory,
	CppDir:              config.RoomNodePlayerMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

// ---------------- Centre Node ----------------

var CentreHandler = HandlerConfig{
	IsValidFunc:         IsCentreServiceMethodHandler,
	GenerateDataFunc:    getServiceHandlerHeadStr,
	GenerateCppDataFunc: getServiceHandlerCppStr,
	Dir:                 config.CentreNodeMethodHandlerDirectory,
	CppDir:              config.CentreNodeMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var CentrePlayerHandler = HandlerConfig{
	IsValidFunc:         IsCentrePlayerServiceMethodHandler,
	GenerateDataFunc:    getPlayerServiceHeadStr,
	GenerateCppDataFunc: getPlayerServiceHandlerCppStr,
	Dir:                 config.CentreNodePlayerMethodHandlerDirectory,
	CppDir:              config.CentreNodePlayerMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var CentreRepliedHandler = HandlerConfig{
	IsValidFunc:         IsCentreServiceResponseHandler,
	GenerateDataFunc:    getServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: getServiceRepliedHandlerCppStr,
	Dir:                 config.CentreMethodRepliedHandleDir,
	CppDir:              config.CentreMethodRepliedHandleDir,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

var CentrePlayerRepliedHandler = HandlerConfig{
	IsValidFunc:         IsCentrePlayerServiceResponseHandler,
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
		RoomHandler,
		RoomPlayerHandler,
		RoomRepliedHandler,
		GsPlayerRepliedHandler,
		CentreHandler,
		CentrePlayerHandler,
		CentreRepliedHandler,
		CentrePlayerRepliedHandler,
		GateHandler,
		GateRepliedHandler,
	}

	for _, cfg := range handlerConfigs {
		utils.Wg.Add(1)
		writeHandlerHeadFile(methodList, cfg)
		utils.Wg.Add(1)
		writeHandlerCppFile(methodList, cfg)
	}

}
