#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_node_system.h"
#include "thread_local/storage.h"
#include "type_alias/player_loading.h"
#include "type_alias/player_redis.h"
#include "thread_local/storage_common_logic.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize()
{
    tls.globalRegistry.emplace<PlayerLoadingInfoList>(GlobalEntity());

    auto& playerRedis = tls.globalRegistry.emplace<PlayerRedis>(GlobalEntity());
    playerRedis = std::make_unique<PlayerRedis::element_type>(*tlsCommonLogic.GetZoneRedis());
    playerRedis->SetLoadCallback(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}