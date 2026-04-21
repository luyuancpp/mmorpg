#pragma once

#include <memory>

#include "engine/infra/storage/redis_client/redis_client.h"
#include "proto/common/database/player_cache.pb.h"
#include <muduo/net/TimerId.h>

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
    ~RedisSystem();

    RedisSystem(const RedisSystem&) = delete;
    RedisSystem& operator=(const RedisSystem&) = delete;
    RedisSystem(RedisSystem &&) = delete;
    RedisSystem &operator=(RedisSystem &&) = delete;

    PlayerDataRedis& GetPlayerDataRedis() {
		return playerRedis;
	}

    void Initialize(muduo::net::EventLoop *loop);

    // Cancel timers and drop the playerRedis instance. Idempotent. Call before
    // the EventLoop is destroyed when ordering matters (the destructor also
    // calls this).
    void Shutdown();

private:
	PlayerDataRedis playerRedis;
    muduo::net::EventLoop *loop_ = nullptr;
    muduo::net::TimerId retryTimerId_;
    muduo::net::TimerId snapshotTimerId_;
    bool retryTimerActive_ = false;
    bool snapshotTimerActive_ = false;
};


extern thread_local RedisSystem tlsRedisSystem;
