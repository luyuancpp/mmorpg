#pragma once

#include "system/redis_system.h"

class CentreThreadLocalStorage
{
public:
    inline RedisSystem& redis_system() { return redis_system_; }

private:
    RedisSystem redis_system_;
};

extern thread_local CentreThreadLocalStorage centre_tls;

