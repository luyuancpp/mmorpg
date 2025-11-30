package cpp

import (
	"pbgen/config"
	"pbgen/internal"
	"sync"
)

func ProcessAllHandlers(wg *sync.WaitGroup, methodList internal.RPCMethods) {

	// ---------------- Game Node ----------------

	var RoomHandler = HandlerConfig{
		IsValidFunc:         IsRoomNodeHostedProtocolHandler,
		GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
		GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
		Dir:                 config.RoomNodeMethodHandlerDirectory,
		CppDir:              config.RoomNodeMethodHandlerDirectory,
		HeaderExt:           config.HandlerHeaderExtension,
		CppExt:              config.HandlerCppExtension,
		IsRepliedHandler:    false,
	}

	var RoomPlayerHandler = HandlerConfig{
		IsValidFunc:         IsRoomNodeHostedPlayerProtocolHandler,
		GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
		GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
		Dir:                 config.RoomNodePlayerMethodHandlerDirectory,
		CppDir:              config.RoomNodePlayerMethodHandlerDirectory,
		HeaderExt:           config.HandlerHeaderExtension,
		CppExt:              config.HandlerCppExtension,
		IsRepliedHandler:    false,
	}

	var RoomRepliedHandler = HandlerConfig{
		IsValidFunc:         IsRoomNodeReceivedProtocolResponseHandler,
		GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
		GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
		Dir:                 config.RoomNodeMethodRepliedHandlerDirectory,
		CppDir:              config.RoomNodeMethodRepliedHandlerDirectory,
		HeaderExt:           config.RepliedHandlerHeaderExtension,
		CppExt:              config.CppRepliedHandlerEx,
		IsRepliedHandler:    true,
	}

	var GsPlayerRepliedHandler = HandlerConfig{
		IsValidFunc:         IsRoomNodeReceivedPlayerResponseHandler,
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
		IsValidFunc:         IsCentreHostedServiceHandler,
		GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
		GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
		Dir:                 config.CentreNodeMethodHandlerDirectory,
		CppDir:              config.CentreNodeMethodHandlerDirectory,
		HeaderExt:           config.HandlerHeaderExtension,
		CppExt:              config.HandlerCppExtension,
		IsRepliedHandler:    false,
	}

	var CentrePlayerHandler = HandlerConfig{
		IsValidFunc:         IsCentreHostedPlayerServiceHandler,
		GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
		GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
		Dir:                 config.CentreNodePlayerMethodHandlerDirectory,
		CppDir:              config.CentreNodePlayerMethodHandlerDirectory,
		HeaderExt:           config.HandlerHeaderExtension,
		CppExt:              config.HandlerCppExtension,
		IsRepliedHandler:    false,
	}

	var CentreRepliedHandler = HandlerConfig{
		IsValidFunc:         IsCentreReceivedServiceResponseHandler,
		GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
		GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
		Dir:                 config.CentreMethodRepliedHandleDir,
		CppDir:              config.CentreMethodRepliedHandleDir,
		HeaderExt:           config.RepliedHandlerHeaderExtension,
		CppExt:              config.CppRepliedHandlerEx,
		IsRepliedHandler:    true,
	}

	var CentrePlayerRepliedHandler = HandlerConfig{
		IsValidFunc:         IsCentreReceivedPlayerServiceResponseHandler,
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
		IsValidFunc:         IsGateNodeHostedServiceHandler,
		GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
		GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
		Dir:                 config.GateMethodHandlerDirectory,
		CppDir:              config.GateMethodHandlerDirectory,
		HeaderExt:           config.HandlerHeaderExtension,
		CppExt:              config.HandlerCppExtension,
		IsRepliedHandler:    false,
	}

	var GateRepliedHandler = HandlerConfig{
		IsValidFunc:         IsGateNodeReceivedResponseHandler,
		GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
		GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
		Dir:                 config.GateMethodRepliedHandlerDirectory,
		CppDir:              config.GateMethodRepliedHandlerDirectory,
		HeaderExt:           config.RepliedHandlerHeaderExtension,
		CppExt:              config.CppRepliedHandlerEx,
		IsRepliedHandler:    true,
	}

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
		wg.Add(1)
		writeHandlerHeadFile(wg, methodList, cfg)
		wg.Add(1)
		writeHandlerCppFile(wg, methodList, cfg)
	}

}
