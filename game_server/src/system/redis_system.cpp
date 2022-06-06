#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player_common_system.h"

RedisSystem g_redis_system;

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    hiredis_ = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    hiredis_->connect();
 
    g_player_data_redis_system = std::make_unique<PlayerDataRedisSystemPtr::element_type>(*hiredis_);
    g_player_data_redis_system->SetLoadCallback(PlayerCommonSystem::OnAsyncLoadPlayerDb);
    g_player_data_redis_system->SetSaveCallback(PlayerCommonSystem::OnAsyncSavePlayerDb);
}