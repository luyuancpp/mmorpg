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
            playerRedis->OnReconnected();
        } });

    // Periodically drain NIL-pending load retries and pending save retries
    // whose backoff has elapsed. MUST NOT touch in-flight loading_queue_ —
    // only pending queues — otherwise load_callback_ can fire twice per key.
    static constexpr double kRetryIntervalSec = 1.0;
    loop->runEvery(kRetryIntervalSec, [this]()
    {
        if (playerRedis)
        {
            playerRedis->RetryDuePending();
        }
    });

    // Periodically log a snapshot of internal queue sizes so operators can spot
    // a Redis stall (rising pending_loads / pending_saves) without enabling
    // per-call DEBUG logs. No output when all queues are empty.
    static constexpr double kQueueSnapshotIntervalSec = 30.0;
    loop->runEvery(kQueueSnapshotIntervalSec, [this]()
                   {
        if (playerRedis)
        {
            playerRedis->LogQueueSnapshot("RedisSystem");
        } });
}
