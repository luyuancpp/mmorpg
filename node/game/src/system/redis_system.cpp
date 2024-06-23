#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "thread_local/thread_local_storage_game.h"
#include "player_common_system.h"


using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    muduo_hiredis_ = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    muduo_hiredis_->connect();

    tls_game.player_redis() = std::make_unique<PlayerRedisPtr::element_type>(*muduo_hiredis_);
    tls_game.player_redis()->SetLoadCallback(PlayerCommonSystem::OnPlayerAsyncLoaded);
    tls_game.player_redis()->SetSaveCallback(PlayerCommonSystem::OnPlayerAsyncSaved);
}