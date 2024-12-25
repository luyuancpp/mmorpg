#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "game_logic/player/util/player_node_system.h"
#include "thread_local/storage.h"
#include "type_alias/player_loading.h"
#include "type_alias/player_redis.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize(muduo::net::InetAddress& server_addr)
{
    hiredis = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    hiredis->connect();

    tls.globalRegistry.emplace<PlayerLoadingInfoList>(GlobalEntity());

    auto& playerRedis = tls.globalRegistry.emplace<PlayerRedis>(GlobalEntity());
    playerRedis = std::make_unique<PlayerRedis::element_type>(*hiredis);
    playerRedis->SetLoadCallback(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}