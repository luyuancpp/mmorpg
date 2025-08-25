#pragma once

#include "base/infra/storage/redis_client/redis_client.h"

#include "proto/db/mysql_database_table.pb.h"
#include "util/game_registry.h"
#include <base/threading/registry_manager.h>
#include <base/threading/entity_manager.h>

using PlayerCentreDataRedis = std::unique_ptr<MessageAsyncClient<Guid, player_centre_database>>;

inline PlayerCentreDataRedis& GetPlayerCentreDataRedis() {
	return tlsRegistryManager.globalRegistry.get<PlayerCentreDataRedis>(GlobalEntity());
}

