#pragma once

#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/network/node_info.h"
#include "src/network/gate_node.h"
#include "src/network/controller_node.h"
#include "src/system/redis_system.h"
#include "src/comp/gs_scene_comp.h"
#include "src/pb/pbc/common_proto/mysql_database_table.pb.h"

using PlayerDataRedisSystemPtr = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;
using PlayerEntityPtrList = std::unordered_map<uint64_t, entt::entity>;
using GateSessionList = std::unordered_map<uint64_t, entt::entity>;

class GameThreadLocalStorage
{
public:
	inline RedisSystem& redis_system() { return redis_system_; }
	inline ControllerNodes& controller_node() { return controller_node_; }
	inline GateNodes& gate_node() { return gate_nodes_; }
	inline GateSessionList& gate_sessions() { return gate_sessions_; }
	inline PlayerList& player_list() { return player_list_; }
	inline PlayerDataRedisSystemPtr& player_data_redis_system() { return player_data_redis_system_; }
	inline PlayerEntityPtrList& async_player_data() { return async_player_data_; }
	inline SceneNavs& scene_nav() { return g_scene_nav; }
private:
	RedisSystem redis_system_;
	ControllerNodes controller_node_;//controller 不会删除，因为不会和gs一样根据负载增加减少，只走底层的自动重连
	GateNodes gate_nodes_;
	GateSessionList gate_sessions_;
	PlayerList player_list_;
	PlayerDataRedisSystemPtr player_data_redis_system_;
	PlayerEntityPtrList async_player_data_;
	SceneNavs g_scene_nav;
};

extern thread_local GameThreadLocalStorage game_tls;

