#ifndef COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
#define COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_

#include "src/server_common/server_type_id.h"

namespace deploy_server
{
    static const uint32_t kRedisPort = 6379;
    static const uint32_t kGroup = 500;
    static const uint32_t kBeginPort = 2000;
    static const uint32_t kGroupServerSize = common::SERVER_ID_GROUP_SIZE - common::SERVER_REDIS + 1;
    static const uint32_t kTotalSize = kGroup * kGroupServerSize;
    static const uint32_t kLogicBeginId = kTotalSize + 1;
    static const uint32_t kLogicBeginPort = kBeginPort + kTotalSize + 1;
    static const std::string kIp = "127.0.0.1";
}//namespace deploy_server

#endif//COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
