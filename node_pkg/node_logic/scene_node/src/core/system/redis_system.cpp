#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_node_system.h"
#include "thread_local/storage_game.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize(muduo::net::InetAddress& serverAddr)
{
    hiredis = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), serverAddr);
    hiredis->connect();

    tlsGame.playerRedis = std::make_unique<PlayerRedis::element_type>(*hiredis);
    tlsGame.playerRedis->SetLoadCallback(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    tlsGame.playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}