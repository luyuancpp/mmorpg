#pragma once

#include "config_loader/config.h"
#include "proto/common/message.pb.h"
#include "type_define/type_define.h"
#include <muduo/contrib/hiredis/hiredis.h>

class ThreadLocalStorageCommonLogic
{
public:
	using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;

    [[nodiscard]] ::BaseDeployConfig& GetBaseDeployConfig()
    {
        return BaseDeployConfig;
    }

    void SetBaseDeployConfig(const ::BaseDeployConfig& baseDeployConfig)
    {
        BaseDeployConfig = baseDeployConfig;
    }

    [[nodiscard]] GameConfig& GetGameConfig()
    {
        return GameConfig;
    }

    void SetGameConfig(const GameConfig& gameConfig)
    {
        GameConfig = gameConfig;
    }

	HiredisPtr& GetZoneRedis()
	{
		return zoneRedis;
	}
private:
    BaseDeployConfig BaseDeployConfig;
    GameConfig GameConfig;
	HiredisPtr zoneRedis;
};

extern thread_local ThreadLocalStorageCommonLogic tlsCommonLogic;


