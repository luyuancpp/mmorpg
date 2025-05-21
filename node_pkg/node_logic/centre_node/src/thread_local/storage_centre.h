#pragma once

#include "redis/system/redis_system.h"

class ThreadLocalStorageCentre
{
public:
    inline RedisSystem& GetRedisSystem() { return redis_system_; }

private:
    RedisSystem redis_system_;
};

extern thread_local ThreadLocalStorageCentre tls_centre;

