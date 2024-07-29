#include "redis.h"

#include "muduo/net/EventLoop.h"

#include "player/player_node.h"
#include "thread_local/storage_game.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Init(muduo::net::InetAddress& server_addr)
{
    hiredis = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), server_addr);
    hiredis->connect();

    tlsGame.playerRedis = std::make_unique<PlayerRedis::element_type>(*hiredis);
    tlsGame.playerRedis->SetLoadCallback(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    tlsGame.playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}