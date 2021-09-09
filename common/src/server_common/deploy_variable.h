#ifndef COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
#define COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_

#include <string>

#include "src/server_common/server_type_id.h"

namespace deploy
{
    static const uint32_t kRedisPort = 6379;
    static const uint32_t kRegionServerBeginPort = 8000;
    static const uint32_t kServerBeginPort = 10000;
    static const uint32_t kGroup = 500;
    static const uint32_t kGroupServerSize = common::kServerSize - common::kServerDatabase;
    static const uint32_t kRegionBegin = kGroup;
    static const uint32_t kGroupBegin = kRegionBegin + kGroup;
    static const uint32_t kTotalGroupSize = kGroup * kGroupServerSize;
    static const uint32_t kLogicBegin = kGroupBegin + kTotalGroupSize;
    static const uint32_t kLogicBeginPort = kLogicBegin ;
}//namespace deploy_server

#endif//COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
