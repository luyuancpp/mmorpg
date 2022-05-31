#include "player_reids_system.h"

#include "src/comp/player_list.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_session.h"
#include "src/system/player_scene_system.h"

#include "component_proto/player_async_comp.pb.h"

PlayerDataRedisSystemPtr g_player_data_redis_system;

void OnAsyncLoadPlayerDatabase(Guid player_id, player_database& message)
{
    auto p_s_it = g_player_session_map.find(player_id);
    if (p_s_it == g_player_session_map.end())
    {
        LOG_INFO << " player off lie or change gs player" << player_id;
        return;
    }
    auto s_p_it = g_gate_sessions.find(p_s_it->second);
    if (s_p_it == g_gate_sessions.end())
    {
        LOG_INFO << " player off lie or change gs player" << player_id;
        return;
    }

    // on load db
    auto ret = g_players.emplace(player_id, EntityPtr());
    if (!ret.second)
    {
        LOG_ERROR << "server emplace erro" << player_id;
        return;
    }
    entt::entity player = ret.first->second;
    registry.emplace<Guid>(player, player_id);
    registry.emplace<Vector3>(player, message.pos());
   	
    // on load db complete

    auto& enter_info = registry.get<EnterSceneInfo>(s_p_it->second);
    PlayerSceneSystem::EnterScene(player, enter_info, p_s_it->second);
}

