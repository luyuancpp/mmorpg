#pragma once

#include "src/system/redis_system.h"

struct GameThreadLocalStorage
{
	RedisSystem redis_system;
};

extern thread_local GameThreadLocalStorage game_tls;

