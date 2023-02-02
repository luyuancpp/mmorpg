#include "player_list.h"

#include "src/game_logic/scene/scene.h"

#include "src/game_logic/thread_local/thread_local_storage.h"
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

bool ControllerPlayerSystem::HasPlayer(Guid guid)
{
	return controller_tls.player_list().find(guid) != controller_tls.player_list().end(); 
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
    //todo ��¼��ʱ��leave
    //todo ���߲����������ߣ�����֮����
    auto it = controller_tls.player_list().find(guid);
    if ( it == controller_tls.player_list().end())
    {
        return;
    }
    auto player = it->second;
    //û���볡����ֻ�ǵ�¼�������л�����������
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

