#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_node_system.h"
#include "thread_local/storage_common_logic.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize()
{
    playerRedis = std::make_unique<PlayerDataRedis::element_type>(*tlsCommonLogic.GetZoneRedis());
    playerRedis->SetLoadCallbackWithExtra(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}