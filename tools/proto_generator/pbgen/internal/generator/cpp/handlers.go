package cpp

import (
	"pbgen/internal"
	_config "pbgen/internal/config"
	"sync"
)

func ProcessAllHandlers(wg *sync.WaitGroup, methodList internal.RPCMethods) {
	// 直接定义配置列表，减少临时变量
	handlerConfigs := []HandlerConfig{
		// ---------------- Game Node ----------------
		{
			IsValidFunc:         IsSceneNodeHostedProtocolHandler,
			GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.SceneNode,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.SceneNode,
			HeaderExt:           _config.Global.FileExtensions.HandlerH,
			CppExt:              _config.Global.FileExtensions.HandlerCpp,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsSceneNodeHostedPlayerProtocolHandler,
			GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.SceneNodePlayer,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.SceneNodePlayer,
			HeaderExt:           _config.Global.FileExtensions.HandlerH,
			CppExt:              _config.Global.FileExtensions.HandlerCpp,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsSceneNodeReceivedProtocolResponseHandler,
			GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.SceneNodeReplied,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.SceneNodeReplied,
			HeaderExt:           _config.Global.FileExtensions.RepliedHandlerHeaderExtension,
			CppExt:              _config.Global.FileExtensions.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		{
			IsValidFunc:         IsSceneNodeReceivedPlayerResponseHandler,
			GenerateDataFunc:    internal.GetPlayerMethodRepliedHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.SceneNodePlayerReplied,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.SceneNodePlayerReplied,
			HeaderExt:           _config.Global.FileExtensions.RepliedHandlerHeaderExtension,
			CppExt:              _config.Global.FileExtensions.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		// ---------------- Centre Node ----------------
		{
			IsValidFunc:         IsCentreHostedServiceHandler,
			GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.CentreNode,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.CentreNode,
			HeaderExt:           _config.Global.FileExtensions.HandlerH,
			CppExt:              _config.Global.FileExtensions.HandlerCpp,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsCentreHostedPlayerServiceHandler,
			GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.CentreNodePlayer,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.CentreNodePlayer,
			HeaderExt:           _config.Global.FileExtensions.HandlerH,
			CppExt:              _config.Global.FileExtensions.HandlerCpp,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsCentreReceivedServiceResponseHandler,
			GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.CentreNodeReplied,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.CentreNodeReplied,
			HeaderExt:           _config.Global.FileExtensions.RepliedHandlerHeaderExtension,
			CppExt:              _config.Global.FileExtensions.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		{
			IsValidFunc:         IsCentreReceivedPlayerServiceResponseHandler,
			GenerateDataFunc:    internal.GetPlayerMethodRepliedHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.CentreNodePlayerReplied,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.CentreNodePlayerReplied,
			HeaderExt:           _config.Global.FileExtensions.RepliedHandlerHeaderExtension,
			CppExt:              _config.Global.FileExtensions.CppRepliedHandlerEx,
			IsRepliedHandler:    true,
		},
		// ---------------- Gate Node ----------------
		{
			IsValidFunc:         IsGateNodeHostedServiceHandler,
			GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.GateNode,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.GateNode,
			HeaderExt:           _config.Global.FileExtensions.HandlerH,
			CppExt:              _config.Global.FileExtensions.HandlerCpp,
			IsRepliedHandler:    false,
		},
		{
			IsValidFunc:         IsGateNodeReceivedResponseHandler,
			GenerateDataFunc:    internal.GetServiceRepliedHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceRepliedHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.GateNodeReplied,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.GateNodeReplied,
			HeaderExt:           _config.Global.FileExtensions.RepliedHandlerHeaderExtension,
			CppExt:              _config.Global.FileExtensions.CppRepliedHandlerEx,
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
