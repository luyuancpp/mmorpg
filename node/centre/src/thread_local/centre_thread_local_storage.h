#pragma once

#include "system/redis_system.h"

class CentreThreadLocalStorage
{
public:
    void Init();
private:
    RedisSystem redis_system_;
};

extern thread_local CentreThreadLocalStorage centre_tls;

