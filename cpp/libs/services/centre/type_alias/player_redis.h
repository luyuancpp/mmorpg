#pragma once

#include "infra/storage/redis_client/redis_client.h"

#include "proto/common/database/mysql_database_table.pb.h"
#include "core/utils/registry/game_registry.h"
#include <engine/threading/registry_manager.h>
#include <engine/threading/entity_manager.h>

using PlayerCentreDataRedis = std::unique_ptr<MessageAsyncClient<Guid, player_centre_database>>;

inline PlayerCentreDataRedis& GetPlayerCentreDataRedis() {
	return tlsRegistryManager.globalRegistry.get<PlayerCentreDataRedis>(GlobalEntity());
}

