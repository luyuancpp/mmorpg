#include "redis.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_lifecycle.h"

#include "type_alias/player_redis.h"
#include "engine/threading/redis_manager.h"

thread_local RedisSystem tlsRedisSystem;

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize()
{
    auto& playerRedis = tlsRegistryManager.globalRegistry.emplace<PlayerCentreDataRedis>(GlobalEntity());
    playerRedis = std::make_unique<PlayerCentreDataRedis::element_type>(*tlsReids.GetZoneRedis());
    playerRedis->SetLoadCallbackWithExtra(PlayerLifecycleSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetSaveCallback(PlayerLifecycleSystem::HandlePlayerAsyncSaved);
}