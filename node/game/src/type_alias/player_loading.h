#pragma once
#include "type_define/type_define.h"

#include "proto/common/centre_service.pb.h"
#include "proto/common/mysql_database_table.pb.h"

using PlayerLoadingInfoList = std::unordered_map<Guid, CentrePlayerGameNodeEntryRequest>;
