#pragma once


#include "type_define/type_define.h"
#include <muduo/contrib/hiredis/hiredis.h>

class ThreadLocalStorageCommonLogic
{
public:
	using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;

	HiredisPtr& GetZoneRedis()
	{
		return zoneRedis;
	}

	HiredisPtr zoneRedis;
};

extern thread_local ThreadLocalStorageCommonLogic tlsCommonLogic;


