#ifndef COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_
#define COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_

#include <cstdint>

namespace common
{
enum ServerTypeIdEnum : uint32_t
{
    kServerDatabase = 0,
    kServerMaster = 1,
    kServerLogin = 2,
    kServerGateway = 3,
    kServerSize = 5,
};

enum eRegionServerTypeIdEnum : uint32_t
{
    kServerCross = 0,
    kRegionServerSize = 4,
};

}//namespace common

#endif // !COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_
