package cpp

import (
	"protogen/internal"
	_config "protogen/internal/config"
	"sync"
)

func ProcessAllHandlers(wg *sync.WaitGroup, methodList internal.RPCMethods) {
	handlerConfigs := []HandlerConfig{
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
		// 节点间 RPC0(Gate / Scene / SceneScene)不再生成 rpc_replies 回包:回包在调用点的每次调用 lambda 里完成
		// (docs/design/muduo-rpc-channel.md §9.2)。rpc_replies 下只剩玩家回包链与手写的 gRPC 应答适配。
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
			IsValidFunc:              IsSceneNodeGrpcHandler,
			GenerateDataFunc:         internal.GetGrpcServiceHandlerHeadStr,
			GenerateHeadFromFileFunc: internal.GetGrpcServiceHandlerHeadFromFileStr,
			GenerateCppDataFunc:      internal.GetGrpcServiceHandlerCppStr,
			Dir:                      _config.Global.PathLists.MethodHandlerDirectories.SceneNodeGrpc,
			CppDir:                   _config.Global.PathLists.MethodHandlerDirectories.SceneNodeGrpc,
			HeaderExt:                _config.Global.FileExtensions.GrpcHandlerH,
			CppExt:                   _config.Global.FileExtensions.GrpcHandlerCpp,
			IsRepliedHandler:         false,
			IsGrpcHandler:            true,
		},
		{
			IsValidFunc:              IsBattleNodeGrpcHandler,
			GenerateDataFunc:         internal.GetGrpcServiceHandlerHeadStr,
			GenerateHeadFromFileFunc: internal.GetGrpcServiceHandlerHeadFromFileStr,
			GenerateCppDataFunc:      internal.GetGrpcServiceHandlerCppStr,
			Dir:                      _config.Global.PathLists.MethodHandlerDirectories.BattleNodeGrpc,
			CppDir:                   _config.Global.PathLists.MethodHandlerDirectories.BattleNodeGrpc,
			HeaderExt:                _config.Global.FileExtensions.GrpcHandlerH,
			CppExt:                   _config.Global.FileExtensions.GrpcHandlerCpp,
			IsGrpcHandler:            true,
		},
	}

	for _, cfg := range handlerConfigs {
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
