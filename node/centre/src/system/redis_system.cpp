#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "thread_local/centre_thread_local_storage.h"
#include "thread_local/thread_local_storage.h"

#include "player_common_system.h"

#include "common_proto/mysql_database_table.pb.h"
#include "common_proto/c2gate.pb.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    muduo_hiredis_ = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    muduo_hiredis_->connect();

    using PlayerRedisPtr = std::unique_ptr<MessageAsyncClient<Guid, player_centre_database>>;

    auto& player_redis = tls.global_registry.emplace<PlayerRedisPtr>(global_entity(),
        std::make_unique<PlayerRedisPtr::element_type>(*muduo_hiredis_));
    player_redis->SetLoadCallback(PlayerCommonSystem::OnPlayerAsyncLoaded);
    player_redis->SetSaveCallback(PlayerCommonSystem::OnPlayerAsyncSaved);

    using PlayerLoadingListInfo = std::unordered_map<Guid, EnterGameRequest>;
    tls.global_registry.emplace<PlayerLoadingListInfo>(global_entity());
    

}