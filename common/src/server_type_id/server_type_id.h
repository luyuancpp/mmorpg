#ifndef COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_
#define COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_

#include <cstdint>

namespace common
{
enum ServerTypeIdEnum : uint32_t
{
    SERVER_REDIS = 0,
    SERVER_DATABASE = 1,
    SERVER_MASTER = 2,
    SERVER_LOGIN = 3,
    SERVER_GATEWAY = 4,
    SERVER_ID_RESERVE = 7,
    SERVER_ID_GROUP_SIZE = 8,
};

}//namespace common

#endif // !COMMON_SRC_SERVER_TYPE_ID_SERVER_TYPE_ID_H_
