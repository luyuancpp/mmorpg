#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "src/thread_local/game_thread_local_storage.h"
#include "player_common_system.h"


using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    hiredis_ = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    hiredis_->connect();
 
    game_tls.player_data_redis_system() = std::make_unique<PlayerDataRedisSystemPtr::element_type>(*hiredis_);
    game_tls.player_data_redis_system()->SetLoadCallback(PlayerCommonSystem::OnAsyncLoadPlayerDb);
    game_tls.player_data_redis_system()->SetSaveCallback(PlayerCommonSystem::OnAsyncSavePlayerDb);
}