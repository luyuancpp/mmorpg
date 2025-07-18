#include "redis_system.h"

#include "muduo/net/EventLoop.h"

#include "player/system/player_node_system.h"
#include "thread_local/storage_game.h"
#include "thread_local/storage_common_logic.h"

using namespace muduo;
using namespace muduo::net;

void RedisSystem::Initialize()
{
    tlsGame.playerRedis = std::make_unique<PlayerDataRedis::element_type>(*tlsCommonLogic.GetZoneRedis());
    tlsGame.playerRedis->SetLoadCallbackWithExtra(PlayerNodeSystem::HandlePlayerAsyncLoaded);
    tlsGame.playerRedis->SetSaveCallback(PlayerNodeSystem::HandlePlayerAsyncSaved);
}