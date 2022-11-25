#include "lobby_service_replied.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/scene/scene.h"
#include "src/system/gs_scene_system.h"

void OnStartCrossGsReplied(const TcpConnectionPtr& conn, const StartCrossGsResponsePtr& replied, Timestamp timestamp)
{
    for (int32_t i = 0; i < replied->scenes_info_size(); ++i)
    {
        CreateSceneBySceneInfoP param;
        param.scene_info_ = replied->scenes_info(i);
        GsSceneSystem::CreateSceneByGuid(param);
    }
    LOG_DEBUG << replied->DebugString();
}


