#pragma once

#include "game_logic/redis/util/redis_util.h"

class ThreadLocalStorageCentre
{
public:
    inline RedisUtil& redis_system() { return redis_system_; }

private:
    RedisUtil redis_system_;
};

extern thread_local ThreadLocalStorageCentre tls_centre;

