#ifndef GAME_SNOW_FLAKE_H
#define GAME_SNOW_FLAKE_H

#include <atomic>
#include <chrono>
#include <cstdint>

#include "src/common_type/common_type.h"

//https://dev.mysql.com/doc/refman/8.0/en/miscellaneous-functions.html#function_uuid

namespace common
{
class SnowFlake
{
public:
    static const int32_t kServerIdByte = 16;
    static const int32_t kIncrementedVariable = 32;

    SnowFlake(const SnowFlake&) = delete;
    SnowFlake& operator=(const SnowFlake&) = delete;

    explicit SnowFlake()
        :server_id_(0),
        server_id_flag_(0)
    {
    }

    GameGuid Generate()
    {
        return incremented_variable_.fetch_add(1);
        /*return GameGuid(server_id_flag_ ^ incremented_variable_.fetch_add(1),
               GetNow());*/
    }

    void SetServerId(uint16_t server_id)
    {
        server_id_ = server_id;
        server_id_flag_ = server_id_flag_ << kServerIdByte << kIncrementedVariable;
    }

    time_t GetNow()
    {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }
private:
    uint16_t server_id_{ 0 };
    uint64_t server_id_flag_{ 0 };
    std::atomic_uint32_t incremented_variable_{ 0 };    
};
}//namespace common


#endif // GAME_SNOW_FLAKE_H