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

std::unordered_map<uint64_t, EntityPtr> g_async_player_data;

void OnAsyncLoadPlayerDatabase(Guid player_id, player_database& message)
{
    auto async_it = g_async_player_data.find(player_id);
    if (async_it == g_async_player_data.end())
    {
		LOG_INFO << "player disconnect" << player_id;
		return;
    }
    auto ret = g_players.emplace(player_id, EntityPtr());
	if (!ret.second)
	{
		LOG_ERROR << "server emplace error" << player_id;
		return;
	}
    // on loaded db
    entt::entity player = ret.first->second;
    registry.emplace<Guid>(player, player_id);
    registry.emplace<Vector3>(player, message.pos());
   	
    // on load db complete


    PlayerNetworkSystem::EnterGs(player, registry.get<EnterGsInfo>(async_it->second));
    g_async_player_data.erase(async_it);
}

