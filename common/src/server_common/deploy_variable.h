#ifndef COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
#define COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_

#include <string>

#include "src/server_common/server_type_id.h"

namespace deploy
{
    static const uint32_t kRedisPort = 6379;
    static const uint32_t kRegionServerBeginPort = 8000;
    static const uint32_t kServerBeginPort = 10000;
    static const uint32_t kDatabeseServerBeginPort = 10000;
    static const uint32_t kLoginServerBeginPort = 12000;
    static const uint32_t kMasterServerBeginPort = 14000;
    static const uint32_t kGatewayServerBeginPort = 16000;
    static const uint32_t kGameServerBeginPort = 18000;
    static const uint32_t kGroup = 500;
    static const uint32_t kGroupServerSize = common::kServerSize - common::kServerDatabase;

}//namespace deploy_server

#endif//COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
