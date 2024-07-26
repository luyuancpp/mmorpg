#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player_node.h"
#include "thread_local/storage.h"
#include "type_alias/player_loading.h"
#include "type_alias/player_redis.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    hiredis_ = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    hiredis_->connect();

    tls.globalRegistry.emplace<PlayerLoadingInfoList>(global_entity());

    auto& player_redis = tls.globalRegistry.emplace<PlayerRedis>(global_entity());
    player_redis = std::make_unique<PlayerRedis::element_type>(*hiredis_);
    player_redis->SetLoadCallback(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    player_redis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}