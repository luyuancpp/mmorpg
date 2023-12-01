#include "player_list.h"

#include "src/game_logic/scene/scene_system.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/thread_local/controller_thread_local_storage.h"

entt::entity ControllerPlayerSystem::GetPlayer(Guid player_id)
{
    const auto it = controller_tls.player_list().find(player_id);
    if (it == controller_tls.player_list().end())
    {
        return entt::null;
    }
    return it->second;
}

void ControllerPlayerSystem::LeaveGame(Guid guid)
{
    //todo 登录的时候leave
    //todo 断线不能马上下线，这里之后会改
    auto it = controller_tls.player_list().find(guid);
    if ( it == controller_tls.player_list().end())
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
    
	controller_tls.player_list().erase(it);
}

