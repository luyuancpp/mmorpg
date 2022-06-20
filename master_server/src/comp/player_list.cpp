#include "player_list.h"

#include "src/game_logic/scene/scene.h"

thread_local PlayerListMap  g_players;

PlayerList* g_player_list = nullptr;

entt::entity PlayerList::GetPlayer(Guid guid)
{
    auto it = g_players.find(guid);
    if (it == g_players.end())
    {
        return entt::null;
    }
    return it->second;
}

EntityPtr PlayerList::GetPlayerPtr(Guid guid)
{
    auto it = g_players.find(guid);
    if (it == g_players.end())
    {
        return EntityPtr();
    }
    return it->second;
}

EntityPtr PlayerList::EnterGame(Guid guid)
{
    auto it = g_players.find(guid);
    if (it != g_players.end())
    {
        return it->second;
    }
    return g_players.emplace(guid, EntityPtr()).first->second;
}

void PlayerList::LeaveGame(Guid guid)
{
    //todo ��¼��ʱ��leave
    //todo ���߲����������ߣ�����֮����
    auto it = g_players.find(guid);
    if ( it == g_players.end())
    {
        return;
    }
    auto player = it->second;
    //û���볡����ֻ�ǵ�¼�������л�����������
    if (nullptr == registry.try_get<SceneEntity>(player))
    {
    }
    else
    {
		LeaveSceneParam lsp;
		lsp.leaver_ = player;
		ScenesSystem::GetSingleton().LeaveScene(lsp);
    }
    
	g_players.erase(it);
}

