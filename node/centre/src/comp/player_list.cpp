#include "player_list.h"

#include "src/system/scene/scene_system.h"
#include "src/comp/scene_comp.h"
#include "src/thread_local/centre_thread_local_storage.h"

void ControllerPlayerSystem::LeaveGame(entity player)
{
    //todo 登录的时候leave
    //todo 断线不能马上下线，这里之后会改
    //没进入场景，只是登录，或者切换场景过程中
    if (nullptr == tls.player_registry.try_get<SceneEntity>(player))
    {
    }
    else
    {
        ScenesSystem::LeaveScene({ player });
    }
    tls.player_registry.destroy(player);
}

