#pragma once

#include "config.h"

class NodeConfigManager {

public:
	NodeConfigManager() = default;

	[[nodiscard]] ::BaseDeployConfig& GetBaseDeployConfig()
	{
		return baseDeployConfig_;
	}

	[[nodiscard]] ::GameConfig& GetGameConfig()
	{
		return gameConfig_;
	}

private:
	::BaseDeployConfig baseDeployConfig_;
	::GameConfig gameConfig_;
};

extern thread_local NodeConfigManager tlsNodeConfigManager;
extern NodeConfigManager gNodeConfigManager;

