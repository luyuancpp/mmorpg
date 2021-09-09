#ifndef COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
#define COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_

#include <string>

#include "src/server_common/server_type_id.h"

namespace deploy
{
    static const uint32_t kRedisPort = 6379;
    static const uint32_t kGroup = 500;
    static const uint32_t kBeginPort = 2000;
    static const uint32_t kGroupServerSize = common::kServerGroupSize - common::kServerRedis + 1;
    static const uint32_t kTotalSize = kGroup * kGroupServerSize;
    static const uint32_t kLogicBeginId = kTotalSize + 1;
    static const uint32_t kLogicBeginPort = kBeginPort + kTotalSize + 1;
    static const uint32_t kLogicBeginSnowflakeId = kGroup * 2 + 1;
    static const uint32_t kLogicSnowflakeIdReduceParam = kLogicBeginId - kLogicBeginSnowflakeId;
    
}//namespace deploy_server

#endif//COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
