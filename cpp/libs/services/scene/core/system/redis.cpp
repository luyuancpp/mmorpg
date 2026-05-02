#include "redis.h"

#include <cstdlib>

#include "muduo/net/EventLoop.h"

#include "player/system/player_lifecycle.h"
#include "thread_context/ecs_context.h"
#include "thread_context/redis_manager.h"

thread_local RedisSystem tlsRedisSystem;

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize(muduo::net::EventLoop* loop)
{
    loop_ = std::ref(*loop);
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

    // Periodically save all online players to Redis to bound the data-loss
    // window if the scene node crashes. Default 300s; set
    // SCENE_PLAYER_SAVE_INTERVAL_SECONDS=0 to disable. Each tick scans
    // tlsEcs.playerList and calls SavePlayerToRedis for every player.
    // Cost grows linearly with online count; the default interval is sized
    // to keep amortized Redis/Kafka load modest (e.g. 10k players over 300s
    // = ~33 saves/sec).
    int periodicSaveSec = 300;
    if (const char* env = std::getenv("SCENE_PLAYER_SAVE_INTERVAL_SECONDS"))
    {
        const int parsed = std::atoi(env);
        if (parsed >= 0)
        {
            periodicSaveSec = parsed;
        }
    }
    if (periodicSaveSec > 0)
    {
        const double interval = static_cast<double>(periodicSaveSec);
        periodicSaveTimerId_ = loop->runEvery(interval, []()
                                              {
            if (tlsEcs.playerList.empty())
            {
                return;
            }
            const std::size_t before = tlsEcs.playerList.size();
            for (const auto& [playerId, player] : tlsEcs.playerList)
            {
                if (!tlsEcs.actorRegistry.valid(player))
                {
                    continue;
                }
                PlayerLifecycleSystem::SavePlayerToRedis(player);
            }
            LOG_INFO << "[RedisSystem] Periodic save scanned " << before << " online players"; });
        periodicSaveTimerActive_ = true;
        LOG_INFO << "[RedisSystem] Periodic player save enabled, interval=" << periodicSaveSec << "s";
    }
    else
    {
        LOG_INFO << "[RedisSystem] Periodic player save disabled (SCENE_PLAYER_SAVE_INTERVAL_SECONDS=0)";
    }
}

void RedisSystem::Shutdown()
{
    // Cancel timers BEFORE dropping playerRedis so a pending fire cannot deref
    // a half-destroyed unique_ptr. Guard on loop_ in case Initialize() never ran.
    if (loop_.has_value())
    {
        muduo::net::EventLoop &loop = loop_->get();
        if (retryTimerActive_)
        {
            loop.cancel(retryTimerId_);
            retryTimerActive_ = false;
        }
        if (snapshotTimerActive_)
        {
            loop.cancel(snapshotTimerId_);
            snapshotTimerActive_ = false;
        }
        if (periodicSaveTimerActive_)
        {
            loop.cancel(periodicSaveTimerId_);
            periodicSaveTimerActive_ = false;
        }
    }
    playerRedis.reset();
}

RedisSystem::~RedisSystem()
{
    Shutdown();
}
