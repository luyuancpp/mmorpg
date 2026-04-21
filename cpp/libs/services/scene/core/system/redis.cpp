#include "redis.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_lifecycle.h"
#include "thread_context/redis_manager.h"

thread_local RedisSystem tlsRedisSystem;

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize(muduo::net::EventLoop* loop)
{
    loop_ = loop;
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
    retryTimerId_ = loop->runEvery(kRetryIntervalSec, [this]()
                                   {
        if (playerRedis)
        {
            playerRedis->RetryDuePending();
        } });
    retryTimerActive_ = true;

    // Periodically log a snapshot of internal queue sizes so operators can spot
    // a Redis stall (rising pending_loads / pending_saves) without enabling
    // per-call DEBUG logs. No output when all queues are empty.
    static constexpr double kQueueSnapshotIntervalSec = 30.0;
    snapshotTimerId_ = loop->runEvery(kQueueSnapshotIntervalSec, [this]()
                                      {
        if (playerRedis)
        {
            playerRedis->LogQueueSnapshot("RedisSystem");
        } });
    snapshotTimerActive_ = true;
}

void RedisSystem::Shutdown()
{
    // Cancel timers BEFORE dropping playerRedis so a pending fire cannot deref
    // a half-destroyed unique_ptr. Guard on loop_ in case Initialize() never ran.
    if (loop_ != nullptr)
    {
        if (retryTimerActive_)
        {
            loop_->cancel(retryTimerId_);
            retryTimerActive_ = false;
        }
        if (snapshotTimerActive_)
        {
            loop_->cancel(snapshotTimerId_);
            snapshotTimerActive_ = false;
        }
    }
    playerRedis.reset();
}

RedisSystem::~RedisSystem()
{
    Shutdown();
}
