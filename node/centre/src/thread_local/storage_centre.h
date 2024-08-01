#pragma once

#include "system/redis_system.h"

class ThreadLocalStorageCentre
{
public:
    inline RedisUtil& redis_system() { return redis_system_; }

private:
    RedisUtil redis_system_;
};

extern thread_local ThreadLocalStorageCentre tls_centre;

