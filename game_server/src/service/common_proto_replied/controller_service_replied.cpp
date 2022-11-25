#include "controller_service_replied.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene.h"
#include "src/pb/pbc/service_method/lobby_scenemethod.h"
#include "src/system/gs_scene_system.h"
#include "src/game_config/deploy_json.h"
#include "src/game_server.h"

void StartGsControllerReplied(const TcpConnectionPtr& conn, const StartGsResponsePtr& replied, Timestamp timestamp)
{
    if (GameConfig::GetSingleton().server_type() == kMainSceneServer)
    {
        for (int32_t i = 0; i < replied->scenes_info_size(); ++i)
        {
            CreateSceneBySceneInfoP param;
            param.scene_info_ = replied->scenes_info(i);
            GsSceneSystem::CreateSceneByGuid(param);
        }
        LOG_DEBUG << replied->DebugString();
    }
    else if (GameConfig::GetSingleton().server_type() == kMainSceneCrossServer)
    {
        lobbyservcie::GameConnectToControllerRequest rq;
        g_gs->lobby_node()->CallMethod(lobbyservcieGameConnectToControllerMethoddesc , &rq);
    }
}


