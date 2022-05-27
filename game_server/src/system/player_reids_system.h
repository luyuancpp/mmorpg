#pragma once

#include "src/redis_client/redis_client.h"

#include "src/pb/pbc/mysql_database_table.pb.h"

using PlayerDataRedisSystemPtr = std::unique_ptr<MessageAsyncClient<Guid, player_database>>;
extern PlayerDataRedisSystemPtr g_player_data_redis_system;

void OnAsyncLoadPlayerDatabase(Guid guid, player_database& message);//如果异步加载过程中玩家断开链接了？会不会造成数据覆盖