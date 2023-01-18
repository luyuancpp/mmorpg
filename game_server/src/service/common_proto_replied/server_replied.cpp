//#include "server_replied.h"
//
//#include "muduo/base/Logging.h"
//
//#include "src/game_logic/comp/scene_comp.h"
//#include "src/game_logic/thread_local/thread_local_storage.h"
//#include "src/game_logic/thread_local/thread_local_storage.h"
//#include "src/game_logic/scene/scene.h"
//#include "src/network/controller_node.h"
//#include "src/network/rpc_client.h"
//#include "src/system/gs_scene_system.h"
//#include "src/game_config/deploy_json.h"
//#include "src/game_server.h"
//
//void StartGsControllerReplied(StartGsControllerRpc replied)
//{
//    if (GameConfig::GetSingleton().server_type() == kMainSceneServer)
//    {
//        auto rsp = replied->s_rp_;
//        for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
//        {
//            CreateSceneBySceneInfoP param;
//            param.scene_info_ = rsp->scenes_info(i);
//            GsSceneSystem::CreateSceneByGuid(param);
//        }
//        LOG_DEBUG << rsp->DebugString();
//    }
//    else if (GameConfig::GetSingleton().server_type() == kMainSceneCrossServer)
//    {
//        GameConnectToControllerRequest rpc;
//        g_gs->lobby_stub().CallMethod(rpc, &LobbyService_Stub::GameConnectToController);
//    }
//}
//
//void StartCrossGsReplied(StartCrossGsRpc replied)
//{
//    auto rsp = replied->s_rp_;
//    for (int32_t i = 0; i < rsp->scenes_info_size(); ++i)
//    {
//        CreateSceneBySceneInfoP param;
//        param.scene_info_ = rsp->scenes_info(i);
//        GsSceneSystem::CreateSceneByGuid(param);
//    }
//    LOG_DEBUG << rsp->DebugString();
//}
//
//
