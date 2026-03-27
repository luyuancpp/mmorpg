#pragma once

#include "redis_client/redis_client.h"

#include "proto/db/mysql_database_table.pb.h"
#include "core/utils/registry/game_registry.h"


using CentrePlayerDataRedis = std::unique_ptr<MessageAsyncClient<Guid, player_centre_database>>;

inline CentrePlayerDataRedis& GetGlobalPlayerRedis() {
	return tlsEcs.globalRegistry.get<CentrePlayerDataRedis>(tlsEcs.GlobalEntity());
}