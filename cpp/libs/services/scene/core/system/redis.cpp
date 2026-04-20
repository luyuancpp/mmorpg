#include "redis.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_lifecycle.h"
#include "thread_context/redis_manager.h"

thread_local RedisSystem tlsRedisSystem;

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize(muduo::net::EventLoop* loop)
{
    playerRedis = std::make_unique<PlayerDataRedis::element_type>(tlsRedis.GetZoneRedis());
    playerRedis->SetLoadCallback(PlayerLifecycleSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetLoadFailedCallback(PlayerLifecycleSystem::HandlePlayerAsyncLoadFailed);
    playerRedis->SetSaveCallback(PlayerLifecycleSystem::HandlePlayerAsyncSaved);

    tlsRedis.SetReconnectCallback([this]()
                                  {
        LOG_INFO << "Redis reconnected, retrying pending player loads";
        if (playerRedis)
        {
            playerRedis->RetryPendingLoads();
        } });

    // Periodically retry loads that got NIL (data not yet written to Redis)
    static constexpr double kRetryIntervalSec = 2.0;
    loop->runEvery(kRetryIntervalSec, [this]()
    {
        if (playerRedis)
        {
            playerRedis->RetryPendingLoads();
        }
    });
}
