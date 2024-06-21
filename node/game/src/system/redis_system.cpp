#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "thread_local/game_thread_local_storage.h"
#include "player_common_system.h"


using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    muduo_hiredis_ = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    muduo_hiredis_->connect();

    game_tls.player_redis() = std::make_unique<PlayerRedisPtr::element_type>(*muduo_hiredis_);
    game_tls.player_redis()->SetLoadCallback(PlayerCommonSystem::OnPlayerAsyncLoaded);
    game_tls.player_redis()->SetSaveCallback(PlayerCommonSystem::OnPlayerAsyncSaved);
}