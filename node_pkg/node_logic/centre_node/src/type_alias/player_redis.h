#pragma once

#include "redis_client/redis_client.h"

#include "proto/db/mysql_database_table.pb.h"
#include "util/game_registry.h"
#include "thread_local/storage.h"
using PlayerRedis = std::unique_ptr<MessageAsyncClient<Guid, player_centre_database>>;



inline PlayerRedis& GetGlobalPlayerRedis() {
	return tls.globalRegistry.get<PlayerRedis>(GlobalEntity());
}