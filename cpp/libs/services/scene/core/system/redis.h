#pragma once

#include <memory>

#include "engine/infra/storage/redis_client/redis_client.h"
#include "proto/common/database/player_cache.pb.h"

using PlayerDataRedis = std::unique_ptr<MessageAsyncClient<Guid, PlayerAllData>>;

namespace muduo
{
    namespace net
    {
        class EventLoop;
    }
}

class RedisSystem
{
public:
    RedisSystem() = default;

    RedisSystem(const RedisSystem&) = delete;
    RedisSystem& operator=(const RedisSystem&) = delete;

	PlayerDataRedis& GetPlayerDataRedis() {
		return playerRedis;
	}

    void Initialize(muduo::net::EventLoop *loop);

private:
	PlayerDataRedis playerRedis;
};


extern thread_local RedisSystem tlsRedisSystem;
