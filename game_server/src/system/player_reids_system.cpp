#include "player_reids_system.h"

#include "src/comp/player_list.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_session.h"
#include "src/network/gate_node.h"
#include "src/network/ms_node.h"
#include "src/network/session.h"
#include "src/system/player_network_system.h"

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
    auto session_id = p_s_it->second;
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
        LOG_ERROR << "server emplace error" << player_id;
        return;
    }
    entt::entity player = ret.first->second;
    registry.emplace<Guid>(player, player_id);
    registry.emplace<Vector3>(player, message.pos());
   	
    // on load db complete

    //enter gs
    auto& enter_gs_info = registry.get<EnterGsInfo>(s_p_it->second);
    PlayerNetworkSystem::EnterGs(player, session_id, enter_gs_info.ms_node_id());

    //todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
   
}

