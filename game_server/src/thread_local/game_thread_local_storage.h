#pragma once

#include "src/system/redis_system.h"

class GameThreadLocalStorage
{
public:
	RedisSystem& redis_system() { return redis_system_; }
private:
	RedisSystem redis_system_;
};

extern thread_local GameThreadLocalStorage game_tls;

