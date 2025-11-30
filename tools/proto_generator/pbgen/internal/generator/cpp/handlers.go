package cpp

import (
	"pbgen/config"
	"pbgen/internal"
	"sync"
)

func ProcessAllHandlers(wg *sync.WaitGroup, methodList internal.RPCMethods) {
	// 直接定义配置列表，减少临时变量
	handlerConfigs := []HandlerConfig{
		// ---------------- Game Node ----------------
		{
			IsValidFunc:         IsRoomNodeHostedProtocolHandler,
			GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
			Dir:                 config.RoomNodeMethodHandlerDirectory,
			CppDir:              config.RoomNodeMethodHandlerDirectory,
			HeaderExt:           config.HandlerHeaderExtension,
			CppExt:              config.HandlerCppExtension,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsRoomNodeHostedPlayerProtocolHandler,
			GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 config.RoomNodePlayerMethodHandlerDirectory,
			CppDir:              config.RoomNodePlayerMethodHandlerDirectory,
			HeaderExt:           config.HandlerHeaderExtension,
			CppExt:              config.HandlerCppExtension,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsRoomNodeReceivedProtocolResponseHandler,
			GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
			Dir:                 config.RoomNodeMethodRepliedHandlerDirectory,
			CppDir:              config.RoomNodeMethodRepliedHandlerDirectory,
			HeaderExt:           config.RepliedHandlerHeaderExtension,
			CppExt:              config.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		{
			IsValidFunc:         IsRoomNodeReceivedPlayerResponseHandler,
			GenerateDataFunc:    internal.GetPlayerMethodRepliedHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 config.RoomNodePlayerMethodRepliedHandlerDirectory,
			CppDir:              config.RoomNodePlayerMethodRepliedHandlerDirectory,
			HeaderExt:           config.RepliedHandlerHeaderExtension,
			CppExt:              config.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		// ---------------- Centre Node ----------------
		{
			IsValidFunc:         IsCentreHostedServiceHandler,
			GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
			Dir:                 config.CentreNodeMethodHandlerDirectory,
			CppDir:              config.CentreNodeMethodHandlerDirectory,
			HeaderExt:           config.HandlerHeaderExtension,
			CppExt:              config.HandlerCppExtension,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsCentreHostedPlayerServiceHandler,
			GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 config.CentreNodePlayerMethodHandlerDirectory,
			CppDir:              config.CentreNodePlayerMethodHandlerDirectory,
			HeaderExt:           config.HandlerHeaderExtension,
			CppExt:              config.HandlerCppExtension,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsCentreReceivedServiceResponseHandler,
			GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
			Dir:                 config.CentreMethodRepliedHandleDir,
			CppDir:              config.CentreMethodRepliedHandleDir,
			HeaderExt:           config.RepliedHandlerHeaderExtension,
			CppExt:              config.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		{
			IsValidFunc:         IsCentreReceivedPlayerServiceResponseHandler,
			GenerateDataFunc:    internal.GetPlayerMethodRepliedHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 config.CentrePlayerMethodRepliedHandlerDirectory,
			CppDir:              config.CentrePlayerMethodRepliedHandlerDirectory,
			HeaderExt:           config.RepliedHandlerHeaderExtension,
			CppExt:              config.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		// ---------------- Gate Node ----------------
		{
			IsValidFunc:         IsGateNodeHostedServiceHandler,
			GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
			Dir:                 config.GateMethodHandlerDirectory,
			CppDir:              config.GateMethodHandlerDirectory,
			HeaderExt:           config.HandlerHeaderExtension,
			CppExt:              config.HandlerCppExtension,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsGateNodeReceivedResponseHandler,
			GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
			Dir:                 config.GateMethodRepliedHandlerDirectory,
			CppDir:              config.GateMethodRepliedHandlerDirectory,
			HeaderExt:           config.RepliedHandlerHeaderExtension,
			CppExt:              config.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
	}

	for _, cfg := range handlerConfigs {
		// 使用闭包捕获当前配置，避免循环变量问题
		currentCfg := cfg

		wg.Add(1)
		go func() {
			defer wg.Done()
			writeHandlerHeadFile(wg, methodList, currentCfg)
		}()

		wg.Add(1)
		go func() {
			defer wg.Done()
			writeHandlerCppFile(wg, methodList, currentCfg)
		}()
	}
}
