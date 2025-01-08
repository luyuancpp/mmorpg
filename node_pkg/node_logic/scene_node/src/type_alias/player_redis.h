#pragma once

#include "redis_client/redis_client.h"

#include "proto/common/mysql_database_table.pb.h"

using PlayerRedis = std::unique_ptr<MessageAsyncClient<Guid, player_centre_database>>;