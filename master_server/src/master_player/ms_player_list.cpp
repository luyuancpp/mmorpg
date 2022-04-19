#include "ms_player_list.h"

#include "src/game_logic/scene/scene.h"

thread_local PlayerListMap  g_players;

entt::entity PlayerList::GetPlayer(common::Guid guid)
{
    auto it = g_players.find(guid);
    if (it == g_players.end())
    {
        return entt::null;
    }
    return it->second.entity();
}

void PlayerList::LeaveGame(common::Guid guid)
{
    //todo 断线不能马上下线，这里之后会改
    auto it = g_players.find(guid);
    if ( it == g_players.end())
    {
        return;
    }
    LeaveSceneParam lsp;
    lsp.leave_entity_ = it->second.entity();
    g_scene_sys->LeaveScene(lsp);
	g_players.erase(it);
}

