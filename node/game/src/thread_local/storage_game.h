#pragma once

#include "system/redis.h"
#include "comp/scene/nav.h"

#include "common_proto/mysql_database_table.pb.h"
#include "component_proto/player_async_comp.pb.h"

using PlayerRedis = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;
using PlayerEnterGSInfoList = std::unordered_map<Guid, EnterGsInfo>;
using GateSessionList = std::unordered_map<uint64_t, entt::entity>;

class ThreadLocalStorageGame
{
public:
	inline RedisSystem& redis_system() { return redis_system_; }
	inline PlayerRedis& player_redis() { return player_data_redis_system_; }
	inline PlayerEnterGSInfoList& aysnc_player_list() { return async_player_data_; }
	inline SceneNavs& scene_nav() { return scene_nav_; }
private:
	RedisSystem redis_system_;
	PlayerRedis player_data_redis_system_;
	PlayerEnterGSInfoList async_player_data_;
	SceneNavs scene_nav_;
};

extern thread_local ThreadLocalStorageGame tls_game;

