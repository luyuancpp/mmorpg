#pragma once

#include "game_logic/core/utils/redis_util.h"
#include "game_logic/scene/comp/nav_comp.h"
#include "proto/common/mysql_database_table.pb.h"
#include "proto/logic/component/frame_comp.pb.h"
#include "proto/logic/component/player_async_comp.pb.h"
#include "util/node_id_generator.h"

using PlayerRedis = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;

using NodeIdGenerator32BitId = NodeIdGenerator<uint64_t, 32>;

class ThreadLocalStorageGame
{
public:
	RedisSystem redis;
	PlayerRedis playerRedis;
	std::unordered_map<Guid, PlayerGameNodeEnteryInfoPBComponent> playerNodeEntryInfoList;
	SceneNavMapComp sceneNav;
	FrameTime frameTime;
	NodeIdGenerator32BitId buffIdGenerator;
	NodeIdGenerator32BitId skillIdGenerator;
	
};

extern thread_local ThreadLocalStorageGame tlsGame;

