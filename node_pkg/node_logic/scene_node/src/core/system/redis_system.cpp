#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_node_system.h"
#include "thread_local/redis_manager.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize()
{
    playerRedis = std::make_unique<PlayerDataRedis::element_type>(*tlsReids.GetZoneRedis());
    playerRedis->SetLoadCallbackWithExtra(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}