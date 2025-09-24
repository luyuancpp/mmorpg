#pragma once

#include "config.h"
#include "proto/common/message.pb.h"

class NodeConfigManager {

public:
	NodeConfigManager() = default;

	[[nodiscard]] ::BaseDeployConfig& GetBaseDeployConfig()
	{
		return BaseDeployConfig;
	}

	[[nodiscard]] GameConfig& GetGameConfig()
	{
		return GameConfig;
	}

private:
	BaseDeployConfig BaseDeployConfig;
	GameConfig GameConfig;
};

extern thread_local NodeConfigManager tlsNodeConfigManager;
extern NodeConfigManager gNodeConfigManager;

