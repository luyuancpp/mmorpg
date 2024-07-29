#pragma once

#include "system/redis.h"
#include "comp/scene/nav.h"

#include "proto/common/mysql_database_table.pb.h"
#include "proto/logic/component/player_async_comp.pb.h"
#include "proto/logic/component/frame_comp.pb.h"

using PlayerRedis = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;
using PlayerEnterGSInfoList = std::unordered_map<Guid, EnterGsInfo>;

class ThreadLocalStorageGame
{
public:
	RedisSystem redis;
	PlayerRedis playerRedis;
	PlayerEnterGSInfoList asyncPlayerList;
	SceneNavs sceneNav;
	FrameTime frameTime;
};

extern thread_local ThreadLocalStorageGame tlsGame;

