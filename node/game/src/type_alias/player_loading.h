#pragma once
#include "type_define/type_define.h"

#include "common_proto/centre_service.pb.h"
#include "common_proto/mysql_database_table.pb.h"

using PlayerLoadingInfoList = std::unordered_map<Guid, EnterGameL2Ctr>;
