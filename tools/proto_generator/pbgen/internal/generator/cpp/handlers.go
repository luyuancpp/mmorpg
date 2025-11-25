package cpp

import (
	"pbgen/config"
	"pbgen/internal"
	"pbgen/internal/utils"
)

// ---------------- Game Node ----------------

var RoomHandler = HandlerConfig{
	IsValidFunc:         internal.IsRoomNodeHostedProtocolHandler,
	GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
	GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
	Dir:                 config.RoomNodeMethodHandlerDirectory,
	CppDir:              config.RoomNodeMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var RoomPlayerHandler = HandlerConfig{
	IsValidFunc:         internal.IsRoomNodeHostedPlayerProtocolHandler,
	GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
	GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
	Dir:                 config.RoomNodePlayerMethodHandlerDirectory,
	CppDir:              config.RoomNodePlayerMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var RoomRepliedHandler = HandlerConfig{
	IsValidFunc:         internal.IsRoomNodeReceivedProtocolResponseHandler,
	GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
	Dir:                 config.RoomNodeMethodRepliedHandlerDirectory,
	CppDir:              config.RoomNodeMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

var GsPlayerRepliedHandler = HandlerConfig{
	IsValidFunc:         internal.IsRoomNodeReceivedPlayerResponseHandler,
	GenerateDataFunc:    internal.GetPlayerMethodRepliedHeadStr,
	GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
	Dir:                 config.RoomNodePlayerMethodRepliedHandlerDirectory,
	CppDir:              config.RoomNodePlayerMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

// ---------------- Centre Node ----------------

var CentreHandler = HandlerConfig{
	IsValidFunc:         internal.IsCentreHostedServiceHandler,
	GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
	GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
	Dir:                 config.CentreNodeMethodHandlerDirectory,
	CppDir:              config.CentreNodeMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var CentrePlayerHandler = HandlerConfig{
	IsValidFunc:         internal.IsCentreHostedPlayerServiceHandler,
	GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
	GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
	Dir:                 config.CentreNodePlayerMethodHandlerDirectory,
	CppDir:              config.CentreNodePlayerMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var CentreRepliedHandler = HandlerConfig{
	IsValidFunc:         internal.IsCentreReceivedServiceResponseHandler,
	GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
	Dir:                 config.CentreMethodRepliedHandleDir,
	CppDir:              config.CentreMethodRepliedHandleDir,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

var CentrePlayerRepliedHandler = HandlerConfig{
	IsValidFunc:         internal.IsCentreReceivedPlayerServiceResponseHandler,
	GenerateDataFunc:    internal.GetPlayerMethodRepliedHeadStr,
	GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
	Dir:                 config.CentrePlayerMethodRepliedHandlerDirectory,
	CppDir:              config.CentrePlayerMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

// ---------------- Gate	 Node ----------------

var GateHandler = HandlerConfig{
	IsValidFunc:         internal.IsGateNodeHostedServiceHandler,
	GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
	GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
	Dir:                 config.GateMethodHandlerDirectory,
	CppDir:              config.GateMethodHandlerDirectory,
	HeaderExt:           config.HandlerHeaderExtension,
	CppExt:              config.HandlerCppExtension,
	IsRepliedHandler:    false,
}

var GateRepliedHandler = HandlerConfig{
	IsValidFunc:         internal.IsGateNodeReceivedResponseHandler,
	GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
	GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
	Dir:                 config.GateMethodRepliedHandlerDirectory,
	CppDir:              config.GateMethodRepliedHandlerDirectory,
	HeaderExt:           config.RepliedHandlerHeaderExtension,
	CppExt:              config.CppRepliedHandlerEx,
	IsRepliedHandler:    true,
}

func ProcessAllHandlers(methodList internal.RPCMethods) {

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
