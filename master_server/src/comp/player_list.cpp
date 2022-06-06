#include "player_list.h"

#include "src/game_logic/scene/scene.h"

thread_local PlayerListMap  g_players;

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
    //todo 断线不能马上下线，这里之后会改
    auto it = g_players.find(guid);
    if ( it == g_players.end())
    {
        return;
    }
    auto player = it->second;
    if (nullptr == registry.try_get<SceneEntity>(player))
    {
    }
    LeaveSceneParam lsp;
    lsp.leaver_ = player;
    ScenesSystem::GetSingleton().LeaveScene(lsp);
	g_players.erase(it);
}

