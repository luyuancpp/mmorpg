#include "player_list.h"

#include "src/system/scene/scene_system.h"
#include "src/comp/scene_comp.h"
#include "src/thread_local/centre_thread_local_storage.h"

entt::entity ControllerPlayerSystem::GetPlayer(Guid player_id)
{
    const auto it = centre_tls.player_list().find(player_id);
    if (it == centre_tls.player_list().end())
    {
        return entt::null;
    }
    return it->second;
}

void ControllerPlayerSystem::LeaveGame(Guid guid)
{
    //todo 登录的时候leave
    //todo 断线不能马上下线，这里之后会改
    auto it = centre_tls.player_list().find(guid);
    if ( it == centre_tls.player_list().end())
    {
        return;
    }
    auto player = it->second;
    //没进入场景，只是登录，或者切换场景过程中
    if (nullptr == tls.registry.try_get<SceneEntity>(player))
    {
    }
    else
    {
		LeaveSceneParam lsp;
		lsp.leaver_ = player;
		ScenesSystem::LeaveScene(lsp);
    }
    
	centre_tls.player_list().erase(it);
}

