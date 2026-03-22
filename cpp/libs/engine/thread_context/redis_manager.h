#pragma once


#include "engine/core/type_define/type_define.h"
#include <muduo/contrib/hiredis/hiredis.h>

class RedisManager
{
public:
	using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;

	HiredisPtr& GetZoneRedis()
	{
		return zoneRedis_;
	}

private:
	HiredisPtr zoneRedis_;
};

extern thread_local RedisManager tlsRedis;


