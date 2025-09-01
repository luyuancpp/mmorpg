#pragma once

#include <memory>

#include "base/infra/storage/redis_client/redis_client.h"
#include "proto/db/mysql_database_table.pb.h"

using PlayerDataRedis = std::unique_ptr<MessageAsyncClient<Guid, PlayerAllData>>;

class RedisSystem
{
public:
    RedisSystem() = default;

    // 禁止拷贝和移动，确保单例唯一性
    RedisSystem(const RedisSystem&) = delete;
    RedisSystem& operator=(const RedisSystem&) = delete;

	PlayerDataRedis& GetPlayerDataRedis() {
		return playerRedis;
	}

    void Initialize();

private:
	PlayerDataRedis playerRedis;
};


extern thread_local RedisSystem tlsRedisSystem;
