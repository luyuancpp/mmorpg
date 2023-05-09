#include "player_list.h"

#include "src/game_logic/scene/scene.h"

#include "src/thread_local/controller_thread_local_storage.h"

entt::entity ControllerPlayerSystem::GetPlayer(Guid guid)
{
    auto it = controller_tls.player_list().find(guid);
    if (it == controller_tls.player_list().end())
    {
        return entt::null;
    }
    return it->second;
}

EntityPtr ControllerPlayerSystem::GetPlayerPtr(Guid guid)
{
    auto it = controller_tls.player_list().find(guid);
    if (it == controller_tls.player_list().end())
    {
        return EntityPtr();
    }
    return it->second;
}

EntityPtr ControllerPlayerSystem::EnterGame(Guid guid)
{
    auto it = controller_tls.player_list().find(guid);
    if (it != controller_tls.player_list().end())
    {
        return it->second;
    }
    return controller_tls.player_list().emplace(guid, EntityPtr()).first->second;
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

