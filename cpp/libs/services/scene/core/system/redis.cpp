#include "redis.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_node.h"
#include "threading/redis_manager.h"

thread_local RedisSystem tlsRedisSystem;

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize()
{
    playerRedis = std::make_unique<PlayerDataRedis::element_type>(*tlsReids.GetZoneRedis());
    playerRedis->SetLoadCallbackWithExtra(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}