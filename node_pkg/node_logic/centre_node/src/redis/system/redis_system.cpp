#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_node_system.h"
#include "thread_local/storage.h"
#include "type_alias/player_redis.h"
#include "thread_local/storage_common_logic.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize()
{
    auto& playerRedis = tls.globalRegistry.emplace<PlayerCentreDataRedis>(GlobalEntity());
    playerRedis = std::make_unique<PlayerCentreDataRedis::element_type>(*tlsCommonLogic.GetZoneRedis());
    playerRedis->SetLoadCallbackWithExtra(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}