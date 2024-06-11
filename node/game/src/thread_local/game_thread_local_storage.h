#pragma once

#include "src/system/redis_system.h"
#include "src/comp/gs_scene_comp.h"

#include "common_proto/mysql_database_table.pb.h"
#include "component_proto/player_async_comp.pb.h"

using PlayerDataRedisSystemPtr = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;
using PlayerEnterGSInfoList = std::unordered_map<Guid, EnterGsInfo>;
using GateSessionList = std::unordered_map<uint64_t, entt::entity>;

class GameThreadLocalStorage
{
public:
	inline RedisSystem& redis_system() { return redis_system_; }
	inline PlayerDataRedisSystemPtr& player_data_redis_system() { return player_data_redis_system_; }
	inline PlayerEnterGSInfoList& async_player_data() { return async_player_data_; }
	inline SceneNavs& scene_nav() { return scene_nav_; }
private:
	RedisSystem redis_system_;
	PlayerDataRedisSystemPtr player_data_redis_system_;
	PlayerEnterGSInfoList async_player_data_;
	SceneNavs scene_nav_;
};

extern thread_local GameThreadLocalStorage game_tls;

