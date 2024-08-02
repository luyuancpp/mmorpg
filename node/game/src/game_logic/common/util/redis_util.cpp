#include "redis_util.h"

#include "muduo/net/EventLoop.h"

#include "game_logic/player/util/player_node_util.h"
#include "thread_local/storage_game.h"

using namespace muduo;
using namespace muduo::net;

void RedisUtil::Initialize(muduo::net::InetAddress& serverAddr)
{
    hiredis = std::make_unique<HiredisPtr::element_type>(EventLoop::getEventLoopOfCurrentThread(), serverAddr);
    hiredis->connect();

    tlsGame.playerRedis = std::make_unique<PlayerRedis::element_type>(*hiredis);
    tlsGame.playerRedis->SetLoadCallback(PlayerNodeUtil::HandlePlayerAsyncLoaded);
    tlsGame.playerRedis->SetSaveCallback(PlayerNodeUtil::HandlePlayerAsyncSaved);
}