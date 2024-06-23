#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "thread_local/thread_local_storage_centre.h"
#include "thread_local/thread_local_storage.h"
#include "player_common_system.h"
#include "type_alias/player_redis.h"
#include "type_alias/player_loading.h"

#include "common_proto/mysql_database_table.pb.h"
#include "common_proto/c2gate.pb.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    muduo_hiredis_ = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    muduo_hiredis_->connect();

    tls.global_registry.emplace<PlayerLoadingInfoList>(global_entity());

    auto& player_redis = tls.global_registry.emplace<PlayerRedisPtr>(global_entity());
    player_redis = std::make_unique<PlayerRedisPtr::element_type>(*muduo_hiredis_);
    player_redis->SetLoadCallback(PlayerCommonSystem::OnPlayerAsyncLoaded);
    player_redis->SetSaveCallback(PlayerCommonSystem::OnPlayerAsyncSaved);
}