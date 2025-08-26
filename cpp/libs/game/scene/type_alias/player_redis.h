#pragma once

#include "redis_client/redis_client.h"

#include "proto/db/mysql_database_table.pb.h"
#include "util/registry/game_registry.h"


using PlayerDataRedis = std::unique_ptr<MessageAsyncClient<Guid, player_centre_database>>;

inline PlayerDataRedis& GetGlobalPlayerRedis() {
	return tlsRegistryManager.globalRegistry.get<PlayerDataRedis>(GlobalEntity());
}