#ifndef COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
#define COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_

#include <string>

namespace deploy
{
    static const uint32_t kRedisPort = 6379;
    static const uint32_t kRSBeginPort = 8000;
    static const uint32_t kDSBeginPort = 20000;
    static const uint32_t kLSBeginPort = 12000;
    static const uint32_t kMSBeginPort = 14000;
    static const uint32_t kGateSBeginPort = 16000;
    static const uint32_t kGSBeginPort = 18000;
    static const uint32_t kGroup = 500;

}//namespace deploy_server

#endif//COMMON_SRC_SERVER_COMMON_DEPLOY_VARIABLE_H_
