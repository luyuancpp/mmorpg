#ifndef COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_
#define COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_

#include <cstdint>

namespace common
{
enum ServerTypeIdEnum : uint32_t
{
    kServerRedis = 0,
    kServerDatabase = 1,
    kServerMaster = 2,
    kServerLogin = 3,
    kServerGateway = 4,
    kServerCurrent = 5,
    kServerGroupSize = 8,
};

}//namespace common

#endif // !COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_
