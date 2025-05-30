#pragma once
#include "type_define/type_define.h"

#include "proto/centre/centre_service.pb.h"
#include "proto/db/mysql_database_table.pb.h"

using PlayerLoadingInfoList = std::unordered_map<Guid, CentrePlayerGameNodeEntryRequest>;

//todo scene node 直接往另外一个scene node 发玩家处理的消息