#include "centre_player_system.h"

#include "src/system/scene/scene_system.h"
#include "src/comp/scene_comp.h"
#include "src/thread_local/centre_thread_local_storage.h"
#include "src/util/defer.h"

void CenterPlayerSystem::LeaveGame(Guid player_uid)
{
    //todo 登录的时候leave
    //todo 断线不能马上下线，这里之后会改
    //没进入场景，只是登录，或者切换场景过程中
    defer(cl_tls.player_list().erase(player_uid));
    auto player = cl_tls.get_player(player_uid);
    if (!tls.registry.valid(player))
    {
        return;
    }
    if (nullptr == tls.registry.try_get<SceneEntity>(player))
    {
    }
    else
    {
        ScenesSystem::LeaveScene({ player });
    }
}

