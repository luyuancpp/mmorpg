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
		},
		{
			IsValidFunc:         IsSceneNodeHostedPlayerProtocolHandler,
			GenerateDataFunc:    internal.GetPlayerServiceHeadStr,
			GenerateCppDataFunc: internal.GetPlayerServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.SceneNodePlayer,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.SceneNodePlayer,
			HeaderExt:           _config.Global.FileExtensions.HandlerH,
			CppExt:              _config.Global.FileExtensions.HandlerCpp,
		},
		// 不再生成 rpc_replies 回包:节点间 RPC0(Gate / Scene / SceneScene)的回包在调用点的每次调用 lambda 里完成
		// (docs/design/muduo-rpc-channel.md §9.2);玩家回包链(PlayerServiceReplied / gPlayerServiceReplied)从无读者,
		// 已随第 6 批删除(§13)。rpc_replies 下只剩手写的 gRPC 应答适配与 gate 的空 InitServiceHandler。
		{
			IsValidFunc:         IsGateNodeHostedServiceHandler,
			GenerateDataFunc:    internal.GetServiceHandlerHeadStr,
			GenerateCppDataFunc: internal.GetServiceHandlerCppStr,
			Dir:                 _config.Global.PathLists.MethodHandlerDirectories.GateNode,
			CppDir:              _config.Global.PathLists.MethodHandlerDirectories.GateNode,
			HeaderExt:           _config.Global.FileExtensions.HandlerH,
			CppExt:              _config.Global.FileExtensions.HandlerCpp,
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
